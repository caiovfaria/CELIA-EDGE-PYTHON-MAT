# ThingSpeak · Etapa 2

O poste manda as leituras e o status para um canal público do ThingSpeak, com 20 s entre um envio e o seguinte. O canal guarda o histórico e mostra os gráficos. O app e a parte de Python da equipe leem os dados de lá.

O envio roda numa **tarefa de rede**, separada do `loop()`. O `loop()` lê os sensores a cada 5 s, decide o status, atualiza o OLED e o LED, e deixa uma cópia dos dados (o "pacote") numa fila de 1 lugar. A tarefa de rede pega o pacote mais novo na hora de enviar. Se a internet ficar lenta, só a tarefa de rede espera. Os sensores, o OLED e o LED continuam funcionando.

- **Canal:** `GS-EDGE-C.E.L.I.A.`
- **Channel ID:** `3577155`
- **Página pública:** https://thingspeak.mathworks.com/channels/3577155
- **Projeto no Wokwi:** https://wokwi.com/projects/480457503878659073

## Campos

| Campo | O que é | Exemplo |
|---|---|---|
| field1 | Temperatura (°C) | 26.0 |
| field2 | Umidade (%) | 50.0 |
| field3 | Índice de calor (°C) | 26.0 |
| field4 | Distância livre na calçada (cm, inteiros) | 200 |
| field5 | Status: 0 acessível (verde) · 1 atenção (amarelo) · 2 evite (vermelho) | 0 |
| field6 | Motivo: 0 nenhum · 1 calçada · 2 calor · 3 calçada e calor · 5 buraco (visto pela câmera, Etapa 4). O 4 (veículo) foi retirado pela equipe | 0 |

**O que cada registro representa.** Cada envio leva o pacote da **última leitura antes dele**, feita no máximo uns 5 s antes. O field5 é o status dessa leitura, o mesmo que o LED mostrava naquela hora. Como o envio roda em paralelo, o LED pode mudar durante um pedido lento, antes de o ThingSpeak gravar o registro. Nesse caso, a mudança vai no envio seguinte (o teste do sketch cobre esse caso, no cenário 17).

O horário que o ThingSpeak mostra (`created_at`) é o da gravação, não o da leitura. Em condições normais, a diferença é de alguns segundos: até 5 s esperando o envio, mais o tempo do pedido (no Wokwi, 1 a 3 s). Isso é uma estimativa, não um teto: com a rede ruim, o pedido pode demorar mais. Se o app precisar do horário exato da leitura, dá para mandar esse horário num campo extra. É uma decisão da equipe, porque muda o que o app recebe.

## Quando o poste envia (e quando não envia)

| Situação | O que acontece |
|---|---|
| Tudo funcionando | Envia os 6 campos. O próximo envio sai 20 s depois do **fim** do anterior, então dois registros nunca ficam a menos de 20 s (o plano gratuito exige pelo menos 15 s) |
| Partida, OLED em "SEM DADOS" | Não envia nada. O poste ainda não decidiu o status. Se ficar mais de 10 min sem envio, o app mostra o trecho em cinza. **Exceção:** com um buraco da câmera valendo, envia status 2 e motivo 5, só com os campos 5 e 6 (os dos sensores ficam de fora) |
| Os dois sensores falharam na última leitura | Não envia nada. O status seria só a repetição do antigo. Se continuar assim por 10 min, o app mostra cinza. **Exceção:** com um buraco da câmera valendo, envia só os campos 5 e 6 (status 2, motivo 5), porque esse status vem da câmera, não dos sensores |
| Nenhuma leitura nova desde o último envio | Não envia (não repete dado velho) |
| DHT22 falhou na última leitura | Envia sem os campos 1, 2 e 3. O gráfico mostra um buraco, em vez de repetir um valor velho |
| HC-SR04 falhou na última leitura | Envia sem o campo 4 |
| Wi-Fi caiu | Não envia e continua decidindo e mostrando no OLED e no LED. O próprio ESP32 tenta reconectar. Depois de 1 minuto sem conexão, o sketch força uma nova tentativa (no máximo uma por minuto) |
| Faltou memória para a fila ou para a tarefa de rede na partida | Avisa no serial e segue sem envio. Sensores, OLED, LED e câmera funcionam normalmente |
| ThingSpeak lento, recusou ou não respondeu | Só a tarefa de rede espera. O limite de 5 s vale para cada operação, separadamente: 5 s para conectar e 5 s para esperar a resposta. A busca do endereço (DNS) vem antes e não tem limite próprio, então não existe um teto total para o pedido. Como tudo isso roda na tarefa de rede, nenhuma dessas esperas trava o poste. Avisa no monitor serial e tenta de novo 20 s depois |
| Chave ainda é `COLE_SUA_CHAVE_AQUI` | Não envia e avisa no serial para colar a chave |

## Como colocar a chave (só na hora de rodar)

O link do projeto no Wokwi é público: quem abre vê o código. Por isso, **o projeto salvo fica sempre com `COLE_SUA_CHAVE_AQUI`**, igual ao GitHub. A chave entra só enquanto a simulação roda:

1. No ThingSpeak, abra o canal e vá na aba **API Keys**. Copie a **Write API Key**.
2. No `sketch.ino` do Wokwi, troque `COLE_SUA_CHAVE_AQUI` pela chave nesta linha (deixe a linha `CHAVE_DE_EXEMPLO` como está):
   ```cpp
   const char* THINGSPEAK_CHAVE = "COLE_SUA_CHAVE_AQUI";
   ```
3. Rode a simulação. Depois de uns 20 s, o monitor serial deve mostrar:
   ```
   Wi-Fi conectado.
   ThingSpeak: registro 1 gravado (status 0, motivo 0)
   ```
4. **Não clique em SAVE com a chave colada.** Quando terminar, volte a linha para `COLE_SUA_CHAVE_AQUI`.

O monitor serial nunca mostra a chave, então os prints dele podem ir para o README. Nos prints do código, deixe a linha da chave de fora.

**Se a chave já ficou salva no projeto público** (ou foi parar num print ou no GitHub), troque a chave: aba **API Keys → Generate New Write API Key**. A antiga para de funcionar na hora. A Write API Key só permite gravar neste canal, mas, com ela, qualquer um poderia mandar dados falsos para o canal.

Quem abrir o link público e rodar vai ver `cole a Write API Key no sketch para enviar` no serial. O resto funciona igual (sensores, OLED e LED). Os envios reais ficam comprovados pelo histórico do canal e pelas evidências em `evidencias/etapa2/`.

## Como ler os dados (app e Python)

O canal é público, então a leitura não precisa de chave:

- Últimos 20 envios: https://api.thingspeak.com/channels/3577155/feeds.json?results=20
- Último status (field5): https://api.thingspeak.com/channels/3577155/fields/5/last.json
- Em CSV, para a parte de Python: https://api.thingspeak.com/channels/3577155/feeds.csv?results=100

## Se algo não bater

- **"ThingSpeak: cole a Write API Key no sketch para enviar."** A chave ainda é a de exemplo.
- **"falha no envio (codigo 400)"** A chave está errada. Foi o que o ThingSpeak respondeu num teste com uma chave inválida.
- **"o canal recusou o envio"** O ThingSpeak respondeu "0". Normalmente, foram dois envios com menos de 15 s (por exemplo, duas abas do Wokwi rodando o mesmo projeto).
- **"falha no envio (codigo ...)"** O pedido não chegou ou não teve resposta. Códigos negativos são falhas de conexão do próprio ESP32. O poste tenta de novo em 20 s.
- **"sem Wi-Fi, envio pulado"** A rede `Wokwi-GUEST` ainda não conectou. No Wokwi, a primeira conexão leva alguns segundos. Se passar 1 minuto, aparece `Wi-Fi: 1 minuto sem conexao, tentando reconectar.`
- **"Tarefa de rede: menor sobra de pilha ate agora: N bytes"** (e o mesmo para a "Tarefa da camera") É a memória que sobrou da tarefa (8 KB cada) no pior momento até agora. A mensagem só aparece quando o valor diminui. Se N ficar perto de 0, aumente o 8192 no `xTaskCreate`.
- **"Aviso: sem memoria para a fila" ou "nao deu para criar a tarefa de rede"** O ESP32 ficou sem memória na partida. O poste funciona, mas sem envio. Reinicie a simulação. Se o aviso for da **câmera**, o poste funciona e envia, mas sem ouvir a câmera.
- **Gráfico parado:** confira se a simulação está rodando. Com a aba do navegador em segundo plano, o Wokwi fica bem mais lento.

## Observação sobre o índice de calor

O índice de calor vem da fórmula da biblioteca DHTesp: a regressão de Rothfusz, usada pelo serviço meteorológico dos EUA (NWS). O próprio NWS avisa que ela não vale para combinações extremas de temperatura e umidade, fora dos dados usados por Steadman. O NWS não dá um limite único, porque a faixa depende das duas medidas juntas. O simulador deixa o DHT22 ir até 80 °C, muito além disso. Com 80 °C e 100% de umidade, a fórmula dá 708,7 °C, um número sem sentido físico. A classificação continua certa, porque acima de 41 °C já é perigo.

- **Na demonstração:** use temperaturas realistas para o Rio, como 26, 32 e 38 °C com 50% de umidade. Os registros 8 a 11 do canal (44 e 80 °C) ficam marcados como ensaio fora da faixa (`evidencias/etapa2/LEIA-ME.md`).
- **Pendente com a equipe:** para o app, a sugestão do Codex é não enviar o field3 quando a fórmula estiver fora da faixa e combinar como sinalizar "calor extremo". Mandar um valor cortado num limite pareceria uma medição de verdade. Isso muda o que o app recebe, então fica para a equipe decidir.

## Observação sobre segurança

O envio usa HTTP simples, porque é o jeito mais curto de explicar e funciona no Wokwi. Com isso, a chave passa sem criptografia até o ThingSpeak. Isso só é aceitável numa demonstração controlada. Para um poste de verdade, o certo é usar HTTPS com validação do certificado (`WiFiClientSecure` com o certificado do ThingSpeak). Fica registrado como melhoria. Mesmo com HTTPS, a chave nunca pode ficar em código público.
