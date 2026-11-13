# Evidências da Etapa 2 (ThingSpeak, 12/11/2026)

> **Atenção:** os 11 registros abaixo foram feitos com a **primeira versão** do envio, em que o pedido HTTP saía de dentro do `loop()`. A revisão 1 do Codex pediu 4 correções médias, e o código mudou:
> - o envio passou para uma tarefa de rede separada;
> - os 20 s passaram a contar do fim da tentativa;
> - o poste não envia nada com os dois sensores falhando;
> - a chave não fica mais salva no projeto público.
>
> A seção "Revalidação depois da revisão 1", no fim deste arquivo, traz os testes do código novo.

## Envio real ao canal público

- **Canal:** `GS-EDGE-C.E.L.I.A.`, ID `3577155`, público: https://thingspeak.mathworks.com/channels/3577155
- **Projeto no Wokwi:** https://wokwi.com/projects/480457503878659073
- **Rodada:** o aluno colou a Write API Key só no Wokwi e rodou o sketch da Etapa 2. A IA não digitou nem guardou a chave. No repositório, a chave continua `COLE_SUA_CHAVE_AQUI`.

| Arquivo | O que mostra |
|---|---|
| `canal_3577155_feeds.json` | Os 11 registros como o próprio ThingSpeak devolve (`/channels/3577155/feeds.json`), lidos às 19h36 |
| `conferencia_registros.txt` | Conferência dos 11 registros, um por um. Status e motivo batem com as regras de `celia_logica.h`, o índice de calor bate com a fórmula da biblioteca, e o intervalo entre envios nunca ficou abaixo de 20 s |
| `graficos_canal_3577155.png` | Gráficos do índice de calor, da distância e do status, desenhados a partir do `feeds.json` |
| `serial_wokwi_sem_wifi_chave_exemplo.txt` | Rodada no navegador embutido do app do Claude, que bloqueia o Wi-Fi do Wokwi, com a chave de exemplo. O poste decidiu e mostrou normalmente por 64 s sem internet, e o serial pediu a chave a cada 20 s |

### O que os registros comprovam

- O Wi-Fi do Wokwi conecta e o envio chega ao canal: são 11 registros, o primeiro às 19:25:52.
- Os 6 campos chegam na ordem certa (temperatura, umidade, índice de calor, distância, status e motivo).
- Menor intervalo observado entre dois registros: **20 s**, acima do mínimo de 15 s do plano gratuito. Os intervalos vão de 20 a 214 s. Os maiores (34 a 214 s) são pausas da simulação: ela foi reiniciada, e a aba do Wokwi ficou em segundo plano.
- O status acompanha o calor: com 26 °C, status 0 (verde). Com 44 °C e 80 °C e a calçada livre, status 1 (amarelo) e motivo 2 (calor). Pela regra, é isso mesmo: calçada livre com calor em perigo dá atenção, não evite. **Os registros 8 a 11 (44 e 80 °C) são ensaio fora da faixa da fórmula do índice de calor** (veja a observação abaixo).
- No monitor serial do aluno, visto num print da tela dele às 19h30, apareceram `ThingSpeak: registro 3 gravado (status 0, motivo 0)` e `registro 4 gravado`, com uma leitura a cada 5 s e a simulação a 100%.

### O que ainda falta registrar no canal

- **Distância mudando e status 2 (vermelho).** Nesta rodada, a distância ficou em 200 cm o tempo todo. Essa parte já está coberta:
  - no teste do sketch no computador (`tests/teste_sketch.cpp`, cenários 11 a 16: 50 cm → field4=50, field5=2, field6=1);
  - nas evidências de distância da Etapa 1 no Wokwi.

  Para o README, falta fazer o passo 6 do `ROTEIRO_DEMO.md` (50 cm) com o envio ligado e tirar o print do gráfico do field5 em 2.
- **Prints da tela do Wokwi e da página do canal.** Faça seguindo o `ROTEIRO_DEMO.md`, sem mostrar a linha do código que tem a chave.

## Observação: índice de calor fora da faixa da fórmula

Com o DHT22 em 80 °C e 100% de umidade, o índice de calor foi a 708,7 °C, e com 44,3 °C e 50%, a 70,5 °C. Não é erro de envio: o valor é exatamente o que a fórmula da biblioteca DHTesp calcula (conferido em `conferencia_registros.txt`). A fórmula (Rothfusz, do serviço meteorológico dos EUA) só vale até uns 50 °C, e o simulador deixa o DHT22 ir até 80 °C. A classificação continua certa, porque acima de 41 °C já é perigo. O número no gráfico, porém, perde o sentido. A demonstração usa 26, 32 e 38 °C, dentro da faixa. Se a equipe quiser, dá para limitar o valor enviado (ponto para o Codex avaliar).

## Testes no computador (mesmo código)

- `tests/teste_logica.cpp`: 97 verificações, todas passando.
- `tests/teste_sketch.cpp`: 35 verificações, todas passando. São 16 da Etapa 1 e 19 novas da Etapa 2:
  - SEM DADOS e a chave de exemplo não enviam nada;
  - sem Wi-Fi: não envia, tenta reconectar e o poste continua decidindo;
  - os 6 campos na ordem certa, com o status igual à cor do LED;
  - intervalo de 20 s entre envios;
  - um sensor com falha deixa o campo dele de fora;
  - ThingSpeak recusando ou sem resposta (5 s): avisa e não trava;
  - a chave nunca aparece no serial.

  Para conferir se esses testes pegam erros de verdade, o código foi estragado de propósito de 7 jeitos: intervalo de 10 s, envio sem status, valor velho com o DHT22 falhando, field5 trocado com o field6, URL impressa no serial, sem reconexão e chave de exemplo ignorada. Os testes pegaram os 7.
- Os dois testes rodaram com g++ 13.3 (máquina do Claude) e com g++ 11.4 (computador da equipe, no clone do repositório).

## Revalidação depois da revisão 1 do Codex

| Achado do Codex | Correção | Como foi conferido |
|---|---|---|
| [médio] O pedido HTTP rodava no `loop()` e podia travar o OLED e o LED por mais de 5 s | O envio passou para uma tarefa de rede do FreeRTOS. O `loop()` só deixa um pacote com os dados numa fila de 1 lugar | Teste do sketch: num pedido lento de 7 s, o `loop()` continua lendo. Em todo o teste, a maior pausa entre leituras foi de 5 s, e nenhum dos 14 pedidos saiu de dentro do `loop()` |
| [médio] Os 20 s contavam do início da tentativa | Os 20 s contam do **fim** da tentativa | Teste: depois de um pedido lento de 7 s, o seguinte só sai 20 s após o fim dele. Entre o fim de um envio e o início do seguinte, sempre de 20 a 20,1 s |
| [médio] Com os dois sensores falhando, o poste continuava enviando o status antigo | Não envia nada nesse caso. Também não reenvia um pacote sem leitura nova | Teste: 45 s com os dois sensores falhando e nenhum pedido. Quando os sensores voltam, envia de novo |
| [médio] O guia mandava salvar a chave no projeto público do Wokwi | A chave entra só enquanto a simulação roda, sem SAVE. Se ficar salva, gere uma nova chave | `THINGSPEAK.md` e `ROTEIRO_DEMO.md` atualizados. O aluno tirou a chave do projeto |
| [leve] O teste não cobria `begin()` falhando, HTTP diferente de 200 e horários do pedido lento | Teste com horários de cada leitura e de cada pedido, `begin()` falhando e HTTP 400 | 45 verificações no teste do sketch. Com 13 erros plantados de propósito, os testes pegaram os 13 |
| [leve] O guia dava 50 °C como limite único da fórmula | O texto passou a citar o aviso do NWS e deixa com a equipe a decisão sobre não enviar o field3 fora da faixa | `THINGSPEAK.md` e `AGENTS.md` (pendência da equipe) |
| [leve] Intervalo mínimo e evidência de 50 cm | Menor intervalo de 20 s escrito no texto acima. A rodada com 50 cm fica para a revalidação no Wokwi | Feito em 13/11: registro 18 com 50 cm, `field5=2` (veja a seção final) |

O código novo **compilou e rodou no Wokwi** do navegador embutido, com os 4 arquivos idênticos aos da pasta (conferidos por SHA-256: `sketch.ino` a5533a85dc8a). Desta vez, o Wi-Fi do Wokwi conectou também nesse navegador.

| Arquivo | O que mostra |
|---|---|
| `serial_wokwi_revisao1_chave_exemplo.txt` | Com a chave de exemplo, a tarefa de rede avisa "cole a Write API Key" a cada ~20 s, e as leituras saem a cada 5 s, em linhas inteiras |
| `serial_wokwi_revisao1_chave_falsa.txt` | Com uma chave falsa de teste, trocada só no editor e sem salvar, o pedido HTTP real sai pela tarefa de rede, e o ThingSpeak responde 400. Enquanto cada pedido espera, as leituras continuam exatamente a cada 5 s. As tentativas saem a cada ~20 s |

## Revalidação depois da revisão 2 do Codex

| Achado do Codex | Correção | Como foi conferido |
|---|---|---|
| [médio] Se `xQueueCreate()` falhasse, o envio passava uma fila nula ao FreeRTOS e podia derrubar o poste. Se `xTaskCreate()` falhasse, o envio parava sem aviso | `iniciarEnvio()` confere os dois retornos, avisa no serial e segue sem envio. `guardarParaEnvio()` não usa fila nula | Teste com a fila falhando e com a tarefa falhando: aviso no serial, nenhum envio, e sensores, OLED e LED continuam. Sem essas proteções, o teste quebra, como o ESP32 quebraria |
| [médio] O guia prometia o field5 igual ao LED no momento do envio | O guia agora diz que cada registro é a leitura capturada antes do envio, e que o horário do ThingSpeak é o da gravação | Teste: o calor sobe durante um pedido lento. O LED fica amarelo antes de o registro lento terminar, esse registro leva status 0 e o seguinte leva status 1 |
| [leve] A pilha da tarefa não era medida | A tarefa mede a sobra com `uxTaskGetStackHighWaterMark` e avisa no serial quando ela diminui | Teste: a mensagem aparece. Valor real em 13/11: 5588 bytes sobrando, de 8192 |
| [leve] `WiFi.reconnect()` a cada tentativa podia atrapalhar a reconexão automática | O sketch só força a reconexão depois de 1 min sem Wi-Fi, no máximo uma vez por minuto | Teste: com 25 s sem Wi-Fi, nenhuma reconexão forçada; com 65 s, exatamente uma. Derrubar o Wi-Fi do Wokwi de propósito não é possível no simulador, então isso fica coberto só pelo teste |

Teste do sketch: **55 verificações**. Com 18 erros plantados de propósito, os testes pegaram os 18.

## Revisão 3 do Codex

O parecer foi **"Etapa 2 aprovada para commit após a rodada do aluno"**, com 2 apontamentos leves, já corrigidos:
- Se a tarefa de rede não for criada, a fila é liberada (`vQueueDelete`) e `filaEnvio` volta a `NULL`. Assim, o `loop()` para de guardar pacotes que ninguém vai ler. Há uma verificação nova no teste: são **56** agora, e o 19º erro plantado (sem liberar a fila) também foi pego.
- O texto sobre tempos foi ajustado. O limite de 5 s vale para cada operação (conectar e esperar a resposta), e a busca do endereço (DNS) não tem limite próprio. A diferença entre a leitura e a gravação é uma estimativa em condições normais, não um teto.

Falta a rodada do aluno com a chave real e o código novo:
- registros novos no canal;
- o passo de 50 cm (LED vermelho e field5 = 2);
- o valor da sobra de pilha depois de um envio aceito.

## Rodada final com a chave real (13/11/2026)

A rodada que faltava foi feita com o código da Etapa 4, que inclui todo o envio da Etapa 2: registros 12 a 18 no canal, um envio aceito com **50 cm confirmados, LED vermelho e `field5=2`** (registro 18) e a **sobra de pilha de 5588 bytes** depois de envios HTTP reais. Intervalos de 22 a 23 s. O serial, o JSON do canal e a conferência estão em `../etapa4/` (`serial_wokwi_chave_real.txt`, `canal_3577155_registros_12a18.json` e `LEIA-ME.md`).
