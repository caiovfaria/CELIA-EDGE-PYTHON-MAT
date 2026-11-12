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
| `edge/` | Projeto do Wokwi: `sketch.ino`, `celia_logica.h`, `diagram.json`, `libraries.txt`, roteiro e evidências | Etapa 1 aprovada pelo Codex na revisão 4 (12/11). Aguarda a aprovação do aluno |
| `tests/` | `teste_logica.cpp` (regras) e `teste_sketch.cpp` (sketch inteiro com sensores falsos de `arduino_falso/`) | 97 + 16 verificações passando |
| `visao/` | Treino do modelo de buracos e API Flask | Etapas 3 e 4 |
| `prompts.md` | Registro de uso de IA (exigido pela FIAP) | Atualizar a cada sessão |

## Decisões (não mudar sem avisar o aluno)

Os limites vêm da Documentação do Módulo Edge e **ainda estão em validação**. Por isso ficam em constantes no topo de `edge/celia_logica.h`.

| Item | Valor |
|---|---|
| Calçada (distância livre, HC-SR04) | ≥ 120 cm livre · ≥ 90 cm estreita · < 90 cm bloqueada. A regra vale para **centímetros inteiros, arredondando para baixo** (119,6 cm = 119 = estreita; 89,6 cm = 89 = bloqueada): na dúvida, a faixa pior |
| Calor (índice de calor do DHT22) | < 32 °C normal · ≥ 32 °C atenção · ≥ 41 °C perigo (pendente: a equipe pode trocar para 39 °C) |
| Status (field5) | 0 acessível (verde) · 1 atenção (amarelo) · 2 evite (vermelho). Sem buzzer |
| Regra do status | Bloqueada → 2 · estreita + perigo → 2 · estreita + normal/atenção → 1 · livre + atenção/perigo → 1 · livre + normal → 0 |
| Motivo (field6) | 0 nenhum · 1 calçada · 2 calor · 3 calçada e calor · 4 veículo (câmera) · 5 buraco (câmera) |
| HC-SR04 | Distância = eco (µs) ÷ 58,8, cortada para cm inteiros. O valor 58,8 µs/cm foi medido em 28 ecos do Wokwi, de 50 a 400 cm, com variação de ±3 µs (`edge/evidencias/calibracao_hc_sr04.md`). Tolerância efetiva: 0,08 cm (0,8 mm), que cobre a maior falta medida (0,054 cm) com cerca de 1,5 µs de sobra. Bordas: 7051 µs = 119 (estreita) e 7052 µs = 120 (livre); 5287 µs = 89 (bloqueada) e 5288 µs = 90 (estreita) |
| Falha de sensor | Sem leitura válida na partida: OLED "SEM DADOS" e LED apagado. Falha depois: mantém o último valor, avisa no OLED e cancela a confirmação da calçada em andamento |
| Tempos | Leitura a cada 5 s · mudança na calçada confirmada após 1 min (15 s com `MODO_DEMONSTRACAO`) · ThingSpeak a cada 20 s · foto vale 10 min |
| ThingSpeak | field1 temperatura · field2 umidade · field3 índice de calor · field4 distância · field5 status · field6 motivo |
| MQTT (Etapa 4) | `broker.hivemq.com`, tópico `celia/poste01/camera`, valores livre/obstaculo/buraco **com o horário junto** (um resultado antigo retido no broker não pode valer como novo) |

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
- Nenhuma chave no repositório: a chave do ThingSpeak fica como `COLE_SUA_CHAVE_AQUI`.
- Nada de dados pessoais reais. Fotos sem rostos nem placas.
- Registre no `prompts.md` os prompts das duas IAs (data, qual IA, pedido, resultado).
- Não mexa na parte de outros integrantes.

## Como testar

```
g++ -std=c++17 -Wall -o teste_logica tests/teste_logica.cpp && ./teste_logica
g++ -std=gnu++17 -I tests/arduino_falso -o teste_sketch tests/teste_sketch.cpp && ./teste_sketch
```
