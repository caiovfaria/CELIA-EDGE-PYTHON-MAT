# Prompts usados com IA (visao computacional)

Registro exigido pela GS: IA como copiloto. Todo codigo gerado foi revisado e precisa ser explicavel pela equipe.

## 2026-11-12 · Escolha do dataset e notebook de treino [HU-06, RF09]

**Ferramenta:** Claude Code

**Prompts (resumo):**
1. "Quais palavras-chave de classe procurar no Roboflow? Usando buracos em pt-br filtrando YOLOv8 nao achei nenhum."
2. "Abre no meu Chrome e faz a busca comigo."

**Resultado:**
- Busca por `pothole` (os datasets estao em ingles). Comparados dois candidatos CC BY 4.0 de 1 classe.
- Escolhido: "Pothole Dataset", por KugaN, v2, https://universe.roboflow.com/kugan/pothole-mftcc (1.595 imagens apos aumento de dados; 1.395 treino, 133 validacao, 67 teste).
- Reserva: "pothole Dataset", por Laura Tsanaullailla, https://universe.roboflow.com/laura-tsanaullailla/pothole-anacz (2.013 imagens).
- Gerado `treino/treino_buracos.ipynb` (YOLOv8n, 50 epocas, saida `buracos.pt`).

**Treino 1 (2026-11-12):** YOLOv8n, 50 epocas, 0,34 h na Tesla T4 do Colab. Validacao (133 imagens, 357 buracos):
precisao 83,7%, recall 66,2%, mAP@50 77,3%, mAP@50-95 49,6%. Inferencia: 4,8 ms por imagem na GPU.
Teste (67 imagens nunca vistas, 182 buracos): precisao 85,7%, recall 74,2%, mAP@50 83,6%, mAP@50-95 54,4%.
Obs.: conjunto de teste pequeno, entao os numeros variam bastante de uma amostra para outra. Fotos de asfalto, nao de calcada.

**Limitacao conhecida (modelo):** ver abaixo.

## 2026-11-12 · API de visao, rota /analisar [HU-05, HU-08, RF08, RF09, RNF01]

**Ferramenta:** Claude Code

**Prompt (resumo):** decisao da equipe: FastAPI. "Qual o proximo passo?"

**Resultado:** `api/main.py` com `GET /` e `POST /analisar` (campo `foto`). Valida tipo e tamanho, abre a imagem so em memoria,
roda `yolov8n.pt` (carro e moto) e `buracos.pt`, responde `resultado`, `caixas_buracos` e `caixas_veiculos`.
Propostas ainda nao aprovadas no codigo: limite de 5 MB e confianca minima 0,25. MQTT e LLM ficam para os proximos passos.

**Limitacao conhecida:** as fotos do dataset sao de asfalto de rua, nao de calcada. O modelo pode errar em calcadas do Rio (pedra portuguesa, por exemplo).

## 2026-11-12 · Remocao da deteccao de veiculos [HU-05, RF09]

**Ferramenta:** Claude Code

**Prompts (resumo):** "Por que esses carros? O objetivo e detectar buracos." / "Tirar carros e manter so buracos."

**Motivo:** o YOLO pre-treinado nao diferencia carro na calcada de carro na rua. Numa foto de calcada com 7 carros estacionados na rua, todos foram detectados; sem buraco, a API responderia "obstaculo" com a calcada livre (falso positivo).

**Resultado:** `api/main.py` agora roda so o `buracos.pt`. Resposta: `resultado` ("livre" ou "buraco"), `caixas_buracos`, `largura`, `altura`. Retestado com as 2 fotos de calcada: mesmos buracos (0,74 e 0,83).

## 2026-11-12 · Publicacao MQTT do resultado [HU-07, RF10, RNF01]

**Ferramenta:** Claude Code

**Prompts (resumo):** "O que e MQTT? O que e broker? Explique como leigo." / "Pode usar o HiveMQ, implementa o MQTT."

**Resultado:** `api/main.py` publica so o rotulo ("livre" ou "buraco") em `celia/poste01/camera` (broker publico HiveMQ, porta 1883, `qos=1`, `retain=True`) com `paho-mqtt`. Conexao assincrona com reconexao automatica; se o broker cair, a API segue respondendo e devolve `mqtt_enviado: false`. Criado `api/ouvir_mqtt.py` para testar sem o Wokwi. Teste: 2 fotos publicadas e recebidas pelo ouvinte; teste com broker invalido devolveu 200 e `mqtt_enviado: false`.

**Limitacao (seguranca):** broker publico sem autenticacao: qualquer pessoa pode ler ou publicar no topico (risco de integridade). Versao real: broker proprio com usuario, senha e TLS (8883).

## 2026-11-12 · Treino 2 com 38 imagens negativas [HU-06, RF09]

**Ferramenta:** Claude Code (busca de fontes, download do Wikimedia, etapa 5b do notebook)

**Prompts (resumo):** "Para incluir as imagens negativas, eu teria que pegar uma a uma no Google?" / "Faca este passo a passo para mim." / "Sim, pode baixar."

**Treino 2 (2026-11-12):** YOLOv8n, 50 epocas, 0,32 h na T4, 1.395 imagens com buraco + 38 de fundo (log: "1395 images, 38 backgrounds"). Melhor epoca: 43.
Validacao (133 imagens, 357 buracos, mesmas do treino 1): precisao 79,8% (antes 83,7%), recall 72,0% (antes 66,2%), mAP@50 77,6% (antes 77,3%), mAP@50-95 48,5% (antes 49,6%).
Leitura: na validacao (so asfalto) os dois modelos empatam; o conjunto nao tem calcada nem desenho, entao nao mede o falso positivo que as negativas querem corrigir. Isso so aparece no reteste das 6 fotos da API.
Teste (67 imagens de asfalto, 182 buracos): precisao 85,0% (antes 85,7%), recall 69,8% (antes 74,2%), mAP@50 79,9% (antes 83,6%), mAP@50-95 51,7% (antes 54,4%).
Leitura: no asfalto o treino 2 ficou um pouco abaixo (cerca de 4 pontos de mAP@50 e de recall; conjunto pequeno, varia bastante). Escolha entre v1 e v2 depende do reteste das 6 fotos de calcada.
Reteste na API (mesmas 6 fotos): v1 4 de 6, v2 6 de 6 (calcadao com desenhos passou a "livre"; calcada quebrada passou a ter caixa sobre o dano real). **Decisao do Joao (12/11): adotar o v2.** v1 guardado como `api/modelos/buracos_v1.pt`.

## 2026-11-12 · Retain removido do MQTT [HU-07, RF10]

**Prompt (resumo):** "Retire o retain da API."

**Motivo:** a biblioteca do ESP32 (PubSubClient) nao informa se a mensagem e retida. Com `retain=True`, um Wokwi reiniciado receberia o ultimo resultado (talvez de horas antes) como novo e ficaria vermelho por 10 min. Agora `retain=False`; a mensagem retida que ficou no topico `celia/poste01/camera` foi apagada.
