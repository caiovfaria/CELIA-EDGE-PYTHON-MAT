"""C.E.L.I.A. · Programa de Logica (Computational Thinking with Python) [HU-02, RF16, RN01]

Le as leituras do poste01 no canal publico do ThingSpeak (3577155), calcula o indice de
risco de cada leitura e mostra a faixa que o cidadao ve no app: Acessivel, Atencao ou Evite.

Medidas usadas (2 campos do canal):
    field3: indice de calor (graus C), calculado no ESP32 a partir da temperatura e umidade
    field7: luminosidade (lux), medida pelo LDR

Desde 18/11/2026 o poste mede luz (LDR) no lugar da distancia (HC-SR04). O buraco na
calcada e visto pela camera com o modelo de IA (field6 = 5), nao por estes sensores.

Os limites NAO foram inventados aqui: sao os mesmos do firmware do poste
(edge/celia_logica.h), para o Python, o ESP32 e o app falarem a mesma lingua (RN01).
Se a equipe mudar um limite, mude la e aqui (CONFIG).

Como rodar (na pasta python/):
    pip install requests
    python main.py
"""

import csv
import json
import os
import sys

import requests

PASTA = os.path.dirname(os.path.abspath(__file__))

CONFIG = {
    "url": "https://api.thingspeak.com/channels/3577155/feeds.json",
    "quantidade": 100,  # pede ate 100; o minimo pedido pela disciplina e 20
    "minimo_leituras": 20,
    "timeout_s": 10,
    "arquivo_backup": os.path.join(PASTA, "backup.json"),
    "arquivo_historico": os.path.join(PASTA, "historico.json"),
    "arquivo_relatorio": os.path.join(PASTA, "relatorio.csv"),
    # Cada medida: campo do canal, faixa de "min" a "max" (min sempre menor que max) e peso.
    # Calor: 32 C e onde a Atencao comeca no ESP32; 41 C e onde comeca o Evite.
    # Luz: "invertida" (quanto MENOS luz, maior o risco). Abaixo de 10 lux o ESP32 considera a
    # rua escura (nao da para ver buraco, degrau ou desnivel): 0 lux -> nivel 100; 10 lux ou
    # mais -> nivel 0. O sol sozinho nao piora o trecho (igual ao ESP32).
    "medidas": {
        "calor": {"campo": "field3", "min": 32.0, "max": 41.0, "peso": 0.5, "unidade": "C",
                  "invertida": False},
        "luz": {"campo": "field7", "min": 0.0, "max": 10.0, "peso": 0.5, "unidade": "lux",
                "invertida": True},
    },
    # Regra do sol do ESP32 (edge/celia_logica.h): sol direto (25.000 lux ou mais) junto com
    # calor (indice de 32 C ou mais) deixa o trecho em Evite. A formula linear nao enxerga o
    # sol (ela so mede o escuro), entao esta regra vem depois dela.
    "regra_sol": {"lux_minimo": 25000.0, "calor_minimo": 32.0},
    # Faixas do risco (0 a 100). Com pesos 0,5 e 0,5, uma medida sozinha no limite do Evite
    # do ESP32 da risco 50: por isso Evite comeca em 50. Qualquer risco acima de 0 e Atencao.
    "corte_atencao": 0.0,
    "corte_evite": 50.0,
    # Aviso ao cidadao em cada faixa: os mesmos textos que o app mostra (src/domain/faixas.js).
    "avisos": {
        "normal": "Calcada livre e sem calor forte. Boa para cadeira de rodas, andador e bengala.",
        "atencao": "Da para passar, mas com cuidado. Veja o motivo antes de sair.",
        "perigo": "Trecho dificil agora. Prefira outro caminho.",
    },
    # Valores fora disso sao lixo de teste ou falha do sensor (o canal tem registros assim).
    "faixa_valida": {"calor": (-10.0, 60.0), "luz": (0.0, 100000.0)},
}

# Mesmos nomes, icones e cores do app (src/domain/faixas.js e tokens.css do front).
FAIXAS = {
    "normal": {"nome": "Acessível", "icone": "✓", "cor": "#1E8E3E", "ansi": "\033[32m"},
    "atencao": {"nome": "Atenção", "icone": "!", "cor": "#B26A00", "ansi": "\033[33m"},
    "perigo": {"nome": "Evite", "icone": "✕", "cor": "#C62828", "ansi": "\033[31m"},
}
FIM_COR = "\033[0m"


def validar_config(config: dict) -> None:
    """Confere a configuracao. Dispara ValueError se algo estiver errado.

    Regras: pesos somam 1, min menor que max, corte do Evite maior que o da Atencao
    e um aviso para cada faixa.
    """
    medidas = config["medidas"]
    if len(medidas) < 2:
        raise ValueError("Use pelo menos 2 medidas.")
    soma = sum(m["peso"] for m in medidas.values())
    if abs(soma - 1.0) > 1e-9:
        raise ValueError(f"Os pesos precisam somar 1 (somam {soma}).")
    for nome, m in medidas.items():
        if m["peso"] < 0:
            raise ValueError(f"Peso negativo em {nome}.")
        if m["min"] >= m["max"]:
            raise ValueError(f"Em {nome}, o minimo precisa ser menor que o maximo.")
    if not 0 <= config["corte_atencao"] < config["corte_evite"] <= 100:
        raise ValueError("Cortes invalidos: precisa 0 <= atencao < evite <= 100.")
    for faixa in FAIXAS:
        if not config["avisos"].get(faixa):
            raise ValueError(f"Falta o aviso da faixa {faixa}.")


def calcular_nivel(valor: float, minimo: float, maximo: float, invertida: bool = False) -> float:
    """Nivel de 0 a 100: nivel = (valor - min) / (max - min) * 100, limitado a 0 e 100.

    invertida=True (luz): menos luz e mais risco, entao o nivel vira 100 - nivel.
    Ex.: luz com 0 lux -> nivel 100; com 5 lux -> nivel 50; com 10 lux ou mais -> nivel 0.
    """
    nivel = (valor - minimo) / (maximo - minimo) * 100
    nivel = max(0.0, min(100.0, nivel))
    return 100.0 - nivel if invertida else nivel


def calcular_risco(medidas: dict, config: dict) -> float:
    """Risco = peso1 * nivel1 + peso2 * nivel2 (de 0 a 100)."""
    risco = 0.0
    for nome, regra in config["medidas"].items():
        nivel = calcular_nivel(medidas[nome], regra["min"], regra["max"], regra["invertida"])
        risco += regra["peso"] * nivel
    return round(risco, 1)


def classificar(risco: float, config: dict) -> str:
    """Devolve o id da faixa: "normal", "atencao" ou "perigo"."""
    if risco >= config["corte_evite"]:
        return "perigo"
    if risco > config["corte_atencao"]:
        return "atencao"
    return "normal"


def aplicar_regra_sol(faixa: str, medidas: dict, config: dict) -> str:
    """Sol direto com calor vira Evite, como no LED do poste. Nos outros casos, mantem a faixa.

    Ex.: 50.000 lux e 33 C -> risco 5,6 (Atencao pela formula), mas o poste mostra Evite.
    """
    regra = config["regra_sol"]
    if medidas["luz"] >= regra["lux_minimo"] and medidas["calor"] >= regra["calor_minimo"]:
        return "perigo"
    return faixa


def buscar_thingspeak(config: dict) -> list[dict]:
    """Busca as leituras no ThingSpeak e devolve a lista "feeds" do canal.

    Dispara requests.RequestException se a internet ou o ThingSpeak falharem, inclusive
    quando a resposta nao tem o formato esperado (ex.: "-1" de canal privado ou inexistente).
    """
    resposta = requests.get(
        config["url"], params={"results": config["quantidade"]}, timeout=config["timeout_s"]
    )
    if resposta.status_code != 200:
        raise requests.RequestException(f"ThingSpeak respondeu com status {resposta.status_code}")
    dados = resposta.json()
    if not isinstance(dados, dict) or not isinstance(dados.get("feeds"), list):
        raise requests.RequestException("ThingSpeak respondeu num formato inesperado")
    return dados["feeds"]


def salvar_backup(feeds: list[dict], config: dict) -> None:
    """Guarda uma copia das leituras em backup.json (plano B). Se nao der, so avisa."""
    try:
        with open(config["arquivo_backup"], "w", encoding="utf-8") as arquivo:
            json.dump(feeds, arquivo, ensure_ascii=False, indent=2)
    except OSError as erro:
        # Os dados vieram; so o plano B nao foi atualizado desta vez
        print(f"Aviso: nao deu para atualizar o backup.json ({erro}).")


def ler_backup(config: dict) -> list[dict]:
    """Le as leituras guardadas em backup.json.

    Devolve lista vazia se o arquivo nao existir, nao abrir ou estiver corrompido.
    """
    if not os.path.exists(config["arquivo_backup"]):
        print("Nao existe backup.json ainda: busque com internet pelo menos uma vez.")
        return []
    try:
        with open(config["arquivo_backup"], encoding="utf-8") as arquivo:
            feeds = json.load(arquivo)
    except (OSError, ValueError) as erro:  # ValueError cobre JSON quebrado e texto nao UTF-8
        print(f"O backup.json nao pode ser lido ({erro}).")
        return []
    if not isinstance(feeds, list):
        print("O backup.json nao tem uma lista de leituras.")
        return []
    return feeds


def converter_numero(texto: str | None) -> float | None:
    """Converte o texto do ThingSpeak em numero. Campo vazio ou invalido vira None."""
    try:
        return float(texto)
    except (TypeError, ValueError):
        return None


def registro_valido(feed: object) -> bool:
    """True se o registro tem entry_id inteiro e created_at em texto (backup editado a mao pode nao ter)."""
    return (isinstance(feed, dict)
            and isinstance(feed.get("entry_id"), int)
            and isinstance(feed.get("created_at"), str))


def extrair_medidas(feed: dict, config: dict) -> dict | None:
    """Le as medidas do registro. Devolve None se algum campo estiver vazio ou absurdo."""
    medidas = {}
    for nome, regra in config["medidas"].items():
        valor = converter_numero(feed.get(regra["campo"]))
        minimo, maximo = config["faixa_valida"][nome]
        if valor is None or not minimo <= valor <= maximo:
            return None
        medidas[nome] = valor
    return medidas


def montar_leituras(feeds: list[dict], config: dict) -> list[dict]:
    """Transforma os registros do canal em leituras {id, horario, medidas, risco, faixa}.

    Pula registros estranhos, com campo vazio (sensor falhou) ou valor absurdo (teste antigo).
    """
    leituras = []
    for feed in feeds:
        if not registro_valido(feed):
            continue
        medidas = extrair_medidas(feed, config)
        if medidas is None:
            continue
        risco = calcular_risco(medidas, config)
        leituras.append({
            "id": feed["entry_id"],
            "horario": feed["created_at"],
            "medidas": medidas,
            "risco": risco,
            "faixa": aplicar_regra_sol(classificar(risco, config), medidas, config),
        })
    return leituras


def carregar_leituras(config: dict) -> tuple[list[dict], str]:
    """Tenta o ThingSpeak; se falhar, usa o backup.json. Devolve (leituras, origem)."""
    try:
        feeds = buscar_thingspeak(config)
        salvar_backup(feeds, config)
        origem = "ThingSpeak (ao vivo)"
    except requests.RequestException as erro:
        print(f"Nao deu para falar com o ThingSpeak ({erro}). Usando o backup.json.")
        feeds = ler_backup(config)
        origem = "backup.json (plano B)"
    return montar_leituras(feeds, config), origem


def texto_faixa(faixa: str) -> str:
    """Faixa com icone e cor no terminal. Ex.: "✕ Evite" em vermelho."""
    f = FAIXAS[faixa]
    return f"{f['ansi']}{f['icone']} {f['nome']}{FIM_COR}"


def mostrar_leituras(leituras: list[dict]) -> None:
    """Imprime uma tabela com as leituras, o risco e a faixa."""
    print(f"\n{'ID':>4}  {'Horario (UTC)':<20} {'Calor':>7} {'Luz':>10} {'Risco':>6}  Faixa")
    for leitura in leituras:
        m = leitura["medidas"]
        print(f"{leitura['id']:>4}  {leitura['horario']:<20} {m['calor']:>5.1f} C "
              f"{m['luz']:>6.0f} lux {leitura['risco']:>6.1f}  {texto_faixa(leitura['faixa'])}")


def contar_faixas(leituras: list[dict]) -> dict:
    """Conta quantas leituras caem em cada faixa."""
    contagem = {faixa: 0 for faixa in FAIXAS}
    for leitura in leituras:
        contagem[leitura["faixa"]] += 1
    return contagem


def calcular_tendencia(leituras: list[dict]) -> str:
    """PIORANDO se o risco subiu nas 3 ultimas leituras seguidas; senao ESTAVEL."""
    if len(leituras) < 4:
        return "ESTAVEL (poucas leituras)"
    ultimos = [leitura["risco"] for leitura in leituras[-4:]]
    altas = sum(1 for i in range(3) if ultimos[i + 1] > ultimos[i])
    return "PIORANDO" if altas == 3 else "ESTAVEL"


def maior_episodio_perigo(leituras: list[dict]) -> tuple[int, str, str]:
    """Maior sequencia seguida de leituras em Evite: (quantidade, inicio, fim)."""
    maior, inicio_maior, fim_maior = 0, "", ""
    atual, inicio = 0, ""
    for leitura in leituras:
        if leitura["faixa"] == "perigo":
            if atual == 0:
                inicio = leitura["horario"]
            atual += 1
            if atual > maior:
                maior, inicio_maior, fim_maior = atual, inicio, leitura["horario"]
        else:
            atual = 0
    return maior, inicio_maior, fim_maior


def mostrar_resumo(leituras: list[dict], config: dict) -> None:
    """Imprime contagem por faixa, tendencia, faixa atual com aviso e maior episodio em Evite."""
    print("\nLeituras por faixa:")
    for faixa, total in contar_faixas(leituras).items():
        print(f"  {texto_faixa(faixa)}: {total}")
    print(f"Tendencia: {calcular_tendencia(leituras)}")
    ultima = leituras[-1]
    print(f"Agora ({ultima['horario']}): {texto_faixa(ultima['faixa'])}. "
          f"AVISO: {config['avisos'][ultima['faixa']]}")
    quantidade, inicio, fim = maior_episodio_perigo(leituras)
    if quantidade:
        print(f"Maior episodio em Evite: {quantidade} leituras seguidas, de {inicio} a {fim}")
    else:
        print("Nenhuma leitura em Evite.")


def guardar_corrompido(caminho: str) -> None:
    """Renomeia um arquivo corrompido para <nome>_corrompido.json, para nao perder o conteudo."""
    copia = caminho.replace(".json", "_corrompido.json")
    os.replace(caminho, copia)
    print(f"O {os.path.basename(caminho)} estava corrompido: guardei uma copia em "
          f"{os.path.basename(copia)} e comecei um novo.")


def ler_historico(config: dict) -> list[dict]:
    """Le o historico.json. Se estiver corrompido, guarda uma copia e comeca um novo."""
    caminho = config["arquivo_historico"]
    if not os.path.exists(caminho):
        return []
    try:
        with open(caminho, encoding="utf-8") as arquivo:
            historico = json.load(arquivo)
        if isinstance(historico, list) and all(
                isinstance(l, dict) and isinstance(l.get("id"), int) for l in historico):
            return historico
    except ValueError:  # JSON quebrado ou texto nao UTF-8
        pass
    guardar_corrompido(caminho)
    return []


def salvar_historico(leituras: list[dict], config: dict) -> int:
    """Junta as leituras novas ao historico.json sem repetir (entry_id num set). Devolve quantas entraram."""
    historico = ler_historico(config)
    ids_salvos = {leitura["id"] for leitura in historico}
    novas = [leitura for leitura in leituras if leitura["id"] not in ids_salvos]
    historico.extend(novas)
    historico.sort(key=lambda leitura: leitura["id"])
    with open(config["arquivo_historico"], "w", encoding="utf-8") as arquivo:
        json.dump(historico, arquivo, ensure_ascii=False, indent=2)
    return len(novas)


def salvar_relatorio(leituras: list[dict], config: dict) -> None:
    """Grava o relatorio.csv com uma linha por leitura (abre no Excel)."""
    with open(config["arquivo_relatorio"], "w", newline="", encoding="utf-8-sig") as arquivo:
        escritor = csv.writer(arquivo, delimiter=";")
        escritor.writerow(["id", "horario", "indice_calor_c", "luminosidade_lux",
                           "risco", "faixa", "cor"])
        for leitura in leituras:
            f = FAIXAS[leitura["faixa"]]
            escritor.writerow([leitura["id"], leitura["horario"], leitura["medidas"]["calor"],
                               leitura["medidas"]["luz"], leitura["risco"], f["nome"], f["cor"]])


def mostrar_menu() -> str:
    """Mostra o menu e devolve a opcao digitada."""
    print("\n=== C.E.L.I.A. · Poste 01 ===")
    print("1. Buscar leituras do poste")
    print("2. Ver leituras e faixas")
    print("3. Ver resumo (faixas, tendencia, maior episodio em Evite)")
    print("4. Salvar historico.json e relatorio.csv")
    print("0. Sair")
    return input("Escolha: ").strip()


def opcao_buscar(config: dict) -> list[dict]:
    """Opcao 1: carrega as leituras e avisa se vieram menos que o minimo pedido."""
    leituras, origem = carregar_leituras(config)
    print(f"{len(leituras)} leituras validas carregadas de {origem}.")
    if len(leituras) < config["minimo_leituras"]:
        print(f"Atencao: menos de {config['minimo_leituras']} leituras validas no canal.")
    return leituras


def opcao_salvar(leituras: list[dict], config: dict) -> None:
    """Opcao 4: grava historico.json e relatorio.csv. Erro de arquivo vira aviso."""
    try:
        novas = salvar_historico(leituras, config)
        salvar_relatorio(leituras, config)
        print(f"historico.json: {novas} leituras novas. relatorio.csv atualizado.")
    except OSError as erro:
        # Ex.: relatorio.csv aberto no Excel (o Windows bloqueia o arquivo)
        print(f"Nao deu para salvar ({erro}). Feche o arquivo e tente de novo.")


def rodar_menu(config: dict) -> None:
    """Menu em loop: le a opcao, executa e repete ate a pessoa escolher Sair (0)."""
    leituras: list[dict] = []
    while True:
        opcao = mostrar_menu()
        if opcao == "1":
            leituras = opcao_buscar(config)
        elif opcao in ("2", "3", "4") and not leituras:
            print("Primeiro busque as leituras (opcao 1).")
        elif opcao == "2":
            mostrar_leituras(leituras)
        elif opcao == "3":
            mostrar_resumo(leituras, config)
        elif opcao == "4":
            opcao_salvar(leituras, config)
        elif opcao == "0":
            print("Ate logo!")
            return
        else:
            print("Opcao invalida. Digite 0, 1, 2, 3 ou 4.")


def main() -> None:
    """Prepara o terminal, valida a configuracao e roda o menu. Ctrl+C sai sem erro."""
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")  # icones ✓ ! ✕ no terminal do Windows
    os.system("")  # liga as cores ANSI no terminal do Windows
    validar_config(CONFIG)
    try:
        rodar_menu(CONFIG)
    except (KeyboardInterrupt, EOFError):
        # Ctrl+C (no menu ou durante a busca) ou fim da entrada: sai sem mostrar erro
        print("\nAte logo!")


if __name__ == "__main__":
    main()
