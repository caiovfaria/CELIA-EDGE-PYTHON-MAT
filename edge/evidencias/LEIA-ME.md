# Evidências da Etapa 1 (teste no Wokwi, 11/11/2026)

Prints tirados no Wokwi com o código desta pasta, mudando os sensores pelos controles da simulação.

## Depois da revisão do Codex (12/11)

O código mudou em quatro pontos: falha de sensor, partida sem leitura, conversão do eco e tempos em `uint32_t`. Testes rodados com o código final:

- `tests/teste_logica.cpp`: **97 verificações**, todas passando. Inclui:
  - o retorno do `millis()` a zero, agora de verdade, com 32 bits;
  - o eco solto seguido de falhas;
  - os 28 ecos medidos no Wokwi (`calibracao_hc_sr04.md`);
  - pulsos consecutivos nas duas bordas (7050 a 7056 µs perto de 120 cm e 5286 a 5292 µs perto de 90 cm);
  - todos os centímetros de 2 a 400 cm, com o eco variando ±3 µs.
- `tests/teste_sketch.cpp`: **16 verificações** do sketch inteiro com sensores falsos, todas passando. Cobrem:
  - partida sem nenhum sensor e partida sem DHT22;
  - tela SEM DADOS mostrando a falha atual;
  - eco solto seguido de 30 s de falhas;
  - DHT22 falhando com o calor em atenção (mantém 34,0 °C e o LED amarelo);
  - os dois sensores falhando juntos.

  A primeira versão deste teste, rodada no código antigo, falhou em 8 verificações, o que confirma que os problemas apontados pelo Codex existiam.
- Os dois testes foram compilados com g++ e rodados no computador da equipe, na máquina Linux do app do Claude.
- Wokwi com os tempos normais (`teste_final/serial_revisao_codex.txt` e `teste_final/revisao_liberacao_15s_200cm_exato.png`):
  - a distância sai exata (100 cm e 200 cm; antes saía 101 e 202);
  - a liberação da calçada levou exatamente 15 s (de t=7s a t=22s).

### Rodada final no Wokwi (código final, leitura de 5 s, confirmação de 15 s)

Os 4 arquivos no Wokwi são idênticos aos da pasta, conferidos por SHA-256.

- `teste_final/serial_final_bordas_15s.txt`: log completo da rodada 200 → 119 → 38 °C → 89 → 120 cm.
- `teste_final/final_119cm_estreita_15s.png`: 119 cm confirmado em 15 s (de t=7s a t=22s). Amarelo, motivo calcada.
- `teste_final/final_119cm_38C_evite.png`: 38 °C com a calçada estreita. Vermelho, motivo calcada+calor.
- `teste_final/final_89cm_bloqueada_15s.png`: 89 cm confirmado em 15 s (de t=32s a t=47s). Vermelho, motivo calcada.
- `teste_final/final_120cm_livre_15s.png`: 120 cm confirmado em 15 s (de t=52s a t=67s). Verde.

A calibração do HC-SR04 que deixou 119, 89 e 120 cm exatos está em `calibracao_hc_sr04.md`.

## Teste final (noite de 11/11, tempos normais: leitura a cada 5 s, confirmação de 15 s)

Antes do teste, os 4 arquivos colados no Wokwi foram comparados com os da pasta da equipe por hash SHA-256: são idênticos. As 70 verificações de `tests/teste_logica.cpp` também foram rodadas no computador da equipe, todas passando.

| Arquivo | Situação | Resultado |
|---|---|---|
| 01_circuito_completo.png | Circuito montado | Todas as peças e fios aparecem; o projeto compila |
| teste_final/passo1_acessivel.png | 26 °C e 200 cm | Verde · ACESSIVEL · motivo nenhum · leituras a cada 5 s |
| teste_final/passo2_calor_atencao.png | 32 °C e 200 cm | Amarelo · ATENCAO · motivo calor (índice de calor 34,4 °C) |
| teste_final/passo3_calor_perigo.png | 38 °C e 200 cm | Amarelo · ATENCAO · motivo calor (índice de calor 48,6 °C, perigo) |
| teste_final/pessoa_passando_confirmando.png | Distância vai a 50 cm | OLED: "Confirmando... 15s" e "Faixa 50cm bloqueada", status ainda ACESSIVEL |
| teste_final/pessoa_passou_continua_acessivel.png | Volta a 200 cm antes de 15 s | Continua ACESSIVEL: a obstrução rápida não mudou o status |

Os passos com espera de 15 s (calçada estreita, bloqueada e liberação) não foram repetidos com o tempo normal neste teste. Com a aba do navegador em segundo plano, a simulação rodou a 3–4% da velocidade real, e cada passo levaria vários minutos. Esses passos foram conferidos no teste da tarde, abaixo, com o tempo acelerado. O cenário de 15 s também está coberto no teste da lógica (`tests/teste_logica.cpp`, parte 5).

## Teste da tarde (tempo acelerado para os passos com espera)

| Arquivo | Situação | Resultado |
|---|---|---|
| 02_livre_calor_atencao.png | 32 °C e 200 cm | Amarelo · ATENCAO · motivo calor (índice de calor 34,4 °C) |
| 03_confirmando_mudanca.png | Distância muda para 100 cm | OLED mostra "Confirmando..." e o status ainda não mudou |
| 04_estreita_atencao.png | 26 °C e 100 cm, já confirmado | Amarelo · ATENCAO · motivo calcada |
| 05_estreita_calor_evite.png | 38 °C e 100 cm | Vermelho · EVITE · motivo calcada+calor (índice de calor 48,6 °C) |
| 06_bloqueada_evite.png | 26 °C e 50 cm, já confirmado | Vermelho · EVITE · motivo calcada |
| 07_liberado_acessivel.png | Volta para 200 cm, já confirmado | Verde · ACESSIVEL · motivo nenhum |

Observações:

- No computador usado no teste, a simulação rodou devagar (de 3% a 18% da velocidade real). Por isso, os prints 03 a 07 foram tirados com um **teste acelerado**: leitura a cada 1 s e confirmação de 3 s, em vez de 5 s e 15 s. A lógica é a mesma, só mudam as duas constantes. Depois do teste, o código voltou aos valores normais.
- Com os valores normais (15 s), a equipe deve tirar os prints finais para o README seguindo o `ROTEIRO_DEMO.md`.
- O teste de "pessoa passando" também foi feito com os valores normais: a distância foi a 50 cm por uma leitura e voltou para 200 cm. O status continuou ACESSIVEL.
- No navegador do teste, os fios não apareceram desenhados na tela, mas as ligações funcionaram (sensores, OLED e LED responderam). Ao colar o `diagram.json` no Wokwi e recarregar a página, os fios devem aparecer.
