# Status da visao computacional (atualizado em 13/11/2026)

> 18/11/2026: a pasta `visao/` foi juntada com a `python/` na pasta `python-backend-IA/`. Os caminhos antigos abaixo, nas partes de histórico, ficam como estavam.

## Pendencias em 13/11/2026 (modelo de buracos e assistente Gemini funcionando)

Feito em 13/11: assistente (/assistente/trajeto e /assistente/descricao), trilha (GET /trilha) e correcao do carregamento sem fim (timeout de 20 s e sem repeticao automatica da biblioteca). Guia do app para o Isaac em INTEGRACAO.md, secao 7.

1. **Mini trilha de IA para o trabalho [HU-19, RF18, RNF08]:** hoje sao 5 licoes de rascunho em `api/dados/trilha.json`. Falta definir com a equipe os temas, revisar os textos e conferir RNF08 (texto simples, leitor de tela).
2. **Imagem junto no pedido ao Gemini [HU-22, RF21] (pedido do Joao em 13/11, ainda NAO implementado):** muda decisoes ja escritas e precisa de aprovacao antes:
   - RNF01 diz que so o resultado trafega; a foto passaria a sair da API para o Google.
   - Na cota gratis o Google pode usar o conteudo; foto real pode ter rosto, placa ou fachada (RNF04, LGPD).
   - Custo: imagem gasta bem mais tokens; a cota gratis acaba mais rapido.
   - Ideia a decidir: a rota /assistente/descricao recebe a foto em memoria, junto com o resultado do /analisar, repassa ao Gemini e descarta, sem salvar; app avisa o cidadao e pede consentimento.
3. **Modelo do Gemini:** o 3.8 esta instavel (503); decidir se o 3.5 vira o principal na demo.
4. **Isaac:** confirmar o formato de trechos e locais (INTEGRACAO.md secoes 5 a 7) e o endereco do app (CORS).
5. **VPS [HU-17]:** escolher provedor e subir a API; sem ela o app da demo nao acha a API.
6. **Retreino v3** com fotos de calcadas do Rio; citar as fontes das fotos de teste 2 a 5.
7. **Caio:** ESP32 assinando o MQTT (Etapa 4) e teste de ponta a ponta com video.
8. **Doc de LGPD:** registrar que, na cota gratis, o Google pode usar as conversas do assistente.
9. README com a ficha da CitizenTech, relatorio, matematica, pitch e programa Python da disciplina de Logica (aguardando resposta do Prof. Caina).

## Estado em 12/11/2026 (fim do dia)

Responsaveis: Caio (edge/ e Wokwi), Isaac (app React e Web Dev), Joao (python-backend-IA/: API, MQTT, visao computacional e programa Python da disciplina de Logica). Sem dono: matematica, relatorio, pitch, RF13 (reports a partir dos postes).

Feito hoje:
- Modelo v2 adotado (6 de 6 nas fotos de calcada); v1 guardado.
- API FastAPI com MQTT (HiveMQ, celia/poste01/camera, qos 1, sem retain), testada de ponta a ponta.
- Deteccao de carros e motos removida; field6 = 4 removido.
- PR #1 no repo do Caio (https://github.com/caiovfaria/GS-C.E.L.I.A.-EDGE/pull/1) com visao/ e INTEGRACAO.md. Pode ser mesclado (so adiciona arquivos).

Proximos passos (13/11 em diante):
1. Caio: merge do PR #1, responder as divergencias do INTEGRACAO.md e Etapa 4 (ESP32 assina o MQTT).
2. Joao: mandar ao Prof. Caina a duvida sobre o programa Python (a API conta para a disciplina?). Ate a resposta, nada de programa de terminal.
3. Equipe: 30 a 50 fotos de calcadas com buraco e 15 a 20 sem buraco (sem rostos e placas); separar 10 so para teste. Rotular no Roboflow e retreinar o v3 partindo do v2, com as negativas.
4. Teste de ponta a ponta com video: foto, API, LED vermelho no Wokwi, field6 = 5 no ThingSpeak.
5. Feito em 13/11: assistente com Gemini Flash (/assistente/trajeto e /assistente/descricao) e trilha (GET /trilha), testados com Gemini falso e com o Gemini real (gemini-3.8-flash, reserva gemini-3.5-flash). Chave no visao/api/.env (fora do git). Falta: Isaac confirmar o formato (INTEGRACAO.md secoes 5 e 6).
6. API numa VPS (HU-17) para o app do Isaac.
7. README com a ficha da CitizenTech na raiz do repo; relatorio, matematica e pitch.

Pendencias de decisao: provedor da VPS, modo chuva, formato dos banheiros, Perigo do calor em 41 ou 39 C, perguntas ao professor de Edge, fontes das fotos de teste 2 a 5, divergencias do INTEGRACAO.md (Flask x FastAPI, horario na mensagem MQTT).

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
2. **Assistente LLM ajuda a descrever o report [RF21, HU-22, RN07]:** provedor **Gemini Flash** (RNF09, decisao do Joao em 13/11/2026). Recebe so rotulo, local e texto do cidadao, nunca a foto. Precisa de chave de API (guardar em `.env`, nunca no codigo).
2b. **Imagens negativas (usadas no v2):** 38 fotos de calcada portuguesa sem buraco do Wikimedia em `treino/negativas.zip` (fontes em `treino/fontes_negativas.md`). Entram pela etapa 5b do notebook, sem rotulo.
3. **Retreino com calcadas do Rio:** equipe tira 50 a 100 fotos (13/11), fork do dataset do KugaN no Roboflow e rotula com a classe `pothole` (14/11), retreino partindo do `buracos.pt` v2 (15/11), mantendo a etapa 5b das negativas. Repetir as 6 fotos de teste (nao colocar no treino).
4. **MQTT no ESP32 (Wokwi):** assinar `celia/poste01/camera` e usar o resultado no status [HU-07, RF10, RN02]. Lado da API pronto.
5. **VPS:** hospedar a API [HU-17]. Decidir se `/docs` fica publico.

## Propostas no codigo ainda NAO aprovadas
- Limite de upload de 5 MB.
- Confianca minima 0,40 (decisao do Joao, 14/11/2026; ver secao "Experimento de confianca").

## Como rodar a API
```powershell
cd python-backend-IA\api
python -m venv .venv
.venv\Scripts\python -m pip install -r requirements.txt
.venv\Scripts\python -m uvicorn main:app --reload
```
Abrir http://127.0.0.1:8000/docs

## Como testar o MQTT
Terminal 1 (faz o papel do ESP32):
```powershell
cd python-backend-IA\api
.venv\Scripts\python ouvir_mqtt.py
```
Terminal 2: rodar a API e enviar uma foto em http://127.0.0.1:8000/docs. O resultado aparece no terminal 1.
Se faltar pacote: `.venv\Scripts\python -m pip install -r requirements.txt` (agora inclui paho-mqtt).
Variaveis opcionais: `MQTT_ATIVO=0` desliga o MQTT; `MQTT_BROKER`, `MQTT_PORTA`, `MQTT_TOPICO` trocam o destino.


## Experimento de confianca (14/11/2026) [HU-06, RF09]

Sem retreinar. Modelo v2, mesmas 67 imagens de teste do dataset (asfalto), acerto com IoU >= 0,5.
Script: `treino/experimento_confianca.py`. Pequeno: menos de 5% da foto; medio: 5% a 15%; grande: acima.

| Confianca | Precisao | Recall | F1 | Falsos positivos | Pequenos | Medios | Grandes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0,25 | 75,1% | 74,7% | 74,9% | 45 | 88/130 | 27/30 | 21/22 |
| 0,30 | 76,4% | 74,7% | 75,6% | 42 | 88/130 | 27/30 | 21/22 |
| 0,35 | 77,3% | 73,1% | 75,1% | 39 | 85/130 | 27/30 | 21/22 |
| **0,40** | **80,9%** | 72,0% | **76,2%** | 31 | 83/130 | 27/30 | 21/22 |

**Decisao do Joao (14/11/2026): 0,40.** Melhor F1 e menos alarmes falsos; a perda fica so em 5 buracos pequenos.
Tambem elimina o falso positivo da `wm_02.jpg` (calcada sem buraco, confianca 0,33).
Os numeros do treino (85,0% e 69,8%) vem do `val()`, que mede na confianca de melhor F1; aqui cada confianca e fixa.
Decisao do Joao (14/11/2026): sem retreino (v3 cancelado).
