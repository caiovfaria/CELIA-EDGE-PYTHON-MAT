"""Rotas da camera do poste (ESP32-CAM) [HU-16, HU-23, HU-24, HU-25, RF09, RF10, RF13, RF22, RF23, RF24, RNF11].

Fluxo:
    ESP32-CAM --(POST /poste/captura, corpo = foto JPEG, cabecalho X-Chave)--> API
    1. confere a chave e se e um JPEG de verdade
    2. salva o arquivo em capturas/ e grava uma linha no banco (banco.py)
    3. o modelo de buracos analisa a foto (o mesmo YOLO do /analisar)
    4. avisa o ESP32 do Wokwi por MQTT (buraco deixa o trecho vermelho, motivo 5) [RN02]
    5. se for buraco, o Gemini escreve o report em segundo plano e ele vai para a tabela
       reportes, sem revisao humana [RN08], no maximo um a cada 10 min por poste [RN09]

Rotas que o site le (area da Prefeitura) [HU-25, RF24]:
    GET /poste/capturas               ultimas capturas do poste, com resultado e caixas
    GET /poste/capturas/{id}/foto     o JPEG da captura
    GET /poste/reportes               reports automaticos, do mais novo ao mais velho

A analise e o MQTT moram no main.py; ele "liga" as funcoes aqui com configurar(),
para este arquivo nao precisar importar o main.py (isso faria um import circular).

Decisao de 15/11/2026: as fotos da camera do poste SAO guardadas (RNF01 atualizado);
a foto do cidadao continua sem passar por aqui.
"""
import io
import json
import os
import re
import secrets
import threading
import uuid
from datetime import datetime, timedelta, timezone

from fastapi import APIRouter, BackgroundTasks, Header, HTTPException, Query, Request
from fastapi.responses import FileResponse
from fastapi.concurrency import run_in_threadpool
from PIL import Image, UnidentifiedImageError

import banco
from assistente import PedidoDescricao, sugerir_descricao

PASTA_CAPTURAS = os.path.join(os.path.dirname(__file__), "capturas")

CONFIG_POSTE = {
    # Chave que o ESP32-CAM manda no cabecalho X-Chave [RNF11]. Fica so no .env.
    # Sem chave configurada, a API recusa toda foto (melhor do que aceitar de qualquer um).
    "chave": os.getenv("POSTE_CHAVE", ""),
    # Uma foto da OV2640 em VGA tem uns 30 a 80 KB; 1 MB ja e muita folga.
    "tamanho_max_bytes": 1 * 1024 * 1024,
    # PROPOSTA (RN09, a validar): no maximo um report automatico a cada 10 min por poste.
    # A camera manda foto a cada 20 s: sem isso, o mesmo buraco viraria 30 reports por
    # minuto e estouraria a cota gratis do Gemini.
    "intervalo_reports": timedelta(minutes=10),
    # Onde fica cada poste, para o texto do report. DADO DE EXEMPLO (o mesmo do app):
    # os pontos reais serao definidos com a prefeitura [RN04].
    "locais": {
        # Com acento: o Gemini copia o jeito que o local esta escrito para o texto do report
        "poste01": "Rua Barão do Bom Retiro, ponto de ônibus, Grajaú, Rio de Janeiro",
    },
}

# Nome do poste: so letras minusculas, numeros, - e _ (vira nome de pasta no disco).
FORMATO_POSTE = re.compile(r"^[a-z0-9_-]{1,32}$")

router = APIRouter(prefix="/poste", tags=["Camera do poste"])

# Funcoes que o main.py entrega em configurar() (analise com o YOLO, MQTT, modelo pronto)
ligacoes = {}

# Quando cada poste pediu o ultimo report (na memoria). Guardado NA HORA DO PEDIDO, e nao
# quando o Gemini responde: duas fotos com buraco seguidas nao geram dois pedidos.
ultimo_pedido_report = {}
trava_reports = threading.Lock()


def configurar(analisar, publicar, modelo_pronto) -> None:
    """Recebe do main.py as funcoes de analise, de MQTT e de "modelo pronto"."""
    ligacoes["analisar"] = analisar
    ligacoes["publicar"] = publicar
    ligacoes["modelo_pronto"] = modelo_pronto


def conferir_chave(chave_recebida: str | None) -> None:
    """Recusa a foto se a chave estiver errada ou se a API estiver sem chave configurada.

    secrets.compare_digest compara em tempo constante: quem tenta adivinhar a chave
    nao descobre, pelo tempo da resposta, quantas letras acertou.
    """
    if not CONFIG_POSTE["chave"]:
        raise HTTPException(503, "API sem POSTE_CHAVE no .env: a camera do poste esta desligada.")
    if not chave_recebida or not secrets.compare_digest(chave_recebida, CONFIG_POSTE["chave"]):
        raise HTTPException(401, "Chave do poste invalida.")


async def ler_corpo_limitado(request: Request) -> bytes:
    """Le o corpo da requisicao em pedacos e para assim que passar do limite.

    Assim um envio gigante e recusado sem a API carregar tudo na memoria.
    """
    dados = bytearray()
    async for pedaco in request.stream():
        dados.extend(pedaco)
        if len(dados) > CONFIG_POSTE["tamanho_max_bytes"]:
            raise HTTPException(413, "Foto muito grande. Limite de 1 MB.")
    if not dados:
        raise HTTPException(400, "Nenhuma foto no corpo da requisicao.")
    return bytes(dados)


def conferir_jpeg(dados: bytes) -> Image.Image:
    """Confere se os bytes sao mesmo uma foto JPEG e devolve a imagem aberta."""
    try:
        imagem = Image.open(io.BytesIO(dados))
        imagem.load()
    except (UnidentifiedImageError, Image.DecompressionBombError, OSError):
        raise HTTPException(400, "O corpo enviado nao e uma imagem valida.")
    if imagem.format != "JPEG":
        raise HTTPException(415, "A camera do poste deve mandar a foto em JPEG.")
    return imagem


def salvar_arquivo(poste: str, dados: bytes, agora: datetime) -> str:
    """Salva o JPEG em capturas/<poste>/ e devolve o caminho relativo (o que vai para o banco).

    Nome do arquivo = data e hora + um pedaco aleatorio, para duas fotos no mesmo
    segundo nao se sobrescreverem. Ex.: poste01/20261115-142530-a1b2c3d4.jpg
    """
    pasta = os.path.join(PASTA_CAPTURAS, poste)
    os.makedirs(pasta, exist_ok=True)
    nome = f"{agora.strftime('%Y%m%d-%H%M%S')}-{uuid.uuid4().hex[:8]}.jpg"
    with open(os.path.join(pasta, nome), "wb") as arquivo:
        arquivo.write(dados)
    return f"{poste}/{nome}"


def pode_pedir_report(poste: str, agora: datetime) -> bool:
    """Diz se ja passou o intervalo do RN09 desde o ultimo report do poste. Se sim, marca o pedido.

    Na primeira vez depois de ligar a API, olha no banco (o ultimo report pode ser de antes).
    """
    with trava_reports:
        ultimo = ultimo_pedido_report.get(poste)
        if ultimo is None:
            texto = banco.ultimo_reporte_em(poste)
            ultimo = datetime.fromisoformat(texto) if texto else None
        if ultimo is not None and agora - ultimo < CONFIG_POSTE["intervalo_reports"]:
            return False
        ultimo_pedido_report[poste] = agora
        return True


def gerar_reporte(captura_id: int, poste: str) -> None:
    """Pede o report ao Gemini e guarda na tabela reportes [HU-16, RF13, RN08].

    Roda em segundo plano, DEPOIS que a API ja respondeu ao ESP32 (o Gemini pode levar
    mais de 10 s). Se o Gemini falhar, libera o intervalo para a proxima foto tentar de novo.
    """
    local = CONFIG_POSTE["locais"].get(poste, "")
    try:
        resposta = sugerir_descricao(PedidoDescricao(resultado_ia="buraco", local=local))
    except Exception as erro:  # Gemini fora, sem cota, sem chave ou resposta estranha
        print(f"[poste] report da captura {captura_id} nao saiu: {erro}")
        with trava_reports:
            ultimo_pedido_report.pop(poste, None)
        return
    agora = datetime.now(timezone.utc).isoformat(timespec="seconds")
    reporte_id = banco.inserir_reporte(captura_id, poste, resposta["descricao_sugerida"], agora)
    print(f"[poste] report {reporte_id} gerado para a captura {captura_id}")


@router.post("/captura", status_code=201)
async def receber_captura(
    request: Request,
    tarefas: BackgroundTasks,
    x_chave: str | None = Header(default=None),
    x_poste: str = Header(default="poste01"),
) -> dict:
    """Recebe uma foto do ESP32-CAM, guarda, analisa e, se for buraco, pede o report.

    O ESP32 manda a foto JPEG pura no corpo (Content-Type: image/jpeg), mais simples no
    Arduino do que um formulario. Cabecalhos: X-Chave (obrigatorio) e X-Poste (padrao poste01).
    """
    conferir_chave(x_chave)
    if not FORMATO_POSTE.match(x_poste):
        raise HTTPException(400, "Nome de poste invalido (use letras minusculas, numeros, - e _).")

    dados = await ler_corpo_limitado(request)
    imagem = conferir_jpeg(dados)

    # 2. guarda o arquivo e a captura
    agora = datetime.now(timezone.utc)
    arquivo = salvar_arquivo(x_poste, dados, agora)
    try:
        captura_id = banco.inserir_captura(
            x_poste, agora.isoformat(timespec="seconds"), arquivo, imagem.width, imagem.height
        )
    except Exception:
        # Se o banco falhar, apaga o arquivo para nao ficar foto "orfa" no disco
        os.remove(os.path.join(PASTA_CAPTURAS, arquivo))
        raise

    resposta = {
        "id": captura_id,
        "poste": x_poste,
        "recebida_em": agora.isoformat(timespec="seconds"),
        "largura": imagem.width,
        "altura": imagem.height,
    }

    # Logo que a API liga, o modelo ainda esta aquecendo (pode passar de 1 min). Em vez de
    # deixar o ESP32 esperando (o tempo limite dele e de poucos segundos), guarda a foto sem
    # analise: a proxima, 20 s depois, ja e analisada.
    if not ligacoes["modelo_pronto"]():
        resposta.update(resultado=None, caixas=[], mqtt_enviado=False, report="pulado: modelo aquecendo")
        return resposta

    # 3. analise com o YOLO (em outra thread, para a API nao travar enquanto o modelo trabalha)
    resultado, caixas = await run_in_threadpool(ligacoes["analisar"], imagem.convert("RGB"))

    # 4. aviso ao ESP32 do Wokwi; 5. report em segundo plano
    mqtt_enviado = ligacoes["publicar"](resultado)
    banco.atualizar_analise(captura_id, resultado, json.dumps(caixas), mqtt_enviado)

    if resultado != "buraco":
        report = "nao se aplica: calcada livre"
    elif pode_pedir_report(x_poste, agora):
        tarefas.add_task(gerar_reporte, captura_id, x_poste)
        report = "gerando"
    else:
        report = "pulado: ja houve report deste poste nos ultimos 10 min (RN09)"

    resposta.update(resultado=resultado, caixas=caixas, mqtt_enviado=mqtt_enviado, report=report)
    return resposta


# ---------------------------------------------------------------------------
# Rotas que o site le (area da Prefeitura) [HU-25, RF24]. Sem chave: sao fotos da
# calcada publica e os reports, que a propria Prefeitura mostra.
# ---------------------------------------------------------------------------
def endereco_da_foto(captura_id: int) -> str:
    """Caminho que o site usa na tag <img>. Ex.: /poste/capturas/12/foto"""
    return f"/poste/capturas/{captura_id}/foto"


def captura_para_o_site(linha: dict) -> dict:
    """Arruma uma linha do banco para o site: caixas viram lista e entra o endereco da foto.

    O caminho do arquivo no disco NAO vai para o site (nao e da conta de quem esta fora).
    """
    return {
        "id": linha["id"],
        "poste": linha["poste"],
        "recebida_em": linha["recebida_em"],
        "resultado": linha["resultado"],
        "caixas": json.loads(linha["caixas"]) if linha["caixas"] else [],
        "mqtt_enviado": bool(linha["mqtt_enviado"]),
        "largura": linha["largura"],
        "altura": linha["altura"],
        "foto": endereco_da_foto(linha["id"]),
    }


@router.get("/capturas")
def listar_capturas(
    poste: str = Query(default="poste01"),
    limite: int = Query(default=10, ge=1, le=100),
) -> list[dict]:
    """Ultimas capturas de um poste, da mais nova para a mais velha [HU-25, RF24].

    resultado vazio (null) = a foto chegou com o modelo ainda aquecendo e nao foi analisada.
    """
    if not FORMATO_POSTE.match(poste):
        raise HTTPException(400, "Nome de poste invalido.")
    return [captura_para_o_site(linha) for linha in banco.listar_capturas(poste, limite)]


@router.get("/capturas/{captura_id}/foto")
def foto_da_captura(captura_id: int) -> FileResponse:
    """Devolve o JPEG de uma captura (o site usa direto numa tag <img>)."""
    captura = banco.buscar_captura(captura_id)
    if captura is None:
        raise HTTPException(404, "Captura nao encontrada.")
    # Confere que o arquivo esta mesmo dentro de capturas/ (protecao contra "../" no caminho)
    pasta = os.path.realpath(PASTA_CAPTURAS)
    caminho = os.path.realpath(os.path.join(pasta, captura["arquivo"]))
    if not caminho.startswith(pasta + os.sep) or not os.path.isfile(caminho):
        raise HTTPException(404, "Foto da captura nao encontrada.")
    return FileResponse(caminho, media_type="image/jpeg")


@router.get("/reportes")
def listar_reportes(limite: int = Query(default=20, ge=1, le=100)) -> list[dict]:
    """Reports automaticos dos postes, do mais novo ao mais velho [HU-16, HU-25, RF13, RF24].

    Cada report traz a foto e as caixas da captura que o originou, para o site mostrar
    o buraco marcado junto do texto.
    """
    return [
        {
            "id": linha["id"],
            "poste": linha["poste"],
            "local": CONFIG_POSTE["locais"].get(linha["poste"], ""),
            "texto": linha["texto"],
            "criado_em": linha["criado_em"],
            "captura": {
                "id": linha["captura_id"],
                "recebida_em": linha["recebida_em"],
                "caixas": json.loads(linha["caixas"]) if linha["caixas"] else [],
                "largura": linha["largura"],
                "altura": linha["altura"],
                "foto": endereco_da_foto(linha["captura_id"]),
            },
        }
        for linha in banco.listar_reportes(limite)
    ]
