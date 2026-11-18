# C.E.L.I.A.

**Luz, calor e buracos na calçada perto de pontos de ônibus, em uma leitura rápida.**

Protótipo de acessibilidade urbana desenvolvido para a **Global Solution 2026.2 da FIAP**, no tema CitizenTech. A proposta é instalar módulos em postes perto de pontos de ônibus no Rio de Janeiro para ajudar pessoas com deficiência física ou mobilidade reduzida a escolher por onde passar.

[Abrir a simulação no Wokwi](https://wokwi.com/projects/481054670710338561) · [Ver o canal público no ThingSpeak](https://thingspeak.mathworks.com/channels/3577155) · [Seguir o roteiro da demonstração](edge/ROTEIRO_DEMO.md)

![Simulação da C.E.L.I.A. no Wokwi com LED vermelho e EVITE por sol direto com calor](edge/evidencias/luz_calor_18-11/04_evite_sol_50000lux_33C.png)

*Poste no Wokwi (18/11). Com 50.000 lux (sol direto) e 33 °C, o LED fica vermelho e o serial mostra EVITE, motivo luz e calor.*

## Como o poste decide

1. **Mede a cada 5 s.** O DHT22 mede temperatura e umidade para calcular o índice de calor. O LDR mede a luz em lux.
2. **Confirma uma mudança na luz.** A nova faixa de luz precisa persistir por 15 s na demonstração ou 60 s no modo normal. A sombra de um ônibus ou o farol de um carro passando não mudam o trecho.
3. **Mostra e publica o status.** O ESP32 acende o LED no local e escreve uma linha no monitor serial. Uma tarefa de rede envia a última leitura disponível ao ThingSpeak e espera 20 s após o fim de cada tentativa. O poste continua avaliando o trecho durante a espera pela rede.
4. **Ouve a câmera.** Se a câmera do poste vê um buraco, a API de visão avisa por MQTT e o trecho fica em Evite (motivo 5) por um tempo.

| LED | Status | Quando aparece |
| --- | --- | --- |
| 🟢 Verde | **0 · Acessível** | Luz normal (ou sol) e calor normal |
| 🟡 Amarelo | **1 · Atenção** | Rua escura, ou calor elevado sem sol direto |
| 🔴 Vermelho | **2 · Evite** | Sol direto com calor elevado, ou buraco visto pela câmera |

A rua conta como **escura abaixo de 10 lux** e como **sol direto a partir de 25.000 lux**. Para o índice de calor, a atenção começa em **32 °C** e o protótipo usa **41 °C** para perigo. A tabela completa está em [`edge/README.md`](edge/README.md).

> **Mudança de 18/11.** O poste tinha um HC-SR04 (largura livre da calçada) e um OLED. Os dois saíram e entrou o LDR. A luz vai no field7 do canal; o field4 (distância) fica vazio nos registros novos. As evidências de 11 a 15/11 continuam no repositório como histórico da versão anterior.

## Experimente

1. Abra o [projeto no Wokwi](https://wokwi.com/projects/481054670710338561) e inicie a simulação. Para rodar exatamente a versão deste repositório, use os arquivos de [`edge/`](edge/).
2. Comece com o módulo de luz em **500 lux** e o DHT22 a **26 °C** e **50%** de umidade: o LED fica verde.
3. Mude a luz para **5 lux**. Quando a mudança for confirmada (15 s), o LED fica amarelo e o serial mostra `motivo 1 luz`.
4. Com a temperatura em **33 °C** e a luz em **50.000 lux**, o LED fica vermelho: sol direto com calor. O passo a passo completo está no [roteiro](edge/ROTEIRO_DEMO.md).
5. Para acompanhar os envios, veja o [guia do ThingSpeak](edge/THINGSPEAK.md). A demonstração local funciona sem uma chave de escrita.

## O que já foi validado

- **Poste com luz e calor (18/11).** No Wokwi, com Wi-Fi e MQTT conectados, o poste passou por luz normal (verde), rua escura (amarelo, motivo luz), calor (amarelo, motivo calor) e sol direto com calor (vermelho, motivo luz e calor), e a mudança de luz levou 15 s para valer. Os valores de lux e ADC bateram com os testes no computador. [Evidências de 18/11](edge/evidencias/luz_calor_18-11/LEIA-ME.md)

Até 15/11 o poste tinha HC-SR04 e OLED. As validações abaixo são dessa versão e mostram a base que continua igual (envio ao ThingSpeak e câmera por MQTT):

- **Circuito e regras locais (versão anterior).** A simulação com leitura de 5 s e confirmação de 15 s registrou estreitamento, alerta por calor, bloqueio e liberação. [Evidências da Etapa 1](edge/evidencias/LEIA-ME.md)
- **Telemetria.** A primeira versão registrou 11 envios reais no canal público. Uma revisão intermediária confirmou no Wokwi que o poste continuava lendo enquanto a tarefa de rede fazia pedidos HTTP. Em 13/11, a rodada com a chave real gravou os registros 12 a 18, inclusive um envio com **50 cm confirmados, LED vermelho e `field5=2`**. A tarefa de rede ficou com 5588 bytes de pilha sobrando, de 8192. Em 15/11, mais 21 registros (19 a 39) passaram pelas três faixas, para o programa Python da equipe. [Evidências da Etapa 2](edge/evidencias/etapa2/LEIA-ME.md) · [Leituras de 15/11](edge/evidencias/leituras_15-11/LEIA-ME.md)
- **Câmera por MQTT (Etapa 4, lado do poste).** No Wokwi, com o broker público real, um `buraco` publicado no tópico `celia/poste01/camera` deixou o trecho em EVITE (motivo 5) na hora, `livre` não mudou nada, uma mensagem desconhecida foi ignorada e o aviso venceu depois do tempo de validade. Com a chave real, o canal recebeu `field5=2` e `field6=5` enquanto o buraco valia. Na versão final, o MQTT roda numa tarefa própria, separada do envio ao ThingSpeak, e foi validado no Wokwi com Wi-Fi em 13/11. [Evidências da Etapa 4](edge/evidencias/etapa4/LEIA-ME.md)
- **Próximas etapas.** Gravar registros do poste novo no canal (com a luz no field7) e fazer o teste de ponta a ponta: câmera do poste → API de visão → MQTT → ESP32 no Wokwi → ThingSpeak, com `field6=5`. Depois, o consumo dos alertas pelo app e a leitura do field7 pelo Python.

## Câmeras (ESP32-CAM)

Pelo backlog de 15/11, a foto da calçada vem de uma câmera no próprio poste, e não mais do celular do cidadão. A ESP32-CAM mostra o vídeo ao vivo no navegador e, a cada 20 s, tira uma foto e manda para a API de visão, com uma chave no cabeçalho (HU-23, RF22, RNF11). No teste com a API do João, a foto foi recebida (resposta 201) e a API avisou o poste por MQTT ([evidência](camera/evidencias/LEIA-ME.md)). Se a API encontra um buraco, avisa o ESP32 do Wokwi por MQTT, e o trecho fica em EVITE com motivo 5 (HU-07). Uma segunda montagem, presa numa cadeira de rodas, salva fotos de calçadas no cartão para treinar o modelo de buracos (HU-06). Programas, ligações e o formato de envio para a API estão em [`camera/`](camera/).

## Código e testes

| Caminho | Conteúdo |
| --- | --- |
| [`edge/`](edge/README.md) | Poste no Wokwi: como funciona, circuito, campos do ThingSpeak e limitações |
| [`edge/sketch.ino`](edge/sketch.ino) | Firmware do ESP32: DHT22, LDR, LED, envio ao ThingSpeak e câmera por MQTT |
| [`edge/celia_logica.h`](edge/celia_logica.h) | Regras: luz em lux, faixas de luz e calor, status, motivo e confirmação |
| [`edge/diagram.json`](edge/diagram.json) | Circuito no Wokwi |
| [`tests/`](tests/) | Testes da lógica e do firmware com sensores e rede simulados |
| [`camera/`](camera/) | Programas da ESP32-CAM: câmera do poste (vídeo ao vivo e foto a cada 20 s para a API) e câmera da cadeira de rodas (fotos no cartão) |
| [`calculo/`](calculo/) | Modelagem com limites, derivadas e integrais (Differentiated Problem Solving) |

Com `g++` disponível, rode na raiz do repositório:

```sh
g++ -std=c++17 -Wall -o tests/teste_logica tests/teste_logica.cpp && ./tests/teste_logica
g++ -std=gnu++17 -I tests/arduino_falso -o tests/teste_sketch tests/teste_sketch.cpp && ./tests/teste_sketch
```
