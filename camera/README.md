# Câmera da C.E.L.I.A. (ESP32-CAM) · exemplo

Esta pasta explica como a câmera funcionaria no poste. A ESP32-CAM trabalha pela rede local: o vídeo abre num endereço IP, como `http://192.168.0.23`, só para quem está na mesma rede. Por isso não tem como deixar a câmera ligada para mostrar funcionando. Aqui ficam a explicação, um exemplo do que aparece e os programas.

## Como funciona

```
câmera no poste ──foto a cada 20 s──▶ API de visão (João) ──"buraco"──▶ MQTT ──▶ ESP32 do poste ──▶ LED vermelho
```

1. **A câmera liga e entra no Wi-Fi.** O Monitor Serial mostra o endereço do vídeo. Abrindo esse endereço no navegador, na mesma rede, aparece o vídeo ao vivo (320x240).
2. **A cada 20 s, ela tira uma foto** em 640x480 e manda para a API de visão do João, com uma chave no cabeçalho (HU-23, RF22, RNF11). O vídeo pausa por um instante enquanto a foto sai.
3. **A API procura buracos** na foto com o modelo treinado (RF09) e guarda a captura (RF23).
4. **Se encontrar um buraco, a API avisa o poste** pelo MQTT: manda a mensagem `buraco` no tópico `celia/poste01/camera` (RF10).
5. **O poste fica vermelho:** EVITE, motivo 5, por 10 min (1 min no modo demonstração), qualquer que seja a luz e o calor (RN02, HU-07). Se a API mandar `livre`, nada muda.

Sem Wi-Fi ou sem resposta da API, a câmera não trava: tenta de novo na foto seguinte.

## Exemplo do que aparece

Na câmera (Monitor Serial da IDE do Arduino, em 115200), quando a API acha um buraco:

```
C.E.L.I.A. - camera do poste (video + API)
Camera: ok
Wi-Fi conectado (sinal -55 dBm). Video: http://192.168.0.23
Foto 1: 13460 bytes (640x480)
API respondeu 201: {"id":12,"poste":"poste01","recebida_em":"2026-11-17T18:42:10+00:00","largura":640,"altura":480,"resultado":"buraco","caixas":[...],"mqtt_enviado":true,"report":"gerando"}
```

- **201:** a API recebeu a foto e guardou a captura.
- **resultado:** `buraco` ou `livre`, o que o modelo viu na calçada.
- **mqtt_enviado true:** a API avisou o poste pelo MQTT.

No poste (monitor serial do Wokwi), logo depois:

```
Camera: buraco recebido.
Camera: trecho em EVITE por 60 s (motivo buraco).
t=95s | T 26.0C U 50% IC 26.0C (normal) | luz 499 lux (adc 1001) lida normal / vale normal | STATUS 2 EVITE | motivo 5 buraco | buraco (camera) 60s
```

O endereço, o número da foto, o id e o horário mudam a cada vez. O lado do poste dá para testar no Wokwi sem câmera: publicando `buraco` no tópico pelo cliente web do HiveMQ, como explica o [roteiro da demonstração](../edge/ROTEIRO_DEMO.md#buraco-visto-pela-câmera-mqtt).

## As duas montagens

As duas usam a mesma placa, a ESP32-CAM (modelo AI-Thinker), encaixada na base com micro-USB e ligada num carregador de 5 V ou num power bank. Não tem nenhum fio.

| Montagem | Programa | Para que serve | Backlog |
| --- | --- | --- | --- |
| **Câmera do poste** | [`camera_poste/camera_poste.ino`](camera_poste/camera_poste.ino) | Vídeo ao vivo no navegador e uma foto a cada 20 s para a API de visão. Antes de mandar, confere que a foto é nova e tem mesmo 640x480 (lê o tamanho de dentro do JPEG) | HU-23, RF22, RNF11 |
| **Câmera da cadeira de rodas** | [`camera_cadeira/camera_cadeira.ino`](camera_cadeira/camera_cadeira.ino) | Salva uma foto a cada 5 s no cartão microSD, sem internet, para treinar o modelo de buracos com calçadas do Rio | HU-06 |

## Como a câmera fala com a API (rota do João, HU-24)

| Item | Valor |
| --- | --- |
| Endereço | `http://<endereço do túnel>/poste/captura`, por um túnel da Cloudflare. O endereço muda quando o João reinicia a API |
| Método e corpo | `POST`, com a foto JPEG pura no corpo |
| Cabeçalhos | `Content-Type: image/jpeg`, `X-Chave: <chave da câmera>` (RNF11) e `X-Poste: poste01` |
| Foto | Uma a cada 20 s (RF22), em 640x480, com limite de 1 MB |
| Respostas | 201: deu certo · 401: chave errada · 400, 413 ou 415: foto vazia, grande demais ou fora do JPEG |

No repositório, o Wi-Fi, o endereço da API e a chave ficam com textos de exemplo (`NOME_DA_REDE_2_4GHZ`, `ENDERECO_DO_TUNEL` e `COLE_A_CHAVE_DA_CAMERA_AQUI`). Os valores reais nunca vão para o GitHub.

## Privacidade (RNF04 e HU-08)

- As duas câmeras apontam para o chão, 1 a 2 m à frente, para pegar a calçada e não as pessoas.
- Antes de usar no treino, as fotos da cadeira que mostrem rosto ou placa são apagadas.
- Quem estiver na cadeira precisa concordar com o teste.
- As capturas do poste ficam no banco da API por decisão da equipe (RNF01).

## Para gravar numa placa

1. Na IDE do Arduino, instale o pacote de placas **esp32 (Espressif)** e escolha **AI Thinker ESP32-CAM**.
2. Numa cópia do programa, fora do repositório, troque os textos de exemplo pelos valores reais (rede de 2,4 GHz, endereço do túnel e chave da câmera).
3. Ligue o USB, clique em **Carregar** e abra o Monitor Serial em 115200.
