"""C.E.L.I.A. · Programa de Logica (Computational Thinking with Python) [HU-02, RF16, RN01]

Le as leituras do poste01 no canal publico do ThingSpeak (3577155), calcula o indice de
risco de cada leitura e mostra a faixa que o cidadao ve no app: Acessivel, Atencao ou Evite.

Medidas usadas (2 campos do canal):
    field3: indice de calor (graus C), calculado no ESP32 a partir da temperatura e umidade
    field4: distancia livre na calcada (cm), medida pelo HC-SR04

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
    # Cada medida: campo do canal, nivel 0 em "min" e nivel 100 em "max".
    # Calor: 32 C e onde a Atencao comeca no ESP32; 41 C e onde comeca o Evite.
    # Calcada: invertida (quanto MENOR a distancia, maior o risco). 120 cm e calcada livre
    # (NBR 9050, faixa livre de 1,20 m); abaixo de 90 cm o ESP32 considera bloqueada.
    "medidas": {
        "calor": {"campo": "field3", "min": 32.0, "max": 41.0, "peso": 0.5, "unidade": "C"},
        "calcada": {"campo": "field4", "min": 120.0, "max": 90.0, "peso": 0.5, "unidade": "cm"},
    },
    # Faixas do risco (0 a 100). Com pesos 0,5 e 0,5, uma medida sozinha no limite do Evite
    # do ESP32 da risco 50: por isso Evite comeca em 50. Qualquer risco acima de 0 e Atencao.
    "corte_atencao": 0.0,
    "corte_evite": 50.0,
    # Valores fora disso sao lixo de teste ou falha do sensor (o canal tem registros assim).
    "faixa_valida": {"calor": (-10.0, 60.0), "calcada": (0.0, 400.0)},
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

    Regras: pesos somam 1, min diferente de max e corte do Evite maior que o da Atencao.
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
        if m["min"] == m["max"]:
            raise ValueError(f"Em {nome}, min e max nao podem ser iguais.")
    if not 0 <= config["corte_atencao"] < config["corte_evite"] <= 100:
        raise ValueError("Cortes invalidos: precisa 0 <= atencao < evite <= 100.")


def calcular_nivel(valor: float, minimo: float, maximo: float) -> float:
    """Nivel de 0 a 100: nivel = (valor - min) / (max - min) * 100, limitado a 0 e 100.

    Com max menor que min (calcada), a conta ja inverte sozinha: menos cm = nivel maior.
    """
    nivel = (valor - minimo) / (maximo - minimo) * 100
    return max(0.0, min(100.0, nivel))


def calcular_risco(medidas: dict, config: dict) -> float:
    """Risco = peso1 * nivel1 + peso2 * nivel2 (de 0 a 100)."""
    risco = 0.0
    for nome, regra in config["medidas"].items():
        risco += regra["peso"] * calcular_nivel(medidas[nome], regra["min"], regra["max"])
    return round(risco, 1)


def classificar(risco: float, config: dict) -> str:
    """Devolve o id da faixa: "normal", "atencao" ou "perigo"."""
    if risco >= config["corte_evite"]:
        return "perigo"
    if risco > config["corte_atencao"]:
        return "atencao"
    return "normal"


def buscar_thingspeak(config: dict) -> list[dict]:
    """Busca as leituras no ThingSpeak e salva uma copia em backup.json (plano B).

    Dispara requests.RequestException se a internet ou o ThingSpeak falharem.
    """
    resposta = requests.get(
        config["url"], params={"results": config["quantidade"]}, timeout=config["timeout_s"]
    )
    if resposta.status_code != 200:
        raise requests.RequestException(f"ThingSpeak respondeu com status {resposta.status_code}")
    feeds = resposta.json().get("feeds", [])
    with open(config["arquivo_backup"], "w", encoding="utf-8") as arquivo:
        json.dump(feeds, arquivo, ensure_ascii=False, indent=2)
    return feeds


def ler_backup(config: dict) -> list[dict]:
    """Le as leituras guardadas em backup.json. Devolve lista vazia se nao existir."""
    if not os.path.exists(config["arquivo_backup"]):
        return []
    with open(config["arquivo_backup"], encoding="utf-8") as arquivo:
        return json.load(arquivo)


def converter_numero(texto) -> float | None:
    """Converte o texto do ThingSpeak em numero. Campo vazio ou invalido vira None."""
    try:
        return float(texto)
    except (TypeError, ValueError):
        return None


def montar_leituras(feeds: list[dict], config: dict) -> list[dict]:
    """Transforma os registros do canal em leituras {id, horario, medidas, faixa}.

    Pula registros com campo vazio (sensor falhou) ou valor absurdo (teste antigo).
    """
    leituras = []
    for feed in feeds:
        medidas = {}
        for nome, regra in config["medidas"].items():
            valor = converter_numero(feed.get(regra["campo"]))
            minimo, maximo = config["faixa_valida"][nome]
            if valor is None or not minimo <= valor <= maximo:
                break
            medidas[nome] = valor
        else:  # so entra aqui se nenhuma medida foi pulada
            risco = calcular_risco(medidas, config)
            leituras.append({
                "id": feed["entry_id"],
                "horario": feed["created_at"],
                "medidas": medidas,
                "risco": risco,
                "faixa": classificar(risco, config),
            })
    return leituras


def carregar_leituras(config: dict) -> tuple[list[dict], str]:
    """Tenta o ThingSpeak; se falhar, usa o backup.json. Devolve (leituras, origem)."""
    try:
        feeds = buscar_thingspeak(config)
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
    print(f"\n{'ID':>4}  {'Horario (UTC)':<20} {'Calor':>7} {'Calcada':>8} {'Risco':>6}  Faixa")
    for leitura in leituras:
        m = leitura["medidas"]
        print(f"{leitura['id']:>4}  {leitura['horario']:<20} {m['calor']:>5.1f} C "
              f"{m['calcada']:>5.0f} cm {leitura['risco']:>6.1f}  {texto_faixa(leitura['faixa'])}")


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


def mostrar_resumo(leituras: list[dict]) -> None:
    """Imprime contagem por faixa, tendencia e maior episodio em Evite."""
    print("\nLeituras por faixa:")
    for faixa, total in contar_faixas(leituras).items():
        print(f"  {texto_faixa(faixa)}: {total}")
    print(f"Tendencia: {calcular_tendencia(leituras)}")
    quantidade, inicio, fim = maior_episodio_perigo(leituras)
    if quantidade:
        print(f"Maior episodio em Evite: {quantidade} leituras seguidas, de {inicio} a {fim}")
    else:
        print("Nenhuma leitura em Evite.")


def salvar_historico(leituras: list[dict], config: dict) -> int:
    """Junta as leituras novas ao historico.json sem repetir (entry_id num set). Devolve quantas entraram."""
    historico = []
    if os.path.exists(config["arquivo_historico"]):
        with open(config["arquivo_historico"], encoding="utf-8") as arquivo:
            historico = json.load(arquivo)
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
        escritor.writerow(["id", "horario", "indice_calor_c", "distancia_livre_cm",
                           "risco", "faixa", "cor"])
        for leitura in leituras:
            f = FAIXAS[leitura["faixa"]]
            escritor.writerow([leitura["id"], leitura["horario"], leitura["medidas"]["calor"],
                               leitura["medidas"]["calcada"], leitura["risco"], f["nome"], f["cor"]])


def mostrar_menu() -> str:
    """Mostra o menu e devolve a opcao digitada."""
    print("\n=== C.E.L.I.A. · Poste 01 ===")
    print("1. Buscar leituras do poste")
    print("2. Ver leituras e faixas")
    print("3. Ver resumo (faixas, tendencia, maior episodio em Evite)")
    print("4. Salvar historico.json e relatorio.csv")
    print("0. Sair")
    return input("Escolha: ").strip()


def main() -> None:
    """Valida a configuracao e roda o menu em loop ate a pessoa escolher Sair."""
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")  # icones ✓ ! ✕ no terminal do Windows
    os.system("")  # liga as cores ANSI no terminal do Windows
    validar_config(CONFIG)

    leituras: list[dict] = []
    while True:
        opcao = mostrar_menu()
        if opcao == "1":
            leituras, origem = carregar_leituras(CONFIG)
            print(f"{len(leituras)} leituras validas carregadas de {origem}.")
            if len(leituras) < CONFIG["minimo_leituras"]:
                print(f"Atencao: menos de {CONFIG['minimo_leituras']} leituras validas no canal.")
        elif opcao in ("2", "3", "4") and not leituras:
            print("Primeiro busque as leituras (opcao 1).")
        elif opcao == "2":
            mostrar_leituras(leituras)
        elif opcao == "3":
            mostrar_resumo(leituras)
        elif opcao == "4":
            novas = salvar_historico(leituras, CONFIG)
            salvar_relatorio(leituras, CONFIG)
            print(f"historico.json: {novas} leituras novas. relatorio.csv atualizado.")
        elif opcao == "0":
            print("Ate logo!")
            break
        else:
            print("Opcao invalida. Digite 0, 1, 2, 3 ou 4.")


if __name__ == "__main__":
    main()
