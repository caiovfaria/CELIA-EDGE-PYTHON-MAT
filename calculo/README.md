# Cálculo: modelagem matemática da C.E.L.I.A.

Parte de **Differentiated Problem Solving** (prof. Celso) da Global Solution 2026.2. O programa usa limites, derivadas e integrais para apoiar decisões do poste (ESP32 com DHT22 e LDR) e do app:

| Conceito | Pergunta | Resultado principal |
| --- | --- | --- |
| Limite | Por que o poste confirma uma mudança da luz antes de trocar a cor? | A regra da luz é descontínua em 10 lux (sem calor) e em 25.000 lux (com calor). Perto de 25.000 lux, um passo do ADC (0,8 mV) vale 440 lux e troca a cor |
| Limite | Por que o poste tem um teto de 100.000 lux? | No modelo do LDR, lux(a) → +∞ quando a leitura a → 0⁺; o modelo passa de 100.000 lux para a < 32,2 |
| Derivada | Quando o calor sobe mais rápido e quando chega ao pico? | No dia mais quente (18/11/2023), +3,2 °C/h às 08h26 e pico às 12h11 |
| Derivada | Com que precisão o LDR enxerga os limites? | 0,025 lux por passo perto de 10 lux (41 passos entre 9 e 10 lux), mas 435 lux por passo perto de 25.000 lux (2,4 passos entre 24.000 e 25.000 lux) |
| Integral | Quanto calor acima de 32 °C uma pessoa acumula esperando o ônibus? | Espera de 15 min ao meio-dia: 3,93 °C·h no dia quente, contra 0,39 °C·h no verão médio |

As fórmulas do programa são as mesmas do ESP32 (`edge/celia_logica.h`): o índice de calor bate com os 10 pares de temperatura e umidade do canal, e a conversão do LDR bate com o `field7` dos registros 40 a 59.

O relatório técnico completo (problema, modelo, análises e interpretação) está em [`relatorio_calculo_celia.pdf`](relatorio_calculo_celia.pdf) e é entregue em PDF no Teams.

## Integrantes

| Nome completo | RM | Parte no projeto |
| --- | --- | --- |
| Caio Viana de Faria | 570634 | Edge (ESP32 no Wokwi e ThingSpeak), câmera do poste e Cálculo |
| Isaac Israel Rosa Coimbra | 570072 | Front-end (app React) e Web Development |
| João Thees Castro Santiago | 572829 | Python: programa de Lógica, API de visão computacional, modelo de buracos e assistente Gemini |

## Como rodar

```sh
pip install numpy matplotlib
python calculo_celia.py
```

O programa mostra todas as contas na tela, salva o mesmo texto em `resultados.txt` e gera as figuras em `graficos/`.

## Arquivos

| Arquivo | Conteúdo |
| --- | --- |
| `calculo_celia.py` | Fórmulas do índice de calor e do LDR (iguais às do ESP32), modelo IC(t), limites, derivadas, integrais e figuras |
| `relatorio_calculo_celia.pdf` | Relatório técnico: problema, modelo, análises de cálculo e interpretação |
| `dados/open_meteo_rio_verao_2023-2024.json` | Temperatura e umidade horárias do Rio: o dia mais quente (18/11/2023) e a média do verão (dez/2023 a fev/2024), com a URL de download |
| `dados/canal_3577155_registros_1a39.json` | Registros 1 a 39 do canal ThingSpeak: testes do DHT22 no Wokwi, inclusive nos extremos do simulador |
| `dados/canal_3577155_registros_40a59.json` | Registros 40 a 59: o poste atual (DHT22 e LDR) no Wokwi, com a luz no `field7` |
| `graficos/fig1_modelo_ic.png` | Dados horários do índice de calor e o modelo ajustado |
| `graficos/fig2_limites_luz.png` | Status pela luz na calçada, com os limites laterais em 10 e 25.000 lux |
| `graficos/fig3_ic_extremo.png` | Comportamento da fórmula do índice de calor em valores extremos |
| `graficos/fig4_derivada.png` | Derivada IC'(t): subida mais rápida e pico |
| `graficos/fig5_integral_exposicao.png` | Calor acumulado acima de 32 °C no dia e numa espera de 15 min |
| `graficos/fig6_sensibilidade_ldr.png` | Modelo do LDR com seus limites nas pontas e a derivada (lux por passo do ADC) |
| `resultados.txt` | Saída completa do programa |

## Fontes dos dados

- [Open-Meteo Historical Weather API](https://open-meteo.com/en/docs/historical-weather-api), licença CC BY 4.0. Ponto de grade em −22,95 / −43,22 (Rio de Janeiro), de 01/11/2023 a 31/03/2024. A URL exata está no arquivo de dados.
- [NOAA — The Heat Index Equation](https://www.wpc.ncep.noaa.gov/html/heatindex_equationbody.html): fórmula do índice de calor.
- [Wokwi — Photoresistor (LDR) Sensor](https://docs.wokwi.com/parts/wokwi-photoresistor-sensor): conversão da leitura do LDR para lux (rl10 = 50 kΩ, gamma = 0,7).
- Erro do DHT22 (±0,5 °C e ±2 % de umidade): [ficha técnica na loja Arduino](https://store-usa.arduino.cc/collections/all/products/grove-temperature-humidity-sensor-pro).
- Os registros do canal [ThingSpeak 3577155](https://thingspeak.mathworks.com/channels/3577155) foram gerados no Wokwi e não são medições reais.

Os limites usados (10 e 25.000 lux para a luz, 32 e 41 °C para o índice de calor) e a conversão do LDR são os do poste, em [`edge/celia_logica.h`](../edge/celia_logica.h).
