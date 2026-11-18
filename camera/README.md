# Câmeras da C.E.L.I.A. (ESP32-CAM)

Duas montagens com a mesma placa, a ESP32-CAM (modelo AI-Thinker):

| Montagem | Pasta | Para que serve | Backlog (15/11/2026) |
| --- | --- | --- | --- |
| **Câmera do poste** | [`camera_poste/`](camera_poste/) | Mostra o vídeo ao vivo no navegador e, a cada 20 s, manda uma foto da calçada para a API de visão, com uma chave no cabeçalho. Sem Wi-Fi, tenta de novo na foto seguinte | HU-23, RF22, RNF11 |
| **Câmera da cadeira de rodas** | [`camera_cadeira/`](camera_cadeira/) | Salva uma foto a cada 5 s no cartão microSD, sem internet, para treinar o modelo de buracos com calçadas do Rio | HU-06 (fotos tiradas pela equipe) |

O resto do caminho fica com outras partes do sistema:

- **API de visão:** decide se há buraco (RF09), guarda a captura (RF23) e avisa o ESP32 do poste por MQTT (RF10).
- **ESP32 do Wokwi** (pasta [`edge/`](../edge/)): fica vermelho com motivo 5 (RN02, HU-07) e continua lendo o DHT22 e o LDR (RF14, RNF03).

## Peças e ligações

**Câmera da cadeira:** a ESP32-CAM fica encaixada na base com micro-USB, e o power bank vai no micro-USB. Não tem nenhum fio, e o cartão microSD (FAT32) entra na própria CAM.

**Câmera do poste:** a mesma montagem da cadeira. A ESP32-CAM fica encaixada na base com micro-USB, ligada no USB do computador, num carregador de tomada de 5 V ou num power bank, sem nenhum fio. Decisão de 15/11: a câmera do poste fica sem sensor, e o calor continua sendo medido pelo ESP32 do Wokwi.

## Como gravar

1. Na IDE do Arduino, instale o pacote de placas **esp32 (Espressif)** e escolha a placa **AI Thinker ESP32-CAM**.
2. Abra a pasta do sketch e cole os valores reais no começo do código: rede de **2,4 GHz**, endereço do túnel da API e chave da câmera. **Não salve esses valores no GitHub:** no repositório ficam os textos de exemplo.
3. Encaixe a CAM na base, ligue o USB e clique em Carregar. Se aparecer "Failed to connect", segure o botão IO0 da base, clique em Carregar e solte quando aparecer "Connecting...". No fim, aperte RST.
4. Abra o Monitor Serial em 115200 para acompanhar.

## Vídeo ao vivo (para a demonstração)

1. Ligue a câmera do poste e abra o Monitor Serial em 115200.
2. Quando aparecer uma linha como `Wi-Fi conectado (sinal -55 dBm). Video: http://192.168.0.23`, abra esse endereço no navegador. O computador precisa estar na mesma rede. O endereço `192.168.0.23` é só um exemplo: o real aparece no Monitor Serial.
3. A página mostra só o vídeo, no meio da tela, em 320x240 para ficar liso. Uma pessoa assiste por vez.
4. Com a chave colada, a cada 20 s a câmera tira uma foto em 640x480 e manda para a API. O vídeo pausa por um instante enquanto a foto sai. Sem a chave (texto de exemplo), só o vídeo funciona e nada é enviado.

A câmera não fica ligada o tempo todo: ela é ligada na hora da apresentação. O teste com a API está registrado em [`evidencias/`](evidencias/LEIA-ME.md).

## Como a câmera fala com a API (rota do João, HU-24)

Combinado com o João em 15/11. A rota já está no ar na API dele, por um túnel da Cloudflare.

| Item | Valor |
| --- | --- |
| Endereço | `http://<endereço do túnel>/poste/captura`. O endereço do túnel **muda quando o João reinicia a API**: peça o atual a ele e cole no `API_URL`. Com `https://` também funciona |
| Método e corpo | `POST`, com a foto JPEG pura no corpo (os bytes de `fb->buf`, tamanho `fb->len`) |
| Cabeçalhos | `Content-Type: image/jpeg`, `X-Chave: <chave da câmera>` (RNF11) e `X-Poste: poste01` |
| Foto | Uma a cada 20 s (RF22), em VGA (640x480), com limite de 1 MB |
| Respostas | 201: deu certo, com `{id, poste, recebida_em, largura, altura}` · 401: chave errada · 400, 413 ou 415: foto vazia, grande demais ou fora do JPEG |

A chave da câmera foi passada pelo João no privado: cole só na hora de gravar e **não suba no GitHub**.

## Privacidade (RNF04 e HU-08)

- Aponte as duas câmeras para o chão, 1 a 2 m à frente, para pegar a calçada e não as pessoas.
- Antes de usar no treino, apague as fotos da cadeira que mostrem rosto ou placa.
- Quem estiver na cadeira precisa concordar com o teste.
- As capturas do poste ficam no banco da API por decisão da equipe (RNF01).

## Se algo der errado

| O que acontece | O que fazer |
| --- | --- |
| "Camera: falhou ao iniciar" | Desligue, levante a trava do conector da câmera, encaixe a fita até o fim e feche a trava |
| Fica sem Wi-Fi | A rede precisa ser de 2,4 GHz. No roteador do celular, ative "Maximizar compatibilidade" (iPhone) ou a banda de 2,4 GHz (Android) |
| "API respondeu 401" | A `CHAVE_CAMERA` do código precisa ser a chave que o João mandou |
| "sem resposta da API" | O endereço do túnel mudou: peça o novo ao João |
| "o endereco redirecionou" | Troque `http://` por `https://` no `API_URL` |
| Cadeira: foto de cabeça para baixo ou espelhada | `VIRAR_IMAGEM` ou `ESPELHAR_IMAGEM` = `true` no `camera_cadeira.ino` |
| Poste: vídeo travando | Deixe só uma pessoa assistindo e a câmera perto do roteador. O vídeo já sai em 320x240 para ficar leve |
| Poste: "Camera: ocupada com o video" | O vídeo não largou a câmera em 3 s. A foto tenta de novo 20 s depois. Se repetir sempre, feche a página do vídeo |
| Poste: "nao conseguiu tirar a foto em 640x480" | A câmera não entregou um quadro novo no tamanho da foto. Tenta de novo em 20 s; se repetir, reinicie a placa (botão RST) |
| A placa reinicia sozinha | Alimentação fraca: use outro cabo ou outro power bank |
| Cadeira: LED piscando rápido sem parar | Falta o cartão microSD, ou ele não está em FAT32 |
| Cadeira: fotos sem data no nome | A rede do celular não estava disponível quando a câmera ligou; as fotos ficam numeradas |
