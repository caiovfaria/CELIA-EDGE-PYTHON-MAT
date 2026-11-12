# Calibração do HC-SR04 no Wokwi (12/11/2026)

Para saber quanto tempo de eco o Wokwi gera para cada distância, rodamos uma versão de teste do sketch. Ela imprimia o valor bruto do `pulseIn` (em µs) a cada 1 s. Depois, mudamos a distância no controle do HC-SR04.

| Distância no Wokwi | Ecos medidos (µs) | µs por cm |
|---|---|---|
| 50 cm | 2941, 2939, 2941, 2939 | 58,78 – 58,82 |
| 89 cm | 5231, 5236, 5231, 5236 | 58,78 – 58,83 |
| 90 cm | 5291, 5293, 5291 | 58,79 – 58,81 |
| 119 cm | 6994, 6996, 6997, 6996, 6997, 6996 | 58,77 – 58,80 |
| 120 cm | 7058, 7059, 7058, 7056 | 58,80 – 58,83 |
| 200 cm | 11763, 11761, 11763 | 58,81 – 58,82 |
| 400 cm | 23525, 23523, 23525, 23523 | 58,81 |

**Resultado:** a média é de 58,80 µs por cm, ou seja, o som a cerca de 340 m/s, contando ida e volta. O mesmo valor de distância varia até ±3 µs entre leituras.

**Decisão no código** (`edge/celia_logica.h`):

- **Conversão:** distância = eco ÷ 58,8.
- **Regra em centímetros inteiros, arredondando para baixo:** só 120 cm completos contam como livre. Na dúvida, a faixa pior.
- **Folga de 0,08 cm (0,8 mm):** a maior falta medida foi de 0,054 cm (o eco de 6994 µs com o simulador em 119 cm). Sem folga, 119 cm às vezes virava 118. A folga cobre essa falta com cerca de 1,5 µs de sobra. É bem menor que a precisão de um HC-SR04 real, que fica em torno de 3 mm.

O teste `tests/teste_logica.cpp` confere que os 28 ecos acima dão exatamente a distância escolhida. Confere também todos os centímetros de 2 a 400 com o eco variando ±3 µs.

**Num sensor real:** a velocidade do som muda com a temperatura (cerca de 343 m/s a 20 °C). Numa versão física, vale recalibrar o valor de µs por cm ou corrigir pela temperatura do DHT22.
