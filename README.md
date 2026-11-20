<p align="center">
  <img src="assets/celia-logo.png" alt="Logo da C.E.L.I.A.: Caminhos em Liberdade, Inclusão e Acessibilidade" width="420">
</p>

<h3 align="center">Caminhos em Liberdade, Inclusão e Acessibilidade</h3>

<p align="center">
  Inteligência artificial a serviço de quem tem deficiência física ou mobilidade reduzida no Rio de Janeiro.<br>
  Um modelo de visão computacional treinado pela equipe encontra buracos na calçada, e um assistente com o Gemini<br>
  ajuda a pessoa a planejar o caminho até o trabalho e leva os problemas da rua até a Prefeitura.
</p>

<p align="center">
  <img alt="YOLOv8" src="https://img.shields.io/badge/YOLOv8-modelo%20pr%C3%B3prio-111F68">
  <img alt="Gemini" src="https://img.shields.io/badge/Gemini-assistente%20LLM-8E75B2?logo=googlegemini&logoColor=white">
  <img alt="Python" src="https://img.shields.io/badge/Python-3.13-3776AB?logo=python&logoColor=white">
  <img alt="FastAPI" src="https://img.shields.io/badge/FastAPI-API-009688?logo=fastapi&logoColor=white">
  <img alt="SQLite" src="https://img.shields.io/badge/SQLite-banco-003B57?logo=sqlite&logoColor=white">
  <img alt="ESP32" src="https://img.shields.io/badge/ESP32-Wokwi-E7352C?logo=espressif&logoColor=white">
  <img alt="ESP32-CAM" src="https://img.shields.io/badge/ESP32--CAM-placa%20real-E7352C?logo=espressif&logoColor=white">
  <img alt="MQTT" src="https://img.shields.io/badge/MQTT-HiveMQ-660066?logo=mqtt&logoColor=white">
  <img alt="ThingSpeak" src="https://img.shields.io/badge/ThingSpeak-IoT-1F6FB2">
  <img alt="React" src="https://img.shields.io/badge/React-app-61DAFB?logo=react&logoColor=black">
  <img alt="Hetzner" src="https://img.shields.io/badge/Hetzner-VPS-D50C2D?logo=hetzner&logoColor=white">
  <img alt="Caddy" src="https://img.shields.io/badge/Caddy-HTTPS-1F88C0?logo=caddy&logoColor=white">
</p>

<p align="center">
  <a href="https://celia-frontend.vercel.app"><b>Abrir o app</b></a> ·
  <a href="https://2-29-156-251.sslip.io/docs">API no ar</a> ·
  <a href="https://drive.google.com/drive/folders/1YmK8JgwKLBA5SRyJwLv0eBwhsRu8y9nC"><b>Vídeo da ESP32-CAM</b></a> ·
  <a href="https://wokwi.com/projects/481054670710338561">Simulação no Wokwi</a> ·
  <a href="https://thingspeak.mathworks.com/channels/3577155">Canal público no ThingSpeak</a> ·
  <a href="edge/ROTEIRO_DEMO.md">Roteiro da demonstração</a> ·
  <a href="https://github.com/Isaacisrael10/celia-frontend">Repositório do app</a>
</p>

<p align="center"><i>Global Solution 2026.2 · FIAP · Engenharia de Software · tema CitizenTech</i></p>

---

## Integrantes

| Nome completo | RM | Parte no projeto |
| --- | --- | --- |
| Caio Viana de Faria | 570634 | Edge (ESP32 no Wokwi e ThingSpeak), câmera do poste e Cálculo |
| Isaac Israel Rosa Coimbra | 570072 | Front-end (app React) e Web Development |
| João Thees Castro Santiago | 572829 | Python: programa de Lógica, API de visão computacional, modelo de buracos e assistente Gemini |

## Ficha CitizenTech

| | |
| --- | --- |
| **Nome** | C.E.L.I.A. · Caminhos em Liberdade, Inclusão e Acessibilidade |
| **Cidade e bairro** | Rio de Janeiro. Os testes são no bairro do Grajaú. |
| **Cidadão beneficiado** | Pessoas com deficiência física ou mobilidade reduzida que vão ao trabalho a pé e de ônibus. E a Prefeitura, que recebe os reportes e acompanha os postes. |
| **Problema em uma frase** | A pessoa sai de casa sem saber se a calçada e o ponto de ônibus do caminho até o trabalho estão seguros, e os buracos que encontra raramente chegam a quem pode consertar. |
| **Objetivo** | Avisar a pessoa antes de sair de casa, no app e no poste do ponto de ônibus, se o caminho até o trabalho está acessível, pede atenção ou deve ser evitado, e fazer os buracos da calçada chegarem à Prefeitura para serem consertados. |
| **Como funciona** | Uma IA de visão treinada pela equipe encontra buracos nas fotos da calçada, tiradas pelo cidadão ou pela câmera do poste. Um assistente com o Gemini planeja o trajeto e escreve os reportes para a Prefeitura. Postes nos pontos de ônibus medem o calor e a luz, e o app junta tudo num mapa com a rota mais acessível. |
| **O que cada parte faz** | **IA de visão (YOLOv8n):** encontra buracos nas fotos da calçada.<br>**Assistente Gemini:** planeja o trajeto e escreve os reportes para a Prefeitura.<br>**API em Python (FastAPI e SQLite):** recebe as fotos, roda as duas IAs, guarda as capturas e os reportes e avisa o poste por MQTT.<br>**Câmera do poste (ESP32-CAM):** fotografa a calçada a cada 20 s.<br>**Poste (ESP32 no Wokwi, com DHT22, LDR e LED RGB):** mede o calor e a luz, decide o status do trecho, acende o LED e envia as leituras ao ThingSpeak.<br>**ThingSpeak:** guarda o histórico do poste e mostra os gráficos.<br>**Programa de Lógica (Python):** calcula o índice de risco de cada leitura do poste.<br>**App (React):** mostra o mapa com o status dos trechos e a rota mais acessível, recebe os reportes com foto e tem a área da Prefeitura.<br>**Cálculo:** modela o calor no ponto de ônibus com limites, derivadas e integrais. |
| **Repositório do app** | [github.com/Isaacisrael10/celia-frontend](https://github.com/Isaacisrael10/celia-frontend) |
| **Indicador de impacto** | Quantas vezes o trajeto ficou inacessível no horário de ida ao trabalho, medido no histórico do poste. |
| **ODS** | 10 (Redução das desigualdades), 11 (Cidades e comunidades sustentáveis) e 8 (Trabalho decente e crescimento econômico) |

### Como a C.E.L.I.A. contribui com os ODS

| ODS | Como a C.E.L.I.A. contribui |
| --- | --- |
| **10 · Redução das desigualdades** | A pessoa com deficiência passa a saber, antes de sair, o que hoje só descobre na rua: onde há buraco, calor forte ou rua escura, e qual é a rota mais acessível. A mini trilha de IA ajuda a diminuir a diferença no acesso ao trabalho. Meta 10.2: inclusão social e econômica de todos, independentemente de deficiência. |
| **11 · Cidades e comunidades sustentáveis** | Os postes nos pontos de ônibus monitoram o calor e a luz, a câmera encontra buracos e a Prefeitura recebe os reportes para decidir o que consertar primeiro. Metas 11.2 (transporte acessível, com atenção às pessoas com deficiência) e 11.7 (espaços públicos seguros, inclusivos e acessíveis). |
| **8 · Trabalho decente e crescimento econômico** | O projeto começa pelo trajeto até o trabalho, onde a falta de acessibilidade afasta a pessoa do emprego, e a mini trilha de IA ajuda a pessoa a permanecer nele. Meta 8.5: emprego pleno e trabalho decente para todos, inclusive pessoas com deficiência. |

## Por que a C.E.L.I.A. existe

O nome homenageia a avó Célia, que contou à equipe, por escrito, as dificuldades que enfrenta nas calçadas. A partir dessa escuta, os números públicos mostram que o trajeto até o trabalho é o lugar certo para começar:

| Dado | O que mostra | Fonte |
| --- | --- | --- |
| **14,4 milhões** | pessoas com deficiência no Brasil, 7,3% da população com 2 anos ou mais | [IBGE, Censo 2022](https://agenciadenoticias.ibge.gov.br/agencia-noticias/2012-agencia-de-noticias/noticias/43463-censo-2022-brasil-tem-14-4-milhoes-de-pessoas-com-deficiencia) |
| **26,6%** | das pessoas com deficiência em idade de trabalhar estão ocupadas; sem deficiência, são 60,7% | [IBGE, PNAD Contínua 2022](https://agenciadenoticias.ibge.gov.br/pt/agencia-noticias/37317-pessoas-com-deficiencia-tem-menor-acesso-a-educacao-ao-trabalho-e-a-renda.html) |
| **R$ 1.860** | rendimento médio de quem trabalha e tem deficiência, contra R$ 2.690 de quem não tem | [IBGE, PNAD Contínua 2022](https://agenciadenoticias.ibge.gov.br/pt/agencia-noticias/37317-pessoas-com-deficiencia-tem-menor-acesso-a-educacao-ao-trabalho-e-a-renda.html) |
| **Nota abaixo de 7** | foi a melhor média de calçada entre as 27 capitais; nenhuma chegou à nota mínima aceitável (8), em cerca de 900 locais avaliados | [Mobilize Brasil, Calçadas do Brasil 2019](https://www.mobilize.org.br/Midias/Campanhas/Calcadas-2019/relatorio-final.pdf) |
| **62,3 °C** | de sensação térmica em Guaratiba, Zona Oeste do Rio, em 17/03/2024, recorde da série medida desde 2014 | [Sistema Alerta Rio](https://www.cnnbrasil.com.br/nacional/calor-sufocante-com-623oc-rio-renova-recorde-de-sensacao-termica/) |

Menos da metade trabalha, quem trabalha ganha menos, a calçada falha no país inteiro e o calor no ponto de ônibus do Rio já passou de 60 °C. A C.E.L.I.A. ataca a parte que dá para medir todo dia: o trajeto.

## A jornada na prática

1. **Antes de sair de casa,** a pessoa abre o app e pergunta ao assistente qual é o melhor caminho até o trabalho. O assistente responde com os trechos e os locais de apoio do mapa, como banheiros acessíveis e pontos de descanso, e avisa onde a calçada está ruim.
2. **No caminho,** ela encontra um buraco e tira uma foto. A IA de visão confere a foto e marca o buraco. Só então o Gemini escreve a descrição para a Prefeitura, e a pessoa revisa antes de enviar.
3. **Sem ninguém precisar reportar,** a câmera do poste fotografa a calçada a cada 20 s. Quando a IA acha um buraco, o trecho fica vermelho no mapa, e o Gemini escreve sozinho um reporte para a Prefeitura.
4. **Na Prefeitura,** a equipe vê as últimas fotos de cada câmera com os buracos marcados, a caixa de reportes e o histórico de cada poste, e decide o que consertar primeiro.
5. **Depois de chegar,** a mini trilha de IA ajuda a pessoa a usar inteligência artificial no próprio trabalho, porque chegar ao emprego é só a primeira parte.

## A inteligência artificial no centro do projeto

A C.E.L.I.A. usa dois tipos de IA, cada uma no que faz melhor. **A IA de visão vê**: ela confere se existe mesmo um buraco na foto. **O LLM escreve e conversa**: ele transforma dados em texto claro para o cidadão e para a Prefeitura. As duas trabalham em ordem, e uma confere a outra.

| IA | O que faz | Resultado |
| --- | --- | --- |
| **Visão computacional (YOLOv8n treinado pela equipe)** | Encontra buracos nas fotos da calçada, mandadas pelo cidadão ou pela ESP32-CAM do poste. Nenhum sensor do poste consegue medir isso | v2 adotado: precisão 85,0%, recall 69,8%, mAP@50 79,9% e 6 de 6 nas fotos de calçada da equipe |
| **Assistente com o Gemini (LLM)** | Planeja o trajeto com os dados do app, escreve a descrição do reporte do cidadão (que ele revisa) e escreve sozinho o reporte da câmera do poste | Não inventa lugares (RN06), não escreve sobre buraco sem a visão confirmar e mascara CPF, telefone e e-mail |

Como o modelo foi treinado, as rotas da API e os cuidados com o Gemini estão no [README do backend](python-backend-IA/README.md).

### Mini trilha de IA (Capacite-se)

Cinco lições curtas e acessíveis, servidas pela API (`GET /trilha`): o que é inteligência artificial, como pedir bem para a IA, IA para escrever e revisar textos, IA a favor da acessibilidade e cuidados ao usar IA. É a parte do projeto que ajuda a pessoa a **permanecer** no emprego, não só a chegar até ele.

## ESP32-CAM: a câmera do poste em hardware real

<p align="center">
  <img alt="ESP32-CAM" src="https://img.shields.io/badge/ESP32--CAM-AI--Thinker-E7352C?logo=espressif&logoColor=white">
  <img alt="Arduino" src="https://img.shields.io/badge/Arduino-IDE-00878F?logo=arduino&logoColor=white">
  <img alt="Google Drive" src="https://img.shields.io/badge/V%C3%ADdeo-Google%20Drive-4285F4?logo=googledrive&logoColor=white">
</p>

<p align="center">
  Além do poste simulado no Wokwi, a equipe montou uma <b>ESP32-CAM de verdade</b>.<br>
  Ela fotografa a calçada a cada 20 s e manda a foto para a API, que procura buracos com o modelo da equipe.<br>
  Uma segunda montagem, presa numa cadeira de rodas, salva fotos de calçadas do Rio para os próximos treinos.
</p>

<p align="center">
  <a href="https://drive.google.com/drive/folders/1YmK8JgwKLBA5SRyJwLv0eBwhsRu8y9nC"><b>▶ Assistir ao vídeo da ESP32-CAM sendo montada</b></a><br>
  <sub>Firmware e detalhes em <a href="camera/README.md"><code>camera/</code></a></sub>
</p>

## As outras partes do projeto

| Parte | O que faz | Detalhes |
| --- | --- | --- |
| **Backend em Python** | API FastAPI na VPS com o modelo de buracos, o assistente Gemini, o SQLite e o MQTT. Na mesma pasta, o programa de Lógica | [`python-backend-IA/`](python-backend-IA/README.md) |
| **Edge** | ESP32 no Wokwi mede calor (DHT22) e luz (LDR), decide o status do trecho, acende o LED RGB e envia ao ThingSpeak a cada 20 s; com buraco avisado por MQTT, fica vermelho | [`edge/`](edge/README.md) |
| **Câmera do poste** | ESP32-CAM real (seção acima) | [`camera/`](camera/README.md) |
| **Cálculo** | Limites, derivadas e integrais do calor no ponto de ônibus, com dados da Open-Meteo e do canal | [`calculo/`](calculo/README.md) |
| **App** | React na Vercel: mapa, rotas, reporte com IA, assistente, locais, trilha de IA, área da Prefeitura e modo claro e escuro | [repositório do Isaac](https://github.com/Isaacisrael10/celia-frontend) |

## Stack

| Camada | Tecnologias |
| --- | --- |
| Visão computacional | Ultralytics YOLOv8n, Google Colab (GPU T4), Roboflow Universe, Wikimedia Commons |
| LLM | Google Gemini Flash (biblioteca google-genai), com três modelos em sequência |
| Backend | Python 3.13, FastAPI, Uvicorn, SQLite, Pillow |
| IoT | ESP32, DHT22, LDR, LED RGB, FreeRTOS, Wokwi, ThingSpeak, MQTT (HiveMQ, PubSubClient e paho-mqtt), ESP32-CAM |
| Lógica e Cálculo | Python, requests, json, csv, Open-Meteo |
| Front-end | React, Vite, custom hooks, MockAPI, Vercel |
| Infraestrutura | VPS Hetzner (Ubuntu), Caddy com Let's Encrypt, systemd, UFW |
| Arquitetura | IcePanel (modelo C4) |
| Testes | FastAPI TestClient, g++ com Arduino simulado |

## O que já foi validado

| Parte | Evidência |
| --- | --- |
| API na VPS | No ar com HTTPS; testada pela internet e pelo app publicado, inclusive depois de reiniciar o servidor. [Detalhes](#api-no-ar-vps) |
| Modelo de buracos | Precisão 85,0%, recall 69,8% e mAP@50 79,9% no teste; 6 de 6 nas fotos de calçada da equipe |
| Assistente Gemini | Rodadas com o Gemini real: ids corretos, sem inventar locais, recusa de pedido fora das regras e dado pessoal mascarado |
| API | 37 testes de falha (rede, Gemini fora do ar, câmera do poste, SQLite e entradas maliciosas), todos passando |
| App com IA | Foto com buraco: caixa desenhada e descrição do Gemini. Foto de caderno: nenhum reporte gerado. Gemini fora do ar: aviso com o motivo e texto padrão |
| Poste no Wokwi | Os quatro status testados com Wi-Fi e MQTT conectados. [Prints e detalhes](edge/evidencias/LEIA-ME.md) |
| ESP32-CAM | Placa real montada e gravada em vídeo. [Assistir](https://drive.google.com/drive/folders/1YmK8JgwKLBA5SRyJwLv0eBwhsRu8y9nC) |
| Firmware | 83 verificações das regras e 77 do firmware inteiro, com sensores e rede simulados |
| Programa de Lógica | 17 verificações com dados simulados |

## Experimente

1. Abra o [app](https://celia-frontend.vercel.app) e vá em **Reportar**.
2. Envie a foto de uma calçada com buraco: a IA de visão marca o buraco e o Gemini sugere a descrição, que você revisa.
3. Envie a foto de qualquer outra coisa, como um caderno: a IA não encontra buraco, e o Gemini não escreve nada.
4. Pergunte ao assistente qual o caminho mais acessível ou onde há um banheiro acessível.
5. Para ver o poste reagir, siga o [roteiro do Wokwi](edge/ROTEIRO_DEMO.md).

## API no ar (VPS)

<p align="center">
  <img alt="Hetzner" src="https://img.shields.io/badge/Hetzner-Cloud-D50C2D?logo=hetzner&logoColor=white">
  <img alt="Ubuntu" src="https://img.shields.io/badge/Ubuntu-LTS-E95420?logo=ubuntu&logoColor=white">
  <img alt="Caddy" src="https://img.shields.io/badge/Caddy-proxy-1F88C0?logo=caddy&logoColor=white">
  <img alt="Let's Encrypt" src="https://img.shields.io/badge/Let's%20Encrypt-HTTPS-003A70?logo=letsencrypt&logoColor=white">
</p>

<p align="center">
  A API com as duas IAs está publicada numa VPS e é usada pelo <a href="https://celia-frontend.vercel.app">app</a> e pela ESP32-CAM.<br>
  <a href="https://2-29-156-251.sslip.io"><b>https://2-29-156-251.sslip.io</b></a> · <a href="https://2-29-156-251.sslip.io/docs">documentação interativa</a>
</p>

```
Navegador (app na Vercel) ──HTTPS 443──► Caddy ──► API FastAPI (só escuta dentro do servidor)
                                                      ├── modelo YOLOv8 de buracos
                                                      ├── assistente Gemini
                                                      └── SQLite (capturas e reportes)
```

| Camada | Como foi feito |
| --- | --- |
| **Servidor** | VPS na Hetzner Cloud com Ubuntu LTS, 2 vCPUs e 4 GB de RAM. O modelo de IA roda no processador (PyTorch só de CPU) |
| **HTTPS** | O Caddy atende a porta 443 com um certificado do Let's Encrypt, obtido e renovado automaticamente. O nome vem do sslip.io, que transforma o IP do servidor num domínio. `http` redireciona para `https` |
| **Processo** | Um serviço do systemd liga a API quando o servidor inicia e a religa se ela cair. Ela roda com um usuário próprio, sem poderes de administrador |
| **Rede** | Firewall com só três portas abertas: SSH, HTTP e HTTPS. A porta da API fica fechada para a internet, e só o Caddy fala com ela |
| **Acesso ao servidor** | SSH só com chave criptográfica protegida por senha. Login por senha desativado |
| **Segredos** | A chave do Gemini e a chave da câmera do poste ficam só num arquivo `.env` no servidor, legível apenas pelo usuário da API. Nunca vão para o GitHub nem para o app |
| **CORS** | Só o app publicado e o ambiente local de desenvolvimento podem chamar a API pelo navegador |
| **Versões** | As bibliotecas foram instaladas nas mesmas versões que passaram nos testes da API |

**Testes do go-live (19/11/2026), feitos pela internet:** foto com buraco detectada (93% de confiança, 0,3 s no servidor), descrição escrita pelo Gemini, aviso MQTT enviado ao poste, CORS liberado só para o app, porta da API fechada para fora e volta automática de tudo depois de reiniciar o servidor. No app publicado, os quatro cenários do reporte passaram: foto com buraco, foto sem buraco (o Gemini não escreve), obstáculo e Gemini fora do ar (pop-up com texto padrão).

**O que os registros mostraram:** minutos depois de o certificado ser emitido, robôs já procuravam caminhos como `/.env` e `/.git/config`. Todos receberam 404, porque só o código da API foi enviado ao servidor, sem o repositório nem arquivos de configuração, e a API não serve nenhuma pasta.

Como atualizar a API no servidor: [README do backend](python-backend-IA/README.md#vps-como-atualizar).

## Como rodar

**API e programa de Lógica:** passo a passo no [README do backend](python-backend-IA/README.md#como-rodar-a-api).

**Testes do firmware** (com `g++`, na raiz do repositório):

```bash
g++ -std=c++17 -Wall -o tests/teste_logica tests/teste_logica.cpp && ./tests/teste_logica
g++ -std=gnu++17 -I tests/arduino_falso -o tests/teste_sketch tests/teste_sketch.cpp && ./tests/teste_sketch
```

## Estrutura do repositório

| Caminho | Conteúdo |
| --- | --- |
| [`python-backend-IA/`](python-backend-IA/README.md) | API com o modelo de buracos, o assistente Gemini e o SQLite (`api/`), treino do modelo (`treino/`) e programa de Lógica (`main.py`) |
| [`edge/`](edge/README.md) | Firmware do ESP32, circuito do Wokwi, guia do ThingSpeak e evidências |
| [`camera/`](camera/README.md) | Programas da ESP32-CAM (câmera do poste e câmera da cadeira de rodas) |
| [`calculo/`](calculo/README.md) | Modelagem com limites, derivadas e integrais |
| [`tests/`](tests/) | Testes do firmware com sensores e rede simulados |
| [`INTEGRACAO.md`](INTEGRACAO.md) | Contrato entre as partes: rotas da API, MQTT e campos do ThingSpeak |
| [`prompts.md`](prompts.md) | Registro do uso de IA (Edge, câmera e Cálculo); o do Python está em [`python-backend-IA/prompts.md`](python-backend-IA/prompts.md) |

## Privacidade e segurança

- Nenhum dado pessoal real: os dados de exemplo são simulados ou públicos, com a fonte citada.
- A foto enviada pelo app é analisada em memória e não é guardada. O Gemini nunca recebe a foto.
- Antes de qualquer texto ir ao Gemini, CPF, telefone e e-mail são mascarados.
- Chaves (Gemini, ThingSpeak, câmera) ficam só no `.env` ou na simulação em execução, nunca no repositório.

## Limitações conhecidas

- O modelo de visão encontra cerca de 7 em cada 10 buracos (recall de 69,8%). Por isso, quando ele não acha, o cidadão ainda pode escrever e enviar o reporte.
- O dataset público é de asfalto. As 38 negativas melhoraram o resultado na calçada, mas o próximo passo é treinar com fotos de calçadas do Rio, coletadas pela câmera da cadeira de rodas.
- O Gemini na cota grátis pode ficar sobrecarregado e levar alguns segundos para responder. O app mostra um contador e oferece o texto padrão se ele falhar.
- O broker MQTT público serve só para o protótipo. A versão real usaria um broker próprio com usuário, senha e TLS.
- Só o poste do ponto de ônibus é lido ao vivo. Os outros trechos do mapa são simulados, e o app avisa isso.

## Arquitetura e fluxos

<p align="center">
  <img src="assets/arquitetura/c2-conteineres-fluxo-camera.png" alt="Diagrama C4 de contêineres da C.E.L.I.A. com o fluxo da câmera do poste encontrando um buraco" width="100%">
</p>

<p align="center">
  <sub>Contêineres da C.E.L.I.A. (modelo C4, feito no IcePanel) com o fluxo "Câmera do poste encontra um buraco":<br>
  ESP32-CAM → API → modelo de buracos → SQLite e MQTT → LED vermelho no poste → ThingSpeak → app e Prefeitura.<br>
  Os diagramas de contexto e de componentes estão no <a href="python-backend-IA/README.md#arquitetura">README do backend</a>.</sub>
</p>

---

<p align="center"><sub>C.E.L.I.A. · Global Solution 2026.2 · FIAP</sub></p>
