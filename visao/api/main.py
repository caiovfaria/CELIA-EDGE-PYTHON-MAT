"""API de visao computacional da C.E.L.I.A. [HU-05, HU-07, HU-08, RF08, RF09, RF10, RNF01]

Recebe a foto de uma calcada, roda o modelo de buracos treinado pela equipe e responde:
- resultado: "livre" ou "buraco"
- caixas_buracos: para o app desenhar sobre a foto
- mqtt_enviado: se o resultado foi publicado para o ESP32 do poste

Depois de analisar, publica so o rotulo ("livre" ou "buraco") num topico MQTT,
que o ESP32 no Wokwi assina [HU-07, RF10]. A foto nunca vai para o MQTT [RNF01].

Decisao de 12/11/2026: deteccao de carros e motos removida; a API so detecta buracos.

Desde 15/11/2026 tambem recebe as fotos da camera do poste (ESP32-CAM): rotas /poste/...
em poste.py, guardadas no SQLite (banco.py) [HU-23, HU-24, RF22, RF23].

Desde 13/11/2026 a mesma API tambem serve:
- a mini trilha de IA (GET /trilha) [HU-19, RF18];
- o assistente com Gemini Flash (POST /assistente/trajeto e /assistente/descricao)
  [HU-21, HU-22, RF20, RF21, RN06, RN07]. Ver assistente.py.

Como rodar (dentro de visao/api):
    uvicorn main:app --reload
Pagina de testes: http://127.0.0.1:8000/docs
"""
import io
import json
import os
import threading
import time
import uuid
from collections import defaultdict, deque
from contextlib import asynccontextmanager

from dotenv import load_dotenv

# Le o arquivo .env (chave do Gemini etc.) antes de qualquer configuracao.
load_dotenv()

from fastapi import FastAPI, File, HTTPException, Request, UploadFile
from fastapi.concurrency import run_in_threadpool
from fastapi.middleware.cors import CORSMiddleware
import paho.mqtt.client as mqtt
from PIL import Image, UnidentifiedImageError
from ultralytics import YOLO

import banco
import poste
from assistente import (
    AssistenteIndisponivel,
    DadosGrandesDemais,
    PedidoDescricao,
    PedidoTrajeto,
    responder_trajeto,
    sugerir_descricao,
)

# ---------------------------------------------------------------------------
# Configuracao
# Valores marcados como PROPOSTA ainda precisam ser aprovados pela equipe.
# ---------------------------------------------------------------------------
CONFIG = {
    # PROPOSTA: limite de 5 MB por foto (protege a API publica na VPS).
    "tamanho_max_bytes": 5 * 1024 * 1024,
    # Confianca minima 0.40 (decisao do Joao, 14/11/2026), escolhida pelo experimento
    # treino/experimento_confianca.py nas 67 imagens de teste: F1 76,2% (contra 74,9% em 0.25),
    # 14 falsos positivos a menos; perde 5 de 130 buracos pequenos, medios e grandes iguais.
    "confianca_minima": float(os.getenv("CONFIANCA_MINIMA", "0.40")),
    "tipos_aceitos": {"image/jpeg", "image/png", "image/webp"},
    # Origens que podem chamar a API pelo navegador (app React em desenvolvimento).
    "origens_permitidas": os.getenv(
        "ORIGENS_PERMITIDAS", "http://localhost:5173,http://localhost:3000"
    ).split(","),
    # MQTT [HU-07, RF10]. Broker publico HiveMQ aprovado pelo Joao em 12/11/2026 SO para o
    # prototipo: qualquer pessoa pode ler e publicar no topico. Versao real: broker proprio
    # com usuario, senha e TLS (porta 8883).
    "mqtt_ativo": os.getenv("MQTT_ATIVO", "1") == "1",
    "mqtt_broker": os.getenv("MQTT_BROKER", "broker.hivemq.com"),
    "mqtt_porta": int(os.getenv("MQTT_PORTA", "1883")),
    "mqtt_topico": os.getenv("MQTT_TOPICO", "celia/poste01/camera"),
    # PROPOSTA: cada IP pode fazer ate 5 perguntas por minuto ao assistente. Protege a cota
    # gratuita do Gemini (cerca de 5 a 15 por minuto para a chave inteira).
    "assistente_limite_por_minuto": int(os.getenv("ASSISTENTE_LIMITE_POR_MINUTO", "5")),
}

# Protege contra "bomba de descompressao": imagem pequena em bytes, gigante em pixels.
Image.MAX_IMAGE_PIXELS = 40_000_000

PASTA_MODELOS = os.path.join(os.path.dirname(__file__), "modelos")

# ---------------------------------------------------------------------------
# Modelo: carregado uma vez so, quando a API liga (carregar a cada foto seria lento).
# ---------------------------------------------------------------------------
modelo_buracos = YOLO(os.path.join(PASTA_MODELOS, "buracos.pt"))  # treinado pela equipe [HU-06]

# A PRIMEIRA predicao e lenta (o PyTorch se prepara nela; no teste de 14/11 passou de 1 min).
# Por isso a API faz uma predicao de aquecimento numa imagem vazia logo que liga, em segundo
# plano, e avisa em GET / quando terminou ("modelo_pronto"). O app mostra "Preparando a IA"
# enquanto isso [HU-05, RF08].
estado_modelo = {"pronto": False}

# Trava do modelo: so UMA predicao por vez. O aquecimento, o /analisar e a camera do poste
# usam o mesmo modelo; sem a trava, duas fotos ao mesmo tempo (ou uma foto durante o
# aquecimento) disputariam o estado interno do YOLO. Quem chega depois espera a vez.
trava_modelo = threading.Lock()


def aquecer_modelo() -> None:
    """Roda uma predicao numa imagem cinza para a primeira foto do cidadao ja sair rapida."""
    with trava_modelo:
        modelo_buracos.predict(Image.new("RGB", (640, 640), "gray"), verbose=False)
    estado_modelo["pronto"] = True

# Licoes da mini trilha de IA [HU-19, RF18]: lidas uma vez, quando a API liga.
with open(os.path.join(os.path.dirname(__file__), "dados", "trilha.json"), encoding="utf-8") as arquivo:
    TRILHA = json.load(arquivo)

# ---------------------------------------------------------------------------
# MQTT: a API e quem PUBLICA; o ESP32 e quem ASSINA o topico.
# ---------------------------------------------------------------------------
# client_id aleatorio: dois clientes com o mesmo id derrubam um ao outro no broker.
cliente_mqtt = mqtt.Client(
    mqtt.CallbackAPIVersion.VERSION2, client_id=f"celia-api-{uuid.uuid4().hex[:8]}"
)


@asynccontextmanager
async def ciclo_de_vida(app: FastAPI):
    """Aquece o modelo, cria o banco, conecta ao broker quando a API liga e desconecta ao desligar."""
    banco.criar_tabelas()  # camera do poste [HU-24, RF23]
    # Thread separada: a API ja responde (GET / com modelo_pronto=False) enquanto aquece.
    threading.Thread(target=aquecer_modelo, daemon=True).start()
    if CONFIG["mqtt_ativo"]:
        # connect_async nao trava a API se o broker estiver fora do ar;
        # loop_start cria uma thread que mantem a conexao e reconecta sozinha.
        cliente_mqtt.connect_async(CONFIG["mqtt_broker"], CONFIG["mqtt_porta"])
        cliente_mqtt.loop_start()
    yield
    if CONFIG["mqtt_ativo"]:
        cliente_mqtt.loop_stop()
        cliente_mqtt.disconnect()


def publicar_resultado(resultado: str) -> bool:
    """Publica o rotulo no topico MQTT do poste [HU-07, RF10]. Devolve True se foi enviado.

    retain=False (decisao do Joao, 12/11/2026): o broker NAO guarda a mensagem. Com retain,
    um ESP32 que reiniciasse receberia um "buraco" antigo como se fosse novo e ficaria
    vermelho por mais 10 min. qos=1: o broker confirma o recebimento.
    Se o broker estiver fora, devolve False e a API continua respondendo ao app.
    """
    if not CONFIG["mqtt_ativo"] or not cliente_mqtt.is_connected():
        return False
    info = cliente_mqtt.publish(CONFIG["mqtt_topico"], resultado, qos=1, retain=False)
    return info.rc == mqtt.MQTT_ERR_SUCCESS


app = FastAPI(title="C.E.L.I.A. · API", version="0.4.0", lifespan=ciclo_de_vida)

# Rotas da camera do poste (ESP32-CAM) [HU-23, HU-24]
app.include_router(poste.router)

app.add_middleware(
    CORSMiddleware,
    allow_origins=CONFIG["origens_permitidas"],
    allow_methods=["GET", "POST"],
    allow_headers=["*"],
)


def extrair_caixas(resultado) -> list[dict]:
    """Converte as deteccoes do YOLO em uma lista simples de dicionarios.

    Cada caixa tem x1, y1 (canto superior esquerdo), x2, y2 (canto inferior direito),
    em pixels da foto original, e a confianca do modelo (0 a 1).
    """
    caixas = []
    for caixa in resultado.boxes:
        x1, y1, x2, y2 = caixa.xyxy[0].tolist()
        caixas.append({
            "x1": round(x1),
            "y1": round(y1),
            "x2": round(x2),
            "y2": round(y2),
            "confianca": round(float(caixa.conf[0]), 2),
        })
    return caixas


def classificar_trecho(caixas_buracos: list) -> str:
    """Decide o resultado final da foto: "buraco" se achou ao menos um, senao "livre"."""
    if caixas_buracos:
        return "buraco"
    return "livre"


def analisar_imagem(imagem: Image.Image) -> tuple[str, list[dict]]:
    """Roda o modelo de buracos na imagem (uma de cada vez) e devolve (resultado, caixas).

    Funcao "lenta" (bloqueia enquanto o YOLO trabalha): as rotas chamam com
    run_in_threadpool para a API continuar respondendo outras requisicoes enquanto isso.
    """
    with trava_modelo:
        res_buracos = modelo_buracos.predict(imagem, conf=CONFIG["confianca_minima"], verbose=False)[0]
    caixas_buracos = extrair_caixas(res_buracos)
    return classificar_trecho(caixas_buracos), caixas_buracos


# A camera do poste (poste.py) usa a mesma analise e o mesmo MQTT desta API.
poste.configurar(
    analisar=analisar_imagem,
    publicar=publicar_resultado,
    modelo_pronto=lambda: estado_modelo["pronto"],
)


async def ler_imagem_em_memoria(foto: UploadFile) -> Image.Image:
    """Valida o upload e abre a imagem SO na memoria, sem salvar em disco [RNF01, HU-08].

    Dispara HTTPException se o arquivo nao for imagem ou passar do tamanho maximo.
    """
    if foto.content_type not in CONFIG["tipos_aceitos"]:
        raise HTTPException(415, "Envie uma imagem JPG, PNG ou WEBP.")

    # Le no maximo o limite + 1 byte: se vier mais que isso, o arquivo e grande demais.
    dados = await foto.read(CONFIG["tamanho_max_bytes"] + 1)
    if len(dados) > CONFIG["tamanho_max_bytes"]:
        raise HTTPException(413, "Foto muito grande. Limite de 5 MB.")

    # O content_type vem do cliente e pode mentir: confirmamos abrindo de verdade.
    try:
        imagem = Image.open(io.BytesIO(dados))
        imagem.load()
    except (UnidentifiedImageError, Image.DecompressionBombError, OSError):
        raise HTTPException(400, "O arquivo enviado nao e uma imagem valida.")

    return imagem.convert("RGB")


@app.get("/")
def status() -> dict:
    """Rota simples para conferir se a API esta no ar e se o modelo ja aqueceu."""
    return {"status": "online", "projeto": "C.E.L.I.A.", "modelo_pronto": estado_modelo["pronto"]}


@app.post("/analisar")
async def analisar(foto: UploadFile = File(...)) -> dict:
    """Analisa a foto com o modelo de buracos e avisa o poste [HU-05, HU-06, HU-07, RF08-RF10]."""
    imagem = await ler_imagem_em_memoria(foto)
    resultado, caixas_buracos = await run_in_threadpool(analisar_imagem, imagem)

    # So o rotulo vai para o MQTT, nunca a foto [RNF01].
    mqtt_enviado = publicar_resultado(resultado)

    # A imagem existe so dentro desta funcao: ao terminar, o Python a descarta.
    return {
        "resultado": resultado,
        "caixas_buracos": caixas_buracos,
        "mqtt_enviado": mqtt_enviado,
        "largura": imagem.width,
        "altura": imagem.height,
    }


# ---------------------------------------------------------------------------
# Mini trilha de IA [HU-19, RF18]
# ---------------------------------------------------------------------------
@app.get("/trilha")
def listar_trilha() -> dict:
    """Devolve a trilha inteira (titulo e licoes) para o app montar a tela."""
    return TRILHA


@app.get("/trilha/{id_licao}")
def ver_licao(id_licao: int) -> dict:
    """Devolve uma licao pelo numero. Responde 404 se ela nao existir."""
    for licao in TRILHA["licoes"]:
        if licao["id"] == id_licao:
            return licao
    raise HTTPException(404, "Licao nao encontrada.")


# ---------------------------------------------------------------------------
# Assistente de IA com Gemini [HU-21, HU-22, RF20, RF21]
# ---------------------------------------------------------------------------
# Horarios dos ultimos pedidos de cada IP. Fica so na memoria: some quando a API reinicia.
pedidos_por_ip: dict[str, deque] = defaultdict(deque)


def checar_limite(ip: str) -> None:
    """Recusa (429) quem passar do limite de perguntas por minuto ao assistente."""
    agora = time.monotonic()
    horarios = pedidos_por_ip[ip]
    while horarios and agora - horarios[0] > 60:  # descarta pedidos com mais de 1 minuto
        horarios.popleft()
    if len(horarios) >= CONFIG["assistente_limite_por_minuto"]:
        raise HTTPException(429, "Muitas perguntas seguidas. Espere um minuto.")
    horarios.append(agora)


@app.post("/assistente/trajeto")
def assistente_trajeto(pedido: PedidoTrajeto, request: Request) -> dict:
    """Responde perguntas sobre o caminho usando so os trechos e locais enviados [RF20, RN06]."""
    checar_limite(request.client.host)
    try:
        return responder_trajeto(pedido)
    except DadosGrandesDemais as erro:
        raise HTTPException(413, str(erro))
    except AssistenteIndisponivel as erro:
        raise HTTPException(503, str(erro))


@app.post("/assistente/descricao")
def assistente_descricao(pedido: PedidoDescricao, request: Request) -> dict:
    """Sugere o texto do report; o cidadao revisa antes de enviar [RF21, RN07]."""
    checar_limite(request.client.host)
    try:
        return sugerir_descricao(pedido)
    except AssistenteIndisponivel as erro:
        raise HTTPException(503, str(erro))
