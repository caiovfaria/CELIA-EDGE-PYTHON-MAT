# Roteiro de demonstração · Etapas 1 e 2 (Wokwi e ThingSpeak)

Como mostrar, no Wokwi, que o poste decide sozinho o status do trecho e manda tudo para o canal público do ThingSpeak.

**Durante a simulação, clique no sensor para abrir os controles:**
- no **DHT22**, você muda a temperatura e a umidade;
- no **HC-SR04**, você muda a distância.

**Deixe a umidade em 50% o tempo todo.** Os valores de índice de calor abaixo foram calculados com a mesma fórmula da biblioteca e conferidos no teste `tests/teste_logica.cpp`.

**Tempos no modo demonstração:**
- o ESP32 lê os sensores a cada 5 s;
- uma mudança na calçada só vale depois de 15 s seguidos (o valor real é 1 minuto: veja `MODO_DEMONSTRACAO` no `sketch.ino`);
- enquanto conta, a linha de cima do OLED mostra `Confirmando... Ns`;
- mudanças de calor aparecem já na leitura seguinte.

| Passo | DHT22 (temperatura) | HC-SR04 (distância) | Índice de calor | Espere | LED | OLED (status · motivo) |
|---|---|---|---|---|---|---|
| 1 | 26 °C | 200 cm | 26,0 °C | — | Verde | ACESSIVEL · nenhum |
| 2 | 32 °C | 200 cm | 34,4 °C | até 5 s | Amarelo | ATENCAO · calor |
| 3 | 38 °C | 200 cm | 48,6 °C | até 5 s | Amarelo | ATENCAO · calor (calor em perigo, mas a calçada está livre) |
| 4 | 26 °C | 100 cm | 26,0 °C | ~20 s | Amarelo | ATENCAO · calcada |
| 5 | 38 °C | 100 cm | 48,6 °C | até 5 s | Vermelho | EVITE · calcada+calor |
| 6 | 26 °C | 50 cm | 26,0 °C | ~20 s | Vermelho | EVITE · calcada |
| 7 | 26 °C | 200 cm | 26,0 °C | ~20 s | Verde | ACESSIVEL · nenhum (trecho liberado) |

**Pessoa passando (não pode mudar o status):**
1. Com o trecho verde, coloque a distância em 50 cm.
2. Espere aparecer `Confirmando...` no OLED.
3. Volte para 200 cm antes de 15 s.

O status continua ACESSIVEL. Isso mostra que uma obstrução rápida não deixa o trecho vermelho.

## Prints para o README

Tire um print do Wokwi inteiro (circuito, OLED, LED e monitor serial) em cada uma destas situações:

1. Passo 1 (verde)
2. Passo 3 (amarelo por calor)
3. Pessoa passando, com `Confirmando...` aparecendo
4. Passo 5 (vermelho por calçada e calor)
5. Passo 6 (vermelho por calçada)
6. Passo 7 (liberado)

O monitor serial mostra uma linha por leitura (`t=...s | T ... | faixa ... | STATUS ...`). Essas linhas também servem de evidência.

## Se algo não bater

- **LED apagado:** confira se o LED RGB está como catodo comum (`"common": "cathode"` no `diagram.json`).
- **OLED em branco:** confira se o endereço é `0x3C` e se DATA está no D21 e CLK no D22.
- **OLED com "SEM DADOS" e LED apagado na partida:** algum sensor ainda não respondeu. É o comportamento esperado: sem leitura, o poste não decide (na dúvida, não fica verde).
- **"Aviso: falha HC-SR04" ou "Aviso: falha DHT22" na linha de cima:** o sensor falhou na última leitura e o OLED mostra o último valor válido. Se a calçada estava mudando, a contagem recomeça na próxima leitura válida.

## Etapa 2: dados no ThingSpeak

Antes, cole a Write API Key no `sketch.ino` do Wokwi, como explica o `THINGSPEAK.md`. A chave entra só enquanto a simulação roda: **não salve o projeto com ela**, nem mostre essa linha nos prints. A chave nunca vai para o GitHub.

1. Abra o canal público em outra aba: https://thingspeak.mathworks.com/channels/3577155
2. Rode a simulação com a aba do Wokwi na frente. Em 20 a 40 s, o monitor serial mostra `Wi-Fi conectado.` e depois `ThingSpeak: registro N gravado (status 0, motivo 0)`.
3. Repita os passos 1 a 7 da tabela acima. A cada 20 s sai um envio, e os gráficos do canal acompanham:
   - field3 (índice de calor) sobe nos passos 2, 3 e 5;
   - field4 (distância) cai nos passos 4 e 6;
   - field5 (status) acompanha a cor do LED: 0 verde, 1 amarelo, 2 vermelho. Cada envio leva a última leitura antes dele, então a mudança aparece no envio seguinte;
   - field6 (motivo) mostra 1 (calçada), 2 (calor) ou 3 (os dois).
4. **Sem internet não trava:** o envio roda numa tarefa separada. O poste continua decidindo e mostrando no OLED e no LED, com uma leitura a cada 5 s. O serial avisa `sem Wi-Fi, envio pulado` e tenta reconectar no próximo envio.
5. Quando terminar, volte a linha da chave para `COLE_SUA_CHAVE_AQUI` antes de salvar o projeto.

O canal recebe no máximo um envio a cada 15 s. Não rode o mesmo projeto em duas abas ao mesmo tempo: os envios das duas se atropelam e o ThingSpeak recusa parte deles.

**Prints para o README (Etapa 2):**

1. Monitor serial com `Wi-Fi conectado.` e `ThingSpeak: registro ... gravado`
2. Página do canal com os gráficos de índice de calor, distância e status mudando
3. Wokwi com o LED vermelho e, ao lado, o gráfico do field5 em 2 no mesmo momento

## Bordas da calçada (para conferir no simulador)

A distância é calculada com 58,8 µs de eco por centímetro, valor medido no próprio Wokwi (veja `evidencias/calibracao_hc_sr04.md`). Ela conta em centímetros inteiros, arredondando para baixo, por isso o valor escolhido no simulador aparece exato no OLED. Numa calçada real, 119,6 cm conta como 119 (estreita): só 120 cm completos são livre. A única tolerância é de 0,8 mm, para cobrir a variação de ±3 µs do eco no simulador.

| HC-SR04 | Resultado |
|---|---|
| 120 cm | livre |
| 119 cm | estreita |
| 90 cm | estreita |
| 89 cm | bloqueada |
