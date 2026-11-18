# Roteiro de demonstração (Wokwi e ThingSpeak)

Como mostrar, no Wokwi, que o poste decide sozinho o status do trecho e manda tudo para o canal público do ThingSpeak.

**Durante a simulação, clique no sensor para abrir os controles:**
- no **módulo de luz (LDR)**, você muda a luz em lux (o controle é em escala logarítmica: arraste até aparecer o número da tabela);
- no **DHT22**, você muda a temperatura e a umidade.

**Deixe a umidade em 50% o tempo todo.** Os valores de índice de calor abaixo foram calculados com a mesma fórmula da biblioteca e conferidos no teste `tests/teste_logica.cpp`.

**Tempos no modo demonstração:**
- o ESP32 lê os sensores a cada 5 s e escreve uma linha no monitor serial;
- uma mudança na luz só vale depois de 15 s seguidos (o valor real é 1 minuto: veja `MODO_DEMONSTRACAO` no `sketch.ino`);
- enquanto conta, a linha do serial termina com `confirmando luz Ns`;
- mudanças de calor aparecem já na leitura seguinte.

| Passo | Luz | DHT22 (temperatura) | Índice de calor | Espere | LED | Serial (status · motivo) |
|---|---|---|---|---|---|---|
| 1 | 500 lux | 26 °C | 26,0 °C | — | Verde | ACESSIVEL · nenhum |
| 2 | 5 lux | 26 °C | 26,0 °C | ~20 s | Amarelo | ATENCAO · luz (rua escura) |
| 3 | 5 lux | 33 °C | 36,3 °C | até 5 s | Amarelo | ATENCAO · luz+calor |
| 4 | 500 lux | 33 °C | 36,3 °C | ~20 s | Amarelo | ATENCAO · calor |
| 5 | 50.000 lux | 33 °C | 36,3 °C | ~20 s | Vermelho | EVITE · luz+calor (sol direto com calor) |
| 6 | 50.000 lux | 36 °C | 43,1 °C | até 5 s | Vermelho | EVITE · luz+calor (calor em perigo) |
| 7 | 50.000 lux | 26 °C | 26,0 °C | até 5 s | Verde | ACESSIVEL · nenhum (sol sozinho não piora o trecho) |

**Farol passando (não pode mudar o status):**
1. Com a rua escura já confirmada (passo 2, amarelo), coloque a luz em 500 lux.
2. Espere aparecer `confirmando luz` no serial.
3. Volte para 5 lux antes de 15 s.

O status continua ATENCAO · luz. Isso mostra que uma luz rápida (um farol, uma lanterna) não muda o trecho.

## Prints para o README

Tire um print do Wokwi (circuito, LED e monitor serial) em cada uma destas situações. Os prints prontos estão em [`evidencias/`](evidencias/LEIA-ME.md).

1. Passo 1 (verde)
2. Passo 2 (amarelo pela rua escura)
3. Passo 4 (amarelo pelo calor)
4. Passo 5 (vermelho por sol e calor)

## Se algo não bater

- **LED apagado na partida, com `SEM DADOS: aguardando a primeira leitura valida do DHT22` no serial:** o DHT22 ainda não respondeu. É o comportamento esperado: sem leitura, o poste não decide (na dúvida, não fica verde). Se a câmera avisar um buraco nessa hora, o LED fica vermelho: o buraco não depende dos sensores.
- **LED apagado depois da partida:** confira se o LED RGB está como catodo comum (`"common": "cathode"` no `diagram.json`).
- **"Aviso: falha ao ler o DHT22; mantendo a leitura anterior."** O DHT22 falhou na última leitura e o poste segue com o último valor válido.
- **A temperatura não muda no serial depois de mexer no DHT22:** numa rodada de 18/11, a mudança feita com a simulação rodando não chegou ao poste (a da luz chegava). Pare a simulação, mude a temperatura no DHT22 e dê play de novo.
- **Luz perto de 25.000 lux:** cada passo do ADC vale uns 450 lux ali, então 25.000 pode virar 24.958 (normal). Use 50.000 lux para mostrar o sol direto.

## Dados no ThingSpeak

Antes, cole a Write API Key no `sketch.ino` do Wokwi, como explica o `THINGSPEAK.md`. A chave entra só enquanto a simulação roda: **não salve o projeto com ela**, nem mostre essa linha nos prints. A chave nunca vai para o GitHub.

1. Abra o canal público em outra aba: https://thingspeak.mathworks.com/channels/3577155
2. Rode a simulação com a aba do Wokwi na frente. Em 20 a 40 s, o monitor serial mostra `Wi-Fi conectado.` e depois `ThingSpeak: registro N gravado (status 0, motivo 0, 499 lux)`.
3. Repita os passos 1 a 7 da tabela acima. A cada 20 s sai um envio, e os gráficos do canal acompanham:
   - field7 (luminosidade) cai no passo 2 e sobe nos passos 4 e 5;
   - field3 (índice de calor) sobe nos passos 3 e 6;
   - field5 (status) acompanha a cor do LED: 0 verde, 1 amarelo, 2 vermelho. Cada envio leva a última leitura antes dele, então a mudança aparece no envio seguinte;
   - field6 (motivo) mostra 1 (luz), 2 (calor), 3 (os dois) ou 5 (buraco visto pela câmera).
4. **Sem internet não trava:** o envio roda numa tarefa separada. O poste continua decidindo e mostrando no LED, com uma leitura a cada 5 s. O serial avisa `sem Wi-Fi, envio pulado` e tenta reconectar no próximo envio.
5. Quando terminar, volte a linha da chave para `COLE_SUA_CHAVE_AQUI` antes de salvar o projeto.

O canal recebe no máximo um envio a cada 15 s. Não rode duas abas do Wokwi com a mesma chave ao mesmo tempo: os envios se atropelam e o ThingSpeak recusa parte deles.

**Prints do ThingSpeak:**

1. Monitor serial com `Wi-Fi conectado.` e `ThingSpeak: registro ... gravado`
2. Página do canal com os gráficos de luminosidade, índice de calor e status mudando
3. Wokwi com o LED vermelho e, ao lado, o gráfico do field5 em 2 no mesmo momento

## Buraco visto pela câmera (MQTT)

O poste escuta o tópico `celia/poste01/camera` no `broker.hivemq.com`, numa tarefa só para a câmera (no serial, `Tarefa da camera ligada`). Assim, o aviso de buraco chega mesmo durante um envio lento ao ThingSpeak. Para mostrar sem depender da API de visão, publique a mensagem você mesmo:

1. Com a simulação rodando e o trecho verde, espere aparecer `MQTT conectado: ouvindo a camera` no monitor serial. Espere mais uns 3 s: o que chega logo depois da conexão é ignorado, porque pode ser uma mensagem antiga guardada no broker.
2. Abra o [cliente web do HiveMQ](https://www.hivemq.com/demos/websocket-client/) e clique em **Connect**, com as opções padrão.
3. Em **Publish**, escreva o tópico `celia/poste01/camera`, a mensagem `buraco` (minúsculo, sem espaço) e clique em **Publish**. Deixe o **Retain** desmarcado.
4. Em 1 ou 2 segundos, o LED fica vermelho. O serial mostra `Camera: buraco recebido.`, e as linhas de leitura passam a terminar com `STATUS 2 EVITE | motivo 5 buraco | buraco (camera) 60s`, contando.
5. Publique `livre`: nada muda (o buraco continua valendo).
6. Depois de 60 s (modo demonstração; 10 min no valor real), o serial mostra `o aviso de buraco venceu` e o trecho volta à regra dos sensores.

Com a chave do ThingSpeak colada, o envio seguinte leva `field5=2` e `field6=5` enquanto o buraco vale.

## Bordas da luz (para conferir no simulador)

A luz é calculada em lux inteiros, arredondando para baixo. O número que aparece no serial e vai para o field7 é o mesmo que decide o status. Valores seguros, longe dos passos do ADC:

| Luz no Wokwi | Resultado |
|---|---|
| 9 lux | escuro |
| 11 lux | normal |
| 20.000 lux | normal |
| 30.000 lux | sol direto |
