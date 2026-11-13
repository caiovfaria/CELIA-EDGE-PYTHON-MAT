"""Escuta o topico MQTT do poste e mostra cada resultado que chega [HU-07, RF10].

Faz o papel do ESP32 para testar sem o Wokwi: deixe rodando num terminal,
envie uma foto para /analisar em outro e veja "livre" ou "buraco" aparecer aqui.

Como rodar (dentro de visao/api):
    .venv\\Scripts\\python ouvir_mqtt.py
Parar: Ctrl+C
"""
import os
import uuid
from datetime import datetime

import paho.mqtt.client as mqtt

BROKER = os.getenv("MQTT_BROKER", "broker.hivemq.com")
PORTA = int(os.getenv("MQTT_PORTA", "1883"))
TOPICO = os.getenv("MQTT_TOPICO", "celia/poste01/camera")


def ao_conectar(cliente, dados, flags, codigo, propriedades) -> None:
    """Chamada quando a conexao com o broker abre: assina o topico."""
    print(f"Conectado a {BROKER}:{PORTA}. Ouvindo o topico '{TOPICO}'...")
    cliente.subscribe(TOPICO, qos=1)


def ao_receber(cliente, dados, mensagem) -> None:
    """Chamada a cada mensagem nova no topico."""
    texto = mensagem.payload.decode("utf-8", errors="replace")
    if not texto:
        print("(mensagem retida apagada do topico)")
        return
    # A API publica sem retain; "retida" so aparece se alguem publicou com retain no topico.
    origem = "retida (ultima guardada no broker)" if mensagem.retain else "nova"
    print(f"[{datetime.now():%H:%M:%S}] {texto}  ({origem})")


cliente = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"celia-ouvinte-{uuid.uuid4().hex[:8]}")
cliente.on_connect = ao_conectar
cliente.on_message = ao_receber
cliente.connect(BROKER, PORTA)
cliente.loop_forever()
