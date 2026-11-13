"""API de visao computacional da C.E.L.I.A. [HU-05, HU-07, HU-08, RF08, RF09, RF10, RNF01]

Recebe a foto de uma calcada, roda o modelo de buracos treinado pela equipe e responde:
- resultado: "livre" ou "buraco"
- caixas_buracos: para o app desenhar sobre a foto
- mqtt_enviado: se o resultado foi publicado para o ESP32 do poste

Depois de analisar, publica so o rotulo ("livre" ou "buraco") num topico MQTT,
que o ESP32 no Wokwi assina [HU-07, RF10]. A foto nunca vai para o MQTT [RNF01].

Decisao de 12/11/2026: deteccao de carros e motos removida; a API so detecta buracos.

Como rodar (dentro de visao/api):
    uvicorn main:app --reload
Pagina de testes: http://127.0.0.1:8000/docs
"""
import io
import os
import uuid
from contextlib import asynccontextmanager

from fastapi import FastAPI, File, HTTPException, UploadFile
from fastapi.middleware.cors import CORSMiddleware
import paho.mqtt.client as mqtt
from PIL import Image, UnidentifiedImageError
from ultralytics import YOLO

# ---------------------------------------------------------------------------
# Configuracao
# Valores marcados como PROPOSTA ainda precisam ser aprovados pela equipe.
# ---------------------------------------------------------------------------
CONFIG = {
    # PROPOSTA: limite de 5 MB por foto (protege a API publica na VPS).
    "tamanho_max_bytes": 5 * 1024 * 1024,
    # PROPOSTA: confianca minima de 0.25 (padrao do YOLO). Baixar aumenta o recall.
    "confianca_minima": float(os.getenv("CONFIANCA_MINIMA", "0.25")),
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
}

# Protege contra "bomba de descompressao": imagem pequena em bytes, gigante em pixels.
Image.MAX_IMAGE_PIXELS = 40_000_000

PASTA_MODELOS = os.path.join(os.path.dirname(__file__), "modelos")

# ---------------------------------------------------------------------------
# Modelo: carregado uma vez so, quando a API liga (carregar a cada foto seria lento).
# ---------------------------------------------------------------------------
modelo_buracos = YOLO(os.path.join(PASTA_MODELOS, "buracos.pt"))  # treinado pela equipe [HU-06]

# ---------------------------------------------------------------------------
# MQTT: a API e quem PUBLICA; o ESP32 e quem ASSINA o topico.
# ---------------------------------------------------------------------------
# client_id aleatorio: dois clientes com o mesmo id derrubam um ao outro no broker.
cliente_mqtt = mqtt.Client(
    mqtt.CallbackAPIVersion.VERSION2, client_id=f"celia-api-{uuid.uuid4().hex[:8]}"
)


@asynccontextmanager
async def ciclo_de_vida(app: FastAPI):
    """Conecta ao broker quando a API liga e desconecta quando ela desliga."""
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


app = FastAPI(title="C.E.L.I.A. · API de visao", version="0.3.0", lifespan=ciclo_de_vida)

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
    """Rota simples para conferir se a API esta no ar."""
    return {"status": "online", "projeto": "C.E.L.I.A."}


@app.post("/analisar")
async def analisar(foto: UploadFile = File(...)) -> dict:
    """Analisa a foto com o modelo de buracos e avisa o poste [HU-05, HU-06, HU-07, RF08-RF10]."""
    imagem = await ler_imagem_em_memoria(foto)
    conf = CONFIG["confianca_minima"]

    # verbose=False evita imprimir detalhes de cada foto no terminal.
    res_buracos = modelo_buracos.predict(imagem, conf=conf, verbose=False)[0]
    caixas_buracos = extrair_caixas(res_buracos)
    resultado = classificar_trecho(caixas_buracos)

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
