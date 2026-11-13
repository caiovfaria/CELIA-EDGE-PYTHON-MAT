# Evidências da Etapa 4 (câmera por MQTT, lado do ESP32, 13/11/2026)

A API de visão, feita pelo João, analisa a foto e publica o resultado no `broker.hivemq.com`, tópico `celia/poste01/camera`. O texto é só `livre` ou `buraco`. Esta etapa é o lado do poste: escutar esse tópico e, com `buraco`, deixar o trecho em EVITE com motivo 5 por um tempo.

## Regras implementadas

| Mensagem | O que o poste faz |
|---|---|
| `buraco` | EVITE (status 2) com motivo 5 (buraco), na hora, por `TEMPO_BURACO_MS`: 10 min no valor da documentação (ainda em proposta), 1 min no `MODO_DEMONSTRACAO`. Um buraco novo recomeça a contagem. O OLED mostra `Buraco (camera) Ns` na linha de cima |
| `livre` | Não muda nada. Vale a regra dos sensores, e um buraco que ainda vale continua valendo |
| Qualquer outra coisa | Ignorada (no broker público, qualquer pessoa pode publicar) |
| Mensagem que chega nos 3 s depois da inscrição | Ignorada, porque pode ser uma mensagem antiga retida no broker (a mensagem não traz horário) |

Mudança combinada com a equipe: o código 4 (veículo) saiu. A API não detecta mais carros e motos, porque o modelo marcava carro estacionado.

## Como funciona no código

- **Tarefa da câmera** (desde a revisão 1 do Codex): `cicloDaCamera()` roda numa tarefa própria do FreeRTOS, a cada 100 ms, separada da tarefa de rede (ThingSpeak) e do `loop()`. Assim, um pedido HTTP lento não atrasa o aviso de buraco. `manterMqtt()` conecta no broker (tenta de novo a cada 5 s, com um nome de conexão diferente a cada vez), assina o tópico, confere se a inscrição deu certo e chama `mqtt.loop()`. Sem Wi-Fi, fecha a conexão local (`clienteMqtt.stop()`) e reconecta quando o Wi-Fi volta. Keepalive de 15 s.
- **Passagem do aviso:** `aoReceberMensagem()` só aceita exatamente `buraco` e `livre` (mesmo tamanho e mesmos bytes, com `memcmp`). Um `buraco` vira um aviso na fila de 1 lugar `filaCamera`.
- **No `loop()`:** `verCamera()` pega o aviso da fila e mostra na hora. Também percebe quando o aviso vence. A regra fica em `celia_logica.h` (`buracoValendo`, `statusComCamera`, `motivoComCamera`), que funciona mesmo quando o `millis()` volta a zero.
- **Buraco sem sensores:** o buraco vale mesmo quando os sensores estão sem leitura, na partida (a tela troca `SEM DADOS` por `EVITE` e `Motivo: buraco`) ou com os dois falhando depois. O LED fica vermelho e o envio leva só os campos 5 e 6.
- **Partida:** `iniciarEnvio()` (fila do pacote e tarefa de rede) e `iniciarCamera()` (fila do aviso e tarefa da câmera) são independentes. Se faltar memória para uma, a outra continua.

## Testes

| Arquivo | O que mostra |
|---|---|
| `serial_wokwi_revisao1_chrome_sem_wifi.txt` | Versão da revisão 1 no Wokwi (Chrome do aluno): compila, liga as duas tarefas e mede a pilha antes de conectar. O Wi-Fi do Wokwi não conectou nesse navegador |
| `serial_wokwi_chave_real.txt` | **Rodada com a chave real** (13/11, 09h40, versão anterior à revisão 1 do Codex): 7 envios aceitos pelo canal, com `buraco`, `livre`, o aviso vencendo e 50 cm na calçada. Detalhes na seção abaixo |
| `canal_3577155_registros_12a18.json` | Os registros 12 a 18 como o próprio ThingSpeak devolve (`/channels/3577155/feeds.json`) |
| `serial_wokwi_mqtt_buraco.txt` | Wokwi com os 4 arquivos idênticos aos da pasta naquele momento (conferidos por SHA-256, versão anterior à revisão 1 do Codex) e o broker real. O MQTT conecta e assina, `buraco` deixa o trecho em EVITE com motivo 5 e o LED vermelho, `livre` não muda nada, `obstaculo` é ignorado, e 60 s depois o aviso vence e o LED volta a verde. As leituras seguem a cada 5 s o tempo todo |

- **Teste da lógica** (`tests/teste_logica.cpp`): **107 verificações**. São 10 novas: validade do buraco, inclusive com o `millis()` voltando a zero; EVITE e motivo 5 nas 9 combinações de calçada e calor; o 4 não existe mais; e a linha do OLED cabe.
- **Teste do sketch** (`tests/teste_sketch.cpp`): **106 verificações** depois das revisões 1 e 2 do Codex (eram 79), todas passando, com um broker MQTT falso (`tests/arduino_falso/PubSubClient.h`). Cobrem:
  - broker e tópico certos, e nova inscrição depois que o Wi-Fi volta;
  - `buraco` na hora, com o motivo no OLED e a contagem;
  - `field5=2` e `field6=5` no ThingSpeak;
  - `livre` não apaga o buraco;
  - o aviso vence em 60 s;
  - 7 mensagens inválidas ignoradas, inclusive `buraco` seguido de um byte zero;
  - mensagem logo depois da inscrição ignorada;
  - buraco novo recomeça a contagem;
  - buraco com a calçada bloqueada: o motivo vira calçada quando o buraco vence;
  - broker fora do ar: uma tentativa a cada 5 s, sem travar;
  - falha ao criar a fila ou a tarefa da câmera, e da tarefa de rede, cada uma sem derrubar a outra;
  - os cenários novos da revisão 1 do Codex (seção abaixo).
- Com 17 erros plantados de propósito no código da câmera e no do envio, os testes pegaram os 17. Depois das revisões 1 e 2, mais 16 erros plantados nas correções novas: os testes pegaram os 16.

## Rodada com a chave real (13/11/2026)

O aluno colou a Write API Key no sketch do Wokwi, aberto no navegador embutido do app do Claude. A IA aplicou o código novo em volta da chave, rodou a simulação, publicou as mensagens e mudou a distância, sem digitar, ler ou guardar a chave. No fim, a linha voltou para `COLE_SUA_CHAVE_AQUI` e o projeto não foi salvo. Os arquivos no Wokwi foram conferidos por SHA-256 e são iguais aos da pasta.

| Registro | Gravado (UTC) | Intervalo | field4 | field5 | field6 | O que aconteceu antes |
|---|---|---|---|---|---|---|
| 12 | 12:41:29 | — | 200 | 0 | 0 | Partida, trecho verde |
| 13 | 12:41:51 | 22 s | 200 | **2** | **5** | `buraco` publicado às 12:41:42: EVITE na hora, LED vermelho |
| 14 | 12:42:14 | 23 s | 200 | 2 | 5 | `livre` publicado às 12:42:03: nada mudou |
| 15 | 12:42:36 | 22 s | 200 | 2 | 5 | Buraco ainda valendo |
| 16 | 12:42:58 | 22 s | 200 | 0 | 0 | O aviso venceu 60 s depois (t=100 s): verde de novo |
| 17 | 12:43:21 | 23 s | 200 | 0 | 0 | Ainda 200 cm |
| 18 | 12:43:44 | 23 s | **50** | **2** | **1** | 50 cm confirmado depois de 15 s: EVITE, motivo calçada |

- Os 7 registros batem, um por um, com as linhas `ThingSpeak: registro N gravado (status S, motivo M)` do serial. Temperatura, umidade e índice de calor ficaram em 26,0 °C, 50% e 26,0 °C em todos.
- Intervalo entre registros: 22 a 23 s, nunca abaixo de 20 s.
- **Sobra de pilha da tarefa de rede com envios HTTP reais:** 5648 bytes depois do 1º envio e 5588 bytes depois do 2º, de 8192 (cerca de 68% livre). Sem envio HTTP, na rodada anterior, era 5792 bytes. Os 8 KB bastam com folga.
- O aviso de buraco chegou ao canal 9 s depois da publicação (12:41:42 → 12:41:51).
- Isso também fecha a pendência da Etapa 2: envio aceito com 50 cm confirmados, LED vermelho e `field5=2`, mais a medição da pilha.

Observação: a descrição do field6 no canal ainda diz "0 nenhum, 1 calcada, 2 calor, 3 ambos". Vale acrescentar "5 buraco" nas configurações do canal (Channel Settings), que só o dono do canal muda.

## Revisão 1 do Codex (13/11/2026)

Parecer: "Etapa 4 pendente para commit pelos achados médios". O Codex não achou corrida no estado da câmera e conferiu os registros 12 a 18 no canal. Os 6 apontamentos foram corrigidos:

| Apontamento | Correção | Evidência |
|---|---|---|
| [médio] A janela de 3 s descarta alertas novos e pode aceitar uma mensagem retida se o HTTP atrasar o callback | O MQTT saiu da tarefa de rede e foi para uma tarefa só dele, que roda a cada 100 ms. Uma mensagem retida, que o broker manda logo depois da inscrição, costuma ser processada dentro da janela mesmo com um pedido HTTP lento, mas isso não é garantido: a janela vale para o momento em que a mensagem é processada. O custo de perder um buraco novo nesses 3 s ficou escrito no código e no `AGENTS.md` (na revisão 2, a janela foi aceita como filtro da demonstração). A solução definitiva, o horário dentro da mensagem, foi para a lista do João, porque muda o formato combinado | Cenário 39: uma mensagem retida chega 0,3 s depois da inscrição, com a tarefa de rede presa num pedido de 7 s, e é ignorada. Um buraco depois da janela é aceito. Cenário 35: um buraco novo logo depois da inscrição é ignorado (custo conhecido) |
| [médio] O `mqtt.loop()` fica parado enquanto o HTTP espera | Tarefa da câmera separada (`cicloDaCamera()`), com 8 KB de pilha, e keepalive de 15 s (antes 60) | Cenário 38: 3 mensagens chegam 2 s depois do início de um pedido de 7 s. O LED fica vermelho **210 ms** depois de chegar, antes de o pedido terminar |
| [médio] O resultado do `subscribe()` não era conferido | Se o pedido de inscrição não sair, o poste desconecta, avisa e tenta de novo em 5 s. Só anuncia `ouvindo a camera` depois que o pedido sai | Cenário 40: o 1º pedido não sai, o seguinte sai, e o buraco chega |
| [médio] Com os dois sensores falhando, ou na partida, o buraco não ia ao ThingSpeak (e na partida nem aparecia) | O buraco vale sozinho: em `SEM DADOS`, a tela mostra `EVITE` e `Motivo: buraco` e o LED fica vermelho. O envio sai só com os campos 5 e 6. Quando o buraco vence sem sensores, volta a `SEM DADOS` e para de enviar | Cenários 1b, 1c e 16b |
| [leve] O `strcmp()` aceitava `buraco\0x` | `mensagemIgual()` compara o tamanho e os bytes (`memcmp`) | Cenário 34: `buraco\0x` e `livre\0` ignorados |
| [leve] O MQTT falso entregava tudo de uma vez e via a queda do Wi-Fi na hora | O falso entrega uma mensagem por `loop()`, como a biblioteca. Depois de uma queda do Wi-Fi, a conexão parece viva mas não entrega nada. Mensagens publicadas com o poste desconectado se perdem. (Nesta revisão, o falso derrubava a conexão depois de 1,5 vez o keepalive. Na revisão 2, isso foi trocado pelas duas fases da biblioteca: veja a tabela abaixo.) | Cenário 41: o Wi-Fi cai por 2 s, o poste fecha a conexão e reconecta, e nada se perde. Cenário 42: veja a revisão 2 |

Rodada da nova versão no Wokwi, no Chrome do aluno (`serial_wokwi_revisao1_chrome_sem_wifi.txt`): ela **compila** e liga as duas tarefas. Sem nenhuma conexão, sobram 6652 bytes de pilha na câmera e 6820 na rede, de 8192. Sem Wi-Fi, o poste segue decidindo e força a reconexão depois de 1 minuto. Nesse Chrome, o Wi-Fi do Wokwi não conectou, então o MQTT na tarefa nova ainda precisa de uma rodada com Wi-Fi. As rodadas com o broker e com a chave real foram feitas com a versão anterior.

## Revisão 2 do Codex (13/11/2026)

Parecer: "Etapa 4 pendente para commit". O Codex conferiu os 16 arquivos por SHA-256, não achou corrida entre as tarefas e confirmou a separação do HTTP e do MQTT, a comparação exata da mensagem e o buraco sem sensores. Apontamentos:

| Apontamento | Correção | Evidência |
|---|---|---|
| [médio] A janela de 3 s ainda descarta um buraco novo logo depois da inscrição e pode aceitar um aviso antigo entregue depois dela. A API do João já publica com `retain=false`, mas a mensagem não tem horário | **Aceito pelo aluno como limitação na demonstração (13/11).** A janela fica, escrita no código, no `AGENTS.md` e no roteiro (o buraco é publicado 3 s depois do `MQTT conectado`). O horário dentro da mensagem fica como melhoria futura, a combinar com o João | Cenários 35 e 39 |
| [leve] O falso derrubava a conexão depois de 1,5 vez o keepalive; a biblioteca manda um ping depois de 15 s parada e desiste se a resposta não vier em mais 15 s | O falso imita as duas fases (ping e espera da resposta). Os "15 a 30 s" no código, no `AGENTS.md` e aqui estimam só a **detecção** da conexão morta; quanto a reconexão demora depende da rede e do broker | Cenário 42, simulado: a conexão morta foi percebida e refeita em 28 s |
| [leve] Com o Wi-Fi caído, `mqtt.disconnect()` tenta mandar um pacote pela rede antes de fechar | `clienteMqtt.stop()` fecha só a conexão local. O falso registra o `stop` | Cenário 41: um `stop` e nenhum DISCONNECT |
| [leve] O `true` do `subscribe()` só diz que o pedido saiu; a confirmação do broker vem depois | Comentário no código e cenário 40 descritos como "pedido de inscrição não saiu". A entrega de um aviso pela tarefa nova será conferida na rodada com o broker | Cenário 40 |
| [leve] `Serial.println()` faz duas escritas (texto e quebra de linha), e outra tarefa podia escrever no meio | `linhaNoSerial()` manda o texto e o `\r\n` numa escrita só, em todas as mensagens depois do `setup()`. O serial falso registra cada escrita | Cenário 43: 555 escritas, todas linhas inteiras |

## Ainda falta

- Rodada no Wokwi **com Wi-Fi** da versão da revisão 1: MQTT na tarefa da câmera e sobra de pilha das duas tarefas depois de conectar. A compilação já foi conferida.
- As respostas do João: `livre` não apaga o buraco e o estado do modelo de buracos. (O `retain=false` já está no contrato de integração, segundo o Codex. O horário dentro da mensagem ficou como melhoria futura.)
- A revisão 3 do Codex.
