# Status da visao computacional (atualizado em 12/11/2026)

Responsavel: Joao. Prazo da GS: 19/11/2026, 23h55.

## Pronto
- **Modelo de buracos [HU-06]: v2 em uso (decisao do Joao, 12/11/2026).** `api/modelos/buracos.pt` = v2; `api/modelos/buracos_v1.pt` = v1 (guardado para voltar).
  Dataset "Pothole Dataset" (KugaN, v2, CC BY 4.0): https://universe.roboflow.com/kugan/pothole-mftcc
  - v1: so o dataset. Teste (67 imagens de asfalto): precisao 85,7%, recall 74,2%, mAP@50 83,6%. Prints em `treino/`.
  - v2: dataset + 38 imagens negativas de calcada portuguesa (Wikimedia, fontes em `treino/fontes_negativas.md`, etapa 5b do notebook). Teste: precisao 85,0%, recall 69,8%, mAP@50 79,9%.
  - Motivo da escolha: no asfalto o v2 perde cerca de 4 pontos de mAP@50, mas nas 6 fotos de calcada passou de 4 para 6 resultados certos (tabela abaixo).
- **API [HU-05, HU-08, RF08, RF09, RNF01]:** `api/main.py` (FastAPI). `GET /` e `POST /analisar` funcionando localmente.
  **Decisao 12/11/2026:** a API so detecta buracos. Deteccao de carros e motos (YOLO pre-treinado) removida.
  Resposta: `resultado` ("livre" ou "buraco"), `caixas_buracos`, `mqtt_enviado`, `largura`, `altura`.
- **MQTT [HU-07, RF10] (12/11):** a API publica o rotulo em `celia/poste01/camera` no broker publico HiveMQ (aprovado pelo Joao so para o prototipo), `qos=1`, **sem retain** (decisao 12/11: evita o ESP32 receber um "buraco" antigo ao reiniciar). Broker fora do ar nao derruba a API (`mqtt_enviado: false`). Testado de ponta a ponta com `api/ouvir_mqtt.py`. Falta o lado do ESP32 no Wokwi.

## Testes da API

### Antes e depois (v1 x v2), mesmas 6 fotos
| # | Foto (arquivo) | Esperado | v1 | v2 |
| --- | --- | --- | --- | --- |
| 1 | Buraco no asfalto (7505890e...jpg) | buraco | ✅ 0,78 | ✅ 0,74 (+ caixa fraca extra 0,28 numa rachadura) |
| 2 | Calcada quebrada sem terra exposta (imagemteste.jpg) | buraco | ❌ caixa errada na borda, 0,31 | ✅ parcial: 0,47 sobre o dano central; 0,73 nos cacos da borda esquerda |
| 3 | Placa de concreto solta (novaimagem.jpg) | buraco | ✅ 0,74 | ✅ 0,61 |
| 4 | Pedras arrancadas (thumb-calcada-irregular...webp) | buraco | ✅ 0,83 | ✅ 0,89 |
| 5 | Calcadao com desenhos escuros (72930c8f...jpg) | livre | ❌ falso positivo 0,69 e 0,64 | ✅ livre |
| 6 | Calcada lisa (calcadalisa.jpg) | livre | ✅ livre | ✅ livre |
| | **Total** | | **4 de 6** | **6 de 6** |

MQTT: `mqtt_enviado: true` nas 6 fotos do v2. Ressalva: 6 fotos e amostra pequena (indicio, nao prova); fotos 2 a 5 sao da internet e a fonte precisa ser citada.

### Historico detalhado do v1
| Foto | Esperado | Obtido |
| --- | --- | --- |
| Buraco em asfalto | buraco | buraco, confianca 0,78, caixa no centro ✅ |
| Calcada quebrada do Rio | buraco | caixa errada na borda, confianca 0,31 ❌ (mesmo com conf 0,05) |
| Calcada portuguesa quebrada com placa de concreto solta (novaimagem.jpg, 487x630) | buraco | buraco, confianca 0,74, caixa sobre a area de pedras soltas e terra a direita da placa ✅. (Na versao antiga, marcou 7 carros estacionados na rua como veiculos: motivo da remocao.) |
| Calcada portuguesa com trecho de pedras arrancado, terra exposta (thumb-calcada-irregular-no-rio-de-janeiro-1.webp, 600x358, fonte da internet a citar) | buraco | buraco, confianca 0,83, caixa cobrindo quase exatamente o buraco ✅. Nao confundiu as tampas de bueiro/caixas de inspecao com buraco; nenhum veiculo |
| Calcadao da praia com desenhos escuros de pedra portuguesa, sem buraco (72930c8f...jpg, 552x414, fonte da internet a citar) | livre | buraco ❌ falso positivo: 2 caixas sobre os desenhos escuros (0,69 e 0,64) |
| Calcada lisa, sem desenho (1086x652) | livre | livre, nenhuma caixa ✅ |

Diagnostico: mudanca de dominio. O dataset e de asfalto; o modelo nao reconhece calcada quebrada.
Falso positivo (12/11): o modelo aprendeu "mancha escura irregular sobre fundo claro" = buraco. Desenhos de pedra portuguesa preta enganam. Confiancas se sobrepoem (falsos 0,64 e 0,69; verdadeiros 0,74, 0,78 e 0,83), entao subir a confianca minima nao resolve de forma confiavel.
Correcao aplicada no v2 (aprovada 12/11): 38 fotos de calcadas com desenhos escuros SEM rotulo (imagens de fundo). O falso positivo da foto 5 sumiu.

## Proximos passos
1. Feito em 12/11: v2 testado e adotado (6 de 6). Pendente: citar a fonte das fotos de teste 2 a 5.
2. **Assistente LLM ajuda a descrever o report [RF21, HU-22, RN07]:** provedor **NAO definido** (RNF09: API pronta ou modelo aberto na VPS; Claude, GPT e Gemini em aberto). Recebe so rotulo, local e texto do cidadao, nunca a foto. Precisa de chave de API (guardar em `.env`, nunca no codigo).
2b. **Imagens negativas (usadas no v2):** 38 fotos de calcada portuguesa sem buraco do Wikimedia em `treino/negativas.zip` (fontes em `treino/fontes_negativas.md`). Entram pela etapa 5b do notebook, sem rotulo.
3. **Retreino com calcadas do Rio:** equipe tira 50 a 100 fotos (13/11), fork do dataset do KugaN no Roboflow e rotula com a classe `pothole` (14/11), retreino partindo do `buracos.pt` v2 (15/11), mantendo a etapa 5b das negativas. Repetir as 6 fotos de teste (nao colocar no treino).
4. **MQTT no ESP32 (Wokwi):** assinar `celia/poste01/camera` e usar o resultado no status [HU-07, RF10, RN02]. Lado da API pronto.
5. **VPS:** hospedar a API [HU-17]. Decidir se `/docs` fica publico.

## Propostas no codigo ainda NAO aprovadas
- Limite de upload de 5 MB.
- Confianca minima 0,25.

## Como rodar a API
```powershell
cd visao\api
python -m venv .venv
.venv\Scripts\python -m pip install -r requirements.txt
.venv\Scripts\python -m uvicorn main:app --reload
```
Abrir http://127.0.0.1:8000/docs

## Como testar o MQTT
Terminal 1 (faz o papel do ESP32):
```powershell
cd visao\api
.venv\Scripts\python ouvir_mqtt.py
```
Terminal 2: rodar a API e enviar uma foto em http://127.0.0.1:8000/docs. O resultado aparece no terminal 1.
Se faltar pacote: `.venv\Scripts\python -m pip install -r requirements.txt` (agora inclui paho-mqtt).
Variaveis opcionais: `MQTT_ATIVO=0` desliga o MQTT; `MQTT_BROKER`, `MQTT_PORTA`, `MQTT_TOPICO` trocam o destino.
