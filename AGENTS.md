# AGENTS.md · C.E.L.I.A. (FIAP Global Solution 2026.2)

Instruções para as IAs que trabalham neste repositório (Claude e Codex). Leia antes de escrever ou revisar qualquer coisa.

## O projeto

A C.E.L.I.A. é uma caixinha com ESP32 instalada em postes perto de pontos de ônibus do Rio. Ela monitora a calçada (HC-SR04) e o calor (DHT22) e avisa um app quando o trecho fica ruim para pessoas com deficiência física ou mobilidade reduzida. O status é decidido no próprio ESP32, aparece no OLED e no LED e vai para o ThingSpeak. O resultado da análise de fotos (YOLO) chega por MQTT.

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
| `edge/` | Projeto do Wokwi: `sketch.ino`, `celia_logica.h`, `diagram.json`, `libraries.txt`, roteiro, guia do ThingSpeak (`THINGSPEAK.md`) e evidências | Etapa 1 aprovada pelo Codex (revisão 4) e pelo aluno, no commit `13fab0d` (12/11). Etapa 2 (ThingSpeak): 11 registros reais no canal 3577155 na primeira versão. A revisão 1 do Codex pediu 4 correções médias, já feitas: rede numa tarefa separada, 20 s contados do fim, nenhum envio com os dois sensores falhando e a chave fora do projeto público. A revisão 2 pediu mais 2 correções médias, também feitas: conferir a criação da fila e da tarefa, e o field5 representa a leitura capturada. Também 2 leves: medir a pilha e reconectar só depois de 1 min. Revisão 3 concluída e ajustes leves aplicados. A rodada com a chave real (envio aceito com status 2 e medição da pilha) foi feita em 13/11, junto com a Etapa 4. **Etapa 2 no commit `253646f`.** Etapa 4 (lado do ESP32): recepção da câmera por MQTT escrita, testada no computador e no Wokwi com o broker real (`edge/evidencias/etapa4/`); rodada com a chave real feita em 13/11 (registros 12 a 18: `field5=2`/`field6=5` com buraco e `field5=2`/`field6=1` com 50 cm; pilha com 5588 bytes sobrando), que também fecha a pendência da Etapa 2. A revisão 1 do Codex pediu 4 correções médias e 2 leves, já feitas: MQTT numa tarefa própria (a câmera não espera o HTTP), inscrição conferida, buraco independente dos sensores (inclusive no envio) e comparação exata da mensagem. Também: o cliente MQTT falso entrega uma mensagem por volta e imita conexão morta sem aviso. A revisão 2 do Codex deixou 1 médio (a janela de 3 s), aceito pelo aluno como limitação na demonstração, e 4 leves, já corrigidos: keepalive em duas fases no MQTT falso, `clienteMqtt.stop()` quando o Wi-Fi cai, `subscribe()` descrito como envio do pedido, e cada linha do serial numa escrita só. **Etapa 4 no commit `8de554d`**; rodada final no Wokwi com Wi-Fi no commit `93995f1` (buraco recebido pela tarefa da câmera, EVITE com motivo 5 e volta a ACESSIVEL depois de 60 s; pilha com 5864 bytes sobrando na câmera e 6276 na rede). Leituras de 15/11 (registros 19 a 39, nas três faixas, para o programa Python) no commit `2d8e783`. Pendente: teste de ponta a ponta com a API de visão |
| `tests/` | `teste_logica.cpp` (regras) e `teste_sketch.cpp` (sketch inteiro com sensores, Wi-Fi, ThingSpeak e FreeRTOS falsos de `arduino_falso/`) | 107 + 106 verificações passando |
| `visao/` | API de visão (FastAPI), modelo de buracos v2 e publicação no MQTT. Parte do João: não mexer | Contrato entre as partes em `INTEGRACAO.md` |
| `prompts.md` | Registro de uso de IA (exigido pela FIAP) | Atualizar a cada sessão |

## Decisões (não mudar sem avisar o aluno)

Os limites vêm da Documentação do Módulo Edge e **ainda estão em validação**. Por isso ficam em constantes no topo de `edge/celia_logica.h`.

| Item | Valor |
|---|---|
| Calçada (distância livre, HC-SR04) | ≥ 120 cm livre · ≥ 90 cm estreita · < 90 cm bloqueada. A regra vale para **centímetros inteiros, arredondando para baixo** (119,6 cm = 119 = estreita; 89,6 cm = 89 = bloqueada): na dúvida, a faixa pior |
| Calor (índice de calor do DHT22) | < 32 °C normal · ≥ 32 °C atenção · ≥ 41 °C perigo (pendente: a equipe pode trocar para 39 °C) |
| Status (field5) | 0 acessível (verde) · 1 atenção (amarelo) · 2 evite (vermelho). Sem buzzer |
| Regra do status | Bloqueada → 2 · estreita + perigo → 2 · estreita + normal/atenção → 1 · livre + atenção/perigo → 1 · livre + normal → 0 |
| Motivo (field6) | 0 nenhum · 1 calçada · 2 calor · 3 calçada e calor · 5 buraco (câmera). O 4 (veículo) foi retirado pela equipe em 12/11 (o modelo marcava carro estacionado) |
| HC-SR04 | Distância = eco (µs) ÷ 58,8, cortada para cm inteiros. O valor 58,8 µs/cm foi medido em 28 ecos do Wokwi, de 50 a 400 cm, com variação de ±3 µs (`edge/evidencias/calibracao_hc_sr04.md`). Tolerância efetiva: 0,08 cm (0,8 mm), que cobre a maior falta medida (0,054 cm) com cerca de 1,5 µs de sobra. Bordas: 7051 µs = 119 (estreita) e 7052 µs = 120 (livre); 5287 µs = 89 (bloqueada) e 5288 µs = 90 (estreita) |
| Falha de sensor | Sem leitura válida na partida: OLED "SEM DADOS" e LED apagado. Falha depois: mantém o último valor, avisa no OLED e cancela a confirmação da calçada em andamento. Um buraco da câmera vale mesmo sem sensores: EVITE, LED vermelho e motivo 5 |
| Tempos | Leitura a cada 5 s · mudança na calçada confirmada após 1 min (15 s com `MODO_DEMONSTRACAO`) · ThingSpeak a cada 20 s · foto vale 10 min |
| ThingSpeak | Canal público `GS-EDGE-C.E.L.I.A.`, ID `3577155`. field1 temperatura · field2 umidade · field3 índice de calor · field4 distância · field5 status · field6 motivo. Projeto do Wokwi: https://wokwi.com/projects/480457503878659073 |
| Envio (Etapa 2) | HTTP simples (`api.thingspeak.com/update`) pelo Wi-Fi `Wokwi-GUEST` (canal 6). O envio roda numa **tarefa de rede** do FreeRTOS, separada do `loop()`. O `loop()` só lê, decide e mostra, e deixa um pacote com os dados numa fila de 1 lugar (`xQueueOverwrite`). A tarefa pega o pacote mais novo (`xQueueReceive`) e envia. Próximo envio 20 s depois do **fim** do anterior. A fila e a tarefa são criadas em `iniciarEnvio()`. Se faltar memória, o poste avisa no serial e segue sem envio. Se a tarefa não for criada, a fila é liberada (`vQueueDelete`) e volta a `NULL`. Cada registro leva a última leitura antes do envio (até ~5 s antes), e o field5 é o status dessa leitura: o LED pode mudar durante um pedido lento, e a mudança vai no envio seguinte. A tarefa mede a sobra da pilha (`uxTaskGetStackHighWaterMark`) e avisa quando ela diminui. Não envia: com "SEM DADOS" ou com os dois sensores falhando (a não ser que haja um buraco valendo: aí vão só os campos 5 e 6), sem leitura nova, com a chave de exemplo ou sem Wi-Fi. O ESP32 reconecta sozinho; depois de 1 min sem conexão, o sketch força `WiFi.reconnect()`, no máximo uma vez por minuto. Se um só sensor falhou, os campos dele ficam de fora. Limites por operação: 5 s para conectar e 5 s para a resposta. A busca do endereço (DNS) não tem limite próprio, então não há teto total para o pedido. Como ele roda na tarefa de rede, isso não trava o poste. A chave nunca aparece no serial. Cada linha do serial sai numa escrita só (`linhaNoSerial()`: texto e quebra de linha juntos), para as tarefas não cortarem a linha uma da outra. HTTPS com certificado fica como melhoria (`edge/THINGSPEAK.md`) |
| Chave no Wokwi | O projeto salvo no Wokwi é público: fica sempre com `COLE_SUA_CHAVE_AQUI`. A chave entra só enquanto a simulação roda, sem SAVE. Se ela ficar salva no projeto público, gere uma nova no ThingSpeak |
| Pendente com a equipe | (1) Índice de calor fora da faixa da fórmula (ex.: 80 °C e 100% → 708,7 °C): não enviar o field3 nesses casos e como sinalizar "calor extremo" ao app (Codex, revisão 1). (2) Se o app precisar do horário exato da leitura, e não só do horário da gravação no ThingSpeak, mandar esse horário num campo extra (Codex, revisão 2) |
| MQTT (Etapa 4) | `broker.hivemq.com:1883`, tópico `celia/poste01/camera`, biblioteca PubSubClient. A API de visão (do João) publica só o texto `livre` ou `buraco`, sem horário. Qualquer outra mensagem é ignorada, porque o broker é público. `buraco` → EVITE (2) com motivo 5 por `TEMPO_BURACO_MS` (10 min, proposta da documentação; 1 min no `MODO_DEMONSTRACAO`). Um buraco novo recomeça a contagem. `livre` não muda nada e não apaga um buraco que ainda vale. O MQTT roda numa **tarefa da câmera**, separada da tarefa de rede (`cicloDaCamera()` a cada 100 ms, 8 KB de pilha): um pedido HTTP lento não atrasa o aviso. A tarefa passa o aviso ao `loop()` por uma fila de 1 lugar (`filaCamera`), e o `loop()` mostra na hora (`verCamera()`). A mensagem precisa ser exatamente `buraco` ou `livre` (tamanho e bytes, com `memcmp`). Como a mensagem não traz horário, todo aviso processado nos 3 s seguintes à inscrição é ignorado (pode ser mensagem retida). É um filtro da demonstração: um aviso antigo processado depois dos 3 s seria aceito. Custo conhecido: um buraco novo publicado nesses 3 s se perde (só na partida e depois de uma queda). **Decisão do aluno (13/11): a janela fica, aceita como limitação na demonstração** (revisão 2 do Codex). Na demo, o buraco é publicado depois de 3 s do `MQTT conectado`. Se o pedido de inscrição não sair (`subscribe()` devolve false), o poste desconecta e tenta de novo em 5 s (o `true` só diz que o pedido saiu; a confirmação do broker se vê quando um aviso chega). Sem Wi-Fi, fecha a conexão local (`clienteMqtt.stop()`, sem mandar nada pela rede caída) e reconecta quando o Wi-Fi volta. Keepalive de 15 s: o ping sai depois de 15 s parado e a biblioteca desiste se a resposta não vier em mais 15 s, então uma conexão morta sem aviso é percebida entre 15 e 30 s (a reconexão depois disso depende da rede e do broker). Reconexão ao broker a cada 5 s, com um nome de conexão diferente a cada vez. O buraco vale mesmo sem leitura dos sensores (na partida ou com os dois falhando): EVITE, LED vermelho e envio dos campos 5 e 6. `iniciarEnvio()` e `iniciarCamera()` são independentes: se faltar memória para uma, a outra segue funcionando. Limitação conhecida: no broker público, qualquer um pode publicar `buraco` |
| Pendente com o João | (1) Retain desligado: já está no contrato de integração (`INTEGRACAO.md` do repositório GS-C.E.L.I.A.-EDGE, segundo o Codex). (2) `livre`: o contrato diz que não força nada e vale a regra dos sensores, igual ao ESP32 (não apaga um buraco que ainda vale). (3) Modelo de buracos: o v2 do João está em `visao/api/modelos/buracos.pt` (decisão de 12/11). (4) Melhoria futura: incluir no texto da mensagem o horário da detecção ou da validade, para o poste descartar aviso velho sem a janela de 3 s (Codex, revisões 1 e 2 da Etapa 4). Na demonstração, a janela foi aceita (13/11) |

## Circuito (Wokwi)

O projeto usa só peças nativas do Wokwi (`wokwi-esp32-devkit-v1`, `wokwi-ssd1306`, `wokwi-dht22`, `wokwi-hc-sr04`, `wokwi-rgb-led`, `wokwi-resistor`). As peças `board-*` carregam de um site externo (github.io), que falhou no primeiro teste. As nativas não dependem dele.

| Peça | Pinos do ESP32 |
|---|---|
| DHT22 | dados D15 · VCC 3V3 · GND |
| HC-SR04 | TRIG D5 · ECHO D18 · VCC VIN · GND |
| OLED SSD1306 (I2C, 0x3C) | DATA (SDA) D21 · CLK (SCL) D22 · VIN 3V3 · GND |
| LED RGB (catodo comum) | R D25 e G D26, cada um com resistor de 220 Ω · COM no GND. Amarelo = R + G |

## Regras

- Os documentos oficiais da FIAP e da C.E.L.I.A. valem mais que qualquer suposição.
- Código simples, de 1º ano, comentado em português. A equipe precisa saber explicar cada parte.
- A lógica de decisão fica em `celia_logica.h`, sem nada de hardware, para poder ser testada no computador. Se mudar uma regra ou o sketch, atualize e rode os dois testes.
- Tempos do confirmador em `uint32_t` (32 bits, como o `millis()` do ESP32).
- Nenhuma chave no repositório: a chave do ThingSpeak fica como `COLE_SUA_CHAVE_AQUI` (o aluno cola a dele só no Wokwi). As IAs não recebem nem digitam a chave.
- Nada de dados pessoais reais. Fotos sem rostos nem placas.
- Registre no `prompts.md` os prompts das duas IAs (data, qual IA, pedido, resultado).
- Não mexa na parte de outros integrantes.

## Como testar

```
g++ -std=c++17 -Wall -o teste_logica tests/teste_logica.cpp && ./teste_logica
g++ -std=gnu++17 -I tests/arduino_falso -o teste_sketch tests/teste_sketch.cpp && ./teste_sketch
```
