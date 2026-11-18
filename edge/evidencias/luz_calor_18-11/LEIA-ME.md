# Evidências do poste com luz + calor (Wokwi, 18/11/2026)

Rodada no Wokwi com os arquivos de `edge/` (DHT22, LDR e LED RGB; sem HC-SR04 e sem OLED). O Wi-Fi e o MQTT conectaram (`Wi-Fi conectado.` e `MQTT conectado: ouvindo a camera em celia/poste01/camera.` no serial). A chave do ThingSpeak não foi colada nesta rodada, por isso o serial mostra `cole a Write API Key no sketch para enviar`.

Projeto salvo no Wokwi: https://wokwi.com/projects/481054670710338561 (público, sem a chave). Os 4 arquivos salvos (`sketch.ino`, `celia_logica.h`, `diagram.json`, `libraries.txt`) foram conferidos por SHA-256 contra os de `edge/`: são idênticos.

Os valores de lux e ADC do Wokwi bateram com os do teste no computador (`tests/teste_sketch.cpp`): 500 lux → ADC 1001 → 499 lux; 5 lux → ADC 3645 → 5 lux; 50.000 lux → ADC 52 → 50.064 lux.

| Arquivo | Situação | Resultado |
|---|---|---|
| `circuito.png` | Circuito montado | ESP32, módulo de luz (AO no D34), DHT22 (D15) e LED RGB (D25 e D26, com resistores de 220 Ω) |
| `01_acessivel_500lux_26C.png` | 500 lux, 26 °C | LED verde, `STATUS 0 ACESSIVEL`, motivo nenhum |
| `02_atencao_escuro_5lux.png` | 5 lux, 26 °C | LED amarelo, `STATUS 1 ATENCAO`, `motivo 1 luz` |
| `03_atencao_calor_33C.png` | 500 lux, 33 °C (índice de calor 36,3 °C) | LED amarelo, `motivo 2 calor` |
| `04_evite_sol_50000lux_33C.png` | 50.000 lux, 33 °C | `confirmando luz 10s` e `5s`, depois LED vermelho, `STATUS 2 EVITE`, `motivo 3 luz+calor` |

Na mesma rodada, a volta de 5 lux para 500 lux levou exatamente 15 s para valer (`confirmando luz 15s`, `10s`, `5s` e depois `vale normal`), como pede o modo demonstração.

Observação: mudar a temperatura no DHT22 com a simulação rodando não chegou ao poste nesta rodada (a luz mudava na hora). Para os passos com 33 °C, a temperatura foi colocada no `diagram.json` com a simulação parada. O projeto salvo e o `diagram.json` do repositório ficam com 26 °C.
