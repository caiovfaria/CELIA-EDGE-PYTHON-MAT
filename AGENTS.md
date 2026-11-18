# AGENTS.md · C.E.L.I.A. (FIAP Global Solution 2026.2)

Instruções para as IAs que trabalham neste repositório (Claude e Codex). Leia antes de escrever ou revisar qualquer coisa.

## O projeto

A C.E.L.I.A. é uma caixinha com ESP32 instalada em postes perto de pontos de ônibus do Rio. Ela monitora a luz (LDR) e o calor (DHT22) e avisa um app quando o trecho fica ruim para pessoas com deficiência física ou mobilidade reduzida. O status é decidido no próprio ESP32, aparece no LED (e no monitor serial) e vai para o ThingSpeak. O resultado da análise de fotos chega por MQTT. Pelo backlog de 15/11, as fotos vêm de uma câmera no próprio poste (ESP32-CAM, pasta `camera/`), que manda uma foto a cada 20 s para a API de visão.

- **Prazo interno:** domingo, 15/11/2026.
- **Entrega oficial:** 19/11/2026, às 23h55.

## Como trabalhamos

- **Claude:** implementa e testa cada etapa.
- **Codex:** revisor independente. No fim de cada etapa, confere os arquivos e aponta problemas assim: `[grave|médio|leve] arquivo:linha — problema — correção`. Não reescreve arquivos inteiros.
- **Aluno responsável:** roda, aprova e decide. Nenhuma etapa nova começa sem a aprovação dele.
- Nada conta como pronto sem evidência (teste, print ou log).

## Pastas

| Pasta | Conteúdo | Estado |
|---|---|---|
| `edge/` | Projeto do Wokwi: `README.md`, `sketch.ino`, `celia_logica.h`, `diagram.json`, `libraries.txt`, roteiro, guia do ThingSpeak (`THINGSPEAK.md`) e evidências (`evidencias/`: circuito e os 4 status, com `LEIA-ME.md`) | Versão final (18/11): DHT22 + LDR no D34 + LED RGB; luz no field7; tabela luz × calor; câmera por MQTT; testes 83 + 77; projeto salvo no Wokwi e rodada com Wi-Fi e MQTT conectados. Só a versão atual fica no repositório: a anterior (HC-SR04 + OLED) e as evidências dela foram tiradas em 18/11, a pedido do aluno |
| `camera/` | Exemplo da câmera (`README.md`: como funciona, exemplo do monitor serial, rota da API e privacidade) e os programas da ESP32-CAM: `camera_poste/` (vídeo ao vivo e foto em 640x480 a cada 20 s para a API, com chave no cabeçalho; HU-23, RF22, RNF11) e `camera_cadeira/` (fotos no microSD para o treino do modelo, HU-06) | Decisão do aluno (18/11): a câmera fica como **exemplo explicado**. Ela funciona por IP na rede local, então não há vídeo nem foto para mostrar no repositório e ela não precisa ser regravada para a entrega. O código do poste inclui a correção da revisão do Codex (trava entre vídeo e foto e tamanho lido do próprio JPEG), testada no computador. A API do João recebe em `POST /poste/captura` (JPEG puro no corpo, cabeçalhos `X-Chave` e `X-Poste`; 201 = ok) |
| `calculo/` | Modelagem matemática (limites, derivadas e integrais) com dados públicos da Open-Meteo e os registros do canal | Relatório técnico em PDF entregue no Teams |
| `tests/` | `teste_logica.cpp` (regras) e `teste_sketch.cpp` (sketch inteiro com sensores, Wi-Fi, ThingSpeak, MQTT e FreeRTOS falsos de `arduino_falso/`) | 83 + 77 verificações passando (versão luz + calor, 18/11) |
| `visao/` | API de visão (FastAPI), modelo de buracos v2 e publicação no MQTT. Parte do João: não mexer | Contrato entre as partes em `INTEGRACAO.md` |
| `prompts.md` | Registro de uso de IA (exigido pela FIAP) | Atualizar a cada sessão |

## Decisões (não mudar sem avisar o aluno)

Os limites vêm da Documentação do Módulo Edge e **ainda estão em validação**. Por isso ficam em constantes no topo de `edge/celia_logica.h`.

| Item | Valor |
|---|---|
| Luz (LDR) | < 10 lux escuro · ≥ 25.000 lux sol direto · entre os dois, normal. Lux **inteiros, arredondando para baixo**: o número que vai para o field7 é o mesmo que decide. Conversão ADC → lux da documentação do Wokwi com 3,3 V (rl10 = 50, gamma = 0,7, resistor de 10 kΩ); média de 8 leituras do ADC. Decisão do aluno (18/11), proposta do Claude |
| Calor (índice de calor do DHT22) | < 32 °C normal · ≥ 32 °C atenção · ≥ 41 °C perigo. Decisão do aluno (15/11): o ESP32 mantém 32 e 41 °C; a RN10 do backlog propõe 30, 32 e 35 °C para o alerta do app, que alinha com o ESP32 |
| Status (field5) | 0 acessível (verde) · 1 atenção (amarelo) · 2 evite (vermelho). Sem buzzer |
| Regra do status | Calor normal: escuro → 1, normal ou sol → 0 · calor em atenção ou perigo: sol → 2, normal ou escuro → 1 · buraco → 2 |
| Motivo (field6) | 0 nenhum · 1 luz · 2 calor · 3 luz e calor · 5 buraco (câmera). A luz só tem culpa quando piorou o status (escuro sempre; sol só junto com calor). O 4 (veículo) foi retirado pela equipe em 12/11 (o modelo marcava carro estacionado) |
| Falha de sensor | Sem leitura do DHT22 na partida: serial "SEM DADOS" e LED apagado. Falha depois: mantém o último valor e avisa no serial. O LDR sempre dá leitura (um fio solto no AO não é detectado: limitação conhecida). Um buraco da câmera vale mesmo sem sensores: EVITE, LED vermelho e motivo 5 |
| Tempos | Leitura a cada 5 s · mudança na luz confirmada após 1 min (15 s com `MODO_DEMONSTRACAO`) · calor sem confirmação · ThingSpeak a cada 20 s · foto vale 10 min |
| ThingSpeak | Canal público `GS-EDGE-C.E.L.I.A.`, ID `3577155`. field1 temperatura · field2 umidade · field3 índice de calor · field4 não usado pelo poste (o Python do João ainda espera esse campo e pula registros sem ele) · field5 status · field6 motivo · field7 luminosidade em lux. Projeto do Wokwi: https://wokwi.com/projects/481054670710338561 |
| Envio (Etapa 2) | HTTP simples (`api.thingspeak.com/update`) pelo Wi-Fi `Wokwi-GUEST` (canal 6). O envio roda numa **tarefa de rede** do FreeRTOS, separada do `loop()`. O `loop()` só lê, decide e mostra, e deixa um pacote com os dados numa fila de 1 lugar (`xQueueOverwrite`). A tarefa pega o pacote mais novo (`xQueueReceive`) e envia. Próximo envio 20 s depois do **fim** do anterior. A fila e a tarefa são criadas em `iniciarEnvio()`. Se faltar memória, o poste avisa no serial e segue sem envio. Se a tarefa não for criada, a fila é liberada (`vQueueDelete`) e volta a `NULL`. Cada registro leva a última leitura antes do envio (até ~5 s antes), e o field5 é o status dessa leitura: o LED pode mudar durante um pedido lento, e a mudança vai no envio seguinte. A tarefa mede a sobra da pilha (`uxTaskGetStackHighWaterMark`) e avisa quando ela diminui. Não envia: com "SEM DADOS" (a não ser que haja um buraco valendo: aí vão a luz e os campos 5 e 6), sem leitura nova, com a chave de exemplo ou sem Wi-Fi. O ESP32 reconecta sozinho; depois de 1 min sem conexão, o sketch força `WiFi.reconnect()`, no máximo uma vez por minuto. Se o DHT22 falhou, os campos 1 a 3 ficam de fora; a luz (field7) vai sempre. Limites por operação: 5 s para conectar e 5 s para a resposta. A busca do endereço (DNS) não tem limite próprio, então não há teto total para o pedido. Como ele roda na tarefa de rede, isso não trava o poste. A chave nunca aparece no serial. Cada linha do serial sai numa escrita só (`linhaNoSerial()`: texto e quebra de linha juntos), para as tarefas não cortarem a linha uma da outra. HTTPS com certificado fica como melhoria (`edge/THINGSPEAK.md`) |
| Chave no Wokwi | O projeto salvo no Wokwi é público: fica sempre com `COLE_SUA_CHAVE_AQUI`. A chave entra só enquanto a simulação roda, sem SAVE. Se ela ficar salva no projeto público, gere uma nova no ThingSpeak |
| Pendente com a equipe | (1) Índice de calor fora da faixa da fórmula (ex.: 80 °C e 100% → 708,7 °C): não enviar o field3 nesses casos e como sinalizar "calor extremo" ao app (Codex, revisão 1). (2) Se o app precisar do horário exato da leitura, e não só do horário da gravação no ThingSpeak, mandar esse horário num campo extra (Codex, revisão 2) |
| MQTT (Etapa 4) | `broker.hivemq.com:1883`, tópico `celia/poste01/camera`, biblioteca PubSubClient. A API de visão (do João) publica só o texto `livre` ou `buraco`, sem horário. Qualquer outra mensagem é ignorada, porque o broker é público. `buraco` → EVITE (2) com motivo 5 por `TEMPO_BURACO_MS` (10 min, proposta da documentação; 1 min no `MODO_DEMONSTRACAO`). Um buraco novo recomeça a contagem. `livre` não muda nada e não apaga um buraco que ainda vale. Inscrição com QoS 1. O MQTT roda numa **tarefa da câmera**, separada da tarefa de rede (`cicloDaCamera()` a cada 100 ms, 8 KB de pilha): um pedido HTTP lento não atrasa o aviso. A tarefa passa o aviso ao `loop()` por uma fila de 1 lugar (`filaCamera`), e o `loop()` mostra na hora (`verCamera()`). A mensagem precisa ser exatamente `buraco` ou `livre` (tamanho e bytes, com `memcmp`). Como a mensagem não traz horário, todo aviso processado nos 3 s seguintes à inscrição é ignorado (pode ser mensagem retida). É um filtro da demonstração: um aviso antigo processado depois dos 3 s seria aceito. Custo conhecido: um buraco novo publicado nesses 3 s se perde (só na partida e depois de uma queda). **Decisão do aluno (13/11): a janela fica, aceita como limitação na demonstração** (revisão 2 do Codex). Na demo, o buraco é publicado depois de 3 s do `MQTT conectado`. Se o pedido de inscrição não sair (`subscribe()` devolve false), o poste desconecta e tenta de novo em 5 s (o `true` só diz que o pedido saiu; a confirmação do broker se vê quando um aviso chega). Sem Wi-Fi, fecha a conexão local (`clienteMqtt.stop()`, sem mandar nada pela rede caída) e reconecta quando o Wi-Fi volta. Keepalive de 15 s: o ping sai depois de 15 s parado e a biblioteca desiste se a resposta não vier em mais 15 s, então uma conexão morta sem aviso é percebida entre 15 e 30 s (a reconexão depois disso depende da rede e do broker). Reconexão ao broker a cada 5 s, com um nome de conexão diferente a cada vez. O buraco vale mesmo sem leitura do DHT22 (na partida): EVITE, LED vermelho e envio da luz e dos campos 5 e 6. `iniciarEnvio()` e `iniciarCamera()` são independentes: se faltar memória para uma, a outra segue funcionando. Limitação conhecida: no broker público, qualquer um pode publicar `buraco` |
| Pendente com o João | (1) Retain desligado: já está no contrato de integração (`INTEGRACAO.md` do repositório GS-C.E.L.I.A.-EDGE, segundo o Codex). (2) `livre`: o contrato diz que não força nada e vale a regra dos sensores, igual ao ESP32 (não apaga um buraco que ainda vale). (3) Modelo de buracos: o v2 do João está em `visao/api/modelos/buracos.pt` (decisão de 12/11). (4) Melhoria futura: incluir no texto da mensagem o horário da detecção ou da validade, para o poste descartar aviso velho sem a janela de 3 s (Codex, revisões 1 e 2 da Etapa 4). Na demonstração, a janela foi aceita (13/11). (5) O `INTEGRACAO.md` ainda descreve o field4 como distância e o motivo 1 como calçada; o Python dele pula registros sem field4 e o app chama o motivo 1 de "calçada". Para usarem a luz, precisam ler o field7 e o motivo 1 como luz (o arquivo e o código são do João e do Isaac: não mexer) |

## Circuito (Wokwi)

O projeto usa só peças nativas do Wokwi (`wokwi-esp32-devkit-v1`, `wokwi-dht22`, `wokwi-photoresistor-sensor`, `wokwi-rgb-led`, `wokwi-resistor`). As peças `board-*` carregam de um site externo (github.io), que falhou no primeiro teste e de novo em 18/11. As nativas não dependem dele.

| Peça | Pinos do ESP32 |
|---|---|
| DHT22 | dados D15 · VCC 3V3 · GND |
| Módulo de luz (LDR) | AO D34 (ADC1: o ADC2 não funciona com o Wi-Fi) · VCC 3V3 · GND · DO sem uso |
| LED RGB (catodo comum) | R D25 e G D26, cada um com resistor de 220 Ω · COM no GND. Amarelo = R + G |

## Regras

- Os documentos oficiais da FIAP e da C.E.L.I.A. valem mais que qualquer suposição.
- Código simples, de 1º ano, comentado em português. A equipe precisa saber explicar cada parte.
- A lógica de decisão fica em `celia_logica.h`, sem nada de hardware, para poder ser testada no computador. Se mudar uma regra ou o sketch, atualize e rode os dois testes.
- Tempos do confirmador em `uint32_t` (32 bits, como o `millis()` do ESP32).
- Nenhuma chave no repositório: a chave do ThingSpeak fica como `COLE_SUA_CHAVE_AQUI` (o aluno cola a dele só no Wokwi). As IAs não recebem nem digitam a chave. O mesmo vale para a chave da câmera (`COLE_A_CHAVE_DA_CAMERA_AQUI`) e para o nome e a senha do Wi-Fi nos programas da ESP32-CAM.
- Nada de dados pessoais reais. Fotos sem rostos nem placas.
- Registre no `prompts.md` os prompts das duas IAs (data, qual IA, pedido, resultado).
- Não mexa na parte de outros integrantes.

## Como testar

```
g++ -std=c++17 -Wall -o teste_logica tests/teste_logica.cpp && ./teste_logica
g++ -std=gnu++17 -I tests/arduino_falso -o teste_sketch tests/teste_sketch.cpp && ./teste_sketch
```
