# Evidências da Etapa 1 (teste no Wokwi, 11/11/2026)

> **Versão anterior do poste.** As evidências desta pasta e das subpastas `etapa2/`, `etapa4/`, `leituras_15-11/` e `teste_final/` são de 11 a 15/11, quando o poste tinha HC-SR04 (distância na calçada) e OLED. Em 18/11, esses dois saíram e entrou o LDR (luz). As evidências do poste atual estão em [`luz_calor_18-11/`](luz_calor_18-11/LEIA-ME.md). O código da versão anterior fica no histórico do git (até o commit `b47342a`).

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

Os cenários de estreitamento, bloqueio e liberação estão na rodada de 12/11, com leitura de 5 s e confirmação de 15 s. O ensaio preliminar acelerado está no histórico da Etapa 1 (commit 13fab0d).
