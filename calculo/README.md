# Cálculo: modelagem matemática da C.E.L.I.A.

Parte de **Differentiated Problem Solving** (prof. Celso) da Global Solution 2026.2. O programa usa limites, derivadas e integrais para apoiar três decisões do poste e do app:

| Conceito | Pergunta | Resultado principal |
| --- | --- | --- |
| Limite | Por que o poste confirma uma mudança da calçada antes de trocar a cor? | A regra da calçada é descontínua em 90 cm e 120 cm. Perto do salto, 3 µs de ruído no eco trocam a cor |
| Derivada | Quando o calor sobe mais rápido e quando chega ao pico? | No dia mais quente (18/11/2023), +3,2 °C/h às 08h26 e pico às 12h11 |
| Integral | Quanto calor acima de 32 °C uma pessoa acumula esperando o ônibus? | Espera de 15 min ao meio-dia: 3,93 °C·h no dia quente, contra 0,39 °C·h no verão médio |

O relatório técnico completo (problema, modelo, análises e interpretação) é entregue em PDF no Teams.

## Como rodar

```sh
pip install numpy matplotlib
python calculo_celia.py
```

O programa mostra todas as contas na tela, salva o mesmo texto em `resultados.txt` e gera as figuras em `graficos/`.

## Arquivos

| Arquivo | Conteúdo |
| --- | --- |
| `calculo_celia.py` | Fórmula do índice de calor (igual à do ESP32), modelo IC(t), limites, derivadas, integrais e figuras |
| `dados/open_meteo_rio_verao_2023-2024.json` | Temperatura e umidade horárias do Rio: o dia mais quente (18/11/2023) e a média do verão (dez/2023 a fev/2024), com a URL de download |
| `dados/canal_3577155_registros_1a39.json` | Os 39 registros do canal ThingSpeak da equipe (simulados no Wokwi) |
| `graficos/fig1_modelo_ic.png` | Dados horários do índice de calor e o modelo ajustado |
| `graficos/fig2_limites_calcada.png` | Status pela largura da calçada, com os limites laterais |
| `graficos/fig3_ic_extremo.png` | Comportamento da fórmula do índice de calor em valores extremos |
| `graficos/fig4_derivada.png` | Derivada IC'(t): subida mais rápida e pico |
| `graficos/fig5_integral_exposicao.png` | Calor acumulado acima de 32 °C no dia e numa espera de 15 min |
| `resultados.txt` | Saída completa do programa |

## Fontes dos dados

- [Open-Meteo Historical Weather API](https://open-meteo.com/en/docs/historical-weather-api), licença CC BY 4.0. Ponto de grade em −22,95 / −43,22 (Rio de Janeiro), de 01/11/2023 a 31/03/2024. A URL exata está no arquivo de dados.
- [NOAA — The Heat Index Equation](https://www.wpc.ncep.noaa.gov/html/heatindex_equationbody.html): fórmula do índice de calor.
- Erro do DHT22 (±0,5 °C e ±2 % de umidade): [ficha técnica na loja Arduino](https://store-usa.arduino.cc/collections/all/products/grove-temperature-humidity-sensor-pro).
- Os registros do canal [ThingSpeak 3577155](https://thingspeak.mathworks.com/channels/3577155) foram gerados no Wokwi e não são medições reais.

Os limites usados (120 cm, 90 cm, 32 °C e 41 °C) são os do poste até 15/11, com HC-SR04 (`edge/celia_logica.h` até o commit `b47342a`). Em 18/11 o HC-SR04 saiu e entrou o LDR; os limites de calor (32 °C e 41 °C) continuam os mesmos em [`edge/celia_logica.h`](../edge/celia_logica.h).
