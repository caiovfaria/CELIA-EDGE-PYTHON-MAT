# Leituras para o programa Python · 15/11/2026

O programa de Lógica do João (pasta `python/`) lê o field3 (índice de calor) e o field4 (distância livre) do canal 3577155 e precisa de pelo menos 20 leituras. Até 13/11, o canal tinha 18 registros, e os registros 8 a 11 eram um ensaio fora da faixa da fórmula (44 °C e 80 °C, veja `../etapa2/LEIA-ME.md`). Nesta rodada, o canal recebeu **21 registros novos (19 a 39)**, todos com valores realistas e passando pelas três faixas.

## Como foi feito

- **Versão:** a final do repositório, carregada no editor do Wokwi (navegador do app do Claude). Os arquivos foram conferidos por SHA-256 e são iguais aos da `main`: `sketch.ino` a3ba19c5932e4c35, `celia_logica.h` 414de7b8c4d18d5e e `libraries.txt` 92f59f552633531e.
- **Chave:** o aluno colou a Write API Key no editor. A IA não digitou, não leu e não guardou a chave. No fim, a linha voltou para `COLE_SUA_CHAVE_AQUI` (conferido pelo hash) e o projeto **não foi salvo**.
- **Sensores:** a temperatura, a umidade e a distância foram mudadas pelos controles do próprio Wokwi, em 8 passos.
- **Rede:** houve três partidas. Na 1ª, a rede do Wokwi caiu logo depois do registro 19: o ThingSpeak falhou com código -1 e o MQTT não reconectou, mas o poste continuou lendo e decidindo. Na 2ª, o Wi-Fi do Wokwi nem conectou, e depois de 1 minuto o poste forçou a reconexão, como previsto. Na 3ª, tudo funcionou. O problema foi da rede do simulador: o próprio navegador continuava acessando o ThingSpeak.

## Registros 19 a 39

| Passo | Sensores (T · U · distância) | Registros | field3 | field4 | Status · motivo |
|---|---|---|---|---|---|
| 1 | 26 °C · 50% · 200 cm | 19 | 26,0 | 200 | 0 Acessível · 0 |
| 2 | 28 °C · 65% · 200 cm | 20, 21 | 30,0 | 200 | 0 Acessível · 0 |
| 3 | 32 °C · 50% · 200 cm | 22, 23 | 34,4 | 200 | 1 Atenção · 2 calor |
| 4 | 34 °C · 60% · 200 cm | 24, 25 | 42,2 | 200 | 1 Atenção · 2 calor (calor em perigo, calçada livre) |
| (volta) | 26 °C · 50% · 200 cm | 26, 27, 28 | 26,0 | 200 | 0 Acessível · 0 |
| 5 | 26 °C · 50% · 100 cm | 29 a 32 | 26,0 | 100 | 1 Atenção · 1 calçada |
| 6 | 38 °C · 50% · 100 cm | 33, 34 | 48,6 | 100 | 2 Evite · 3 calçada e calor |
| 7 | 26 °C · 45% · 50 cm | 35 | 25,8 | 50 | 1 Atenção · 1 (50 cm ainda confirmando) |
| 7 | 26 °C · 45% · 50 cm | 36, 37 | 25,8 | 50 | 2 Evite · 1 calçada |
| 8 | 26 °C · 45% · 200 cm | 38, 39 | 25,8 | 200 | 0 Acessível · 0 (liberado) |

- **Por faixa:** 8 registros Acessível, 9 Atenção e 4 Evite.
- **Conferência:** os 21 registros batem, um por um, com as linhas `ThingSpeak: registro N gravado (status S, motivo M)` do serial.
- **Intervalos:** de 22 a 25 s entre registros na 3ª partida.
- **Registro 35:** a distância já era 50 cm, mas a calçada ainda estava nos 15 s de confirmação, por isso o status era 1 (estreita, o último valor confirmado). É o comportamento esperado do confirmador.
- **Uma leitura de 207 cm:** apareceu em t=99 s, durante o passo 4. Foi variação do sensor simulado e não foi enviada: o registro 24 tem 200 cm.
- **Total no canal:** 39 registros. Sem o ensaio fora da faixa (8 a 11), sobram 35 leituras realistas.
- **Sobra de pilha:** a menor da tarefa de rede foi 5504 bytes; a da câmera, 5864 bytes, de 8192 cada.

## Arquivos

| Arquivo | Conteúdo |
|---|---|
| `serial_wokwi_leituras.txt` | Monitor serial das três partidas, condensado de forma automática (leituras repetidas resumidas, nenhuma linha escrita à mão) |
| `canal_3577155_registros_1a39.json` | Os 39 registros como o ThingSpeak devolve (`/channels/3577155/feeds.json?results=40`) |

## Observações

- **Projeto salvo no Wokwi:** em 15/11, o projeto público (https://wokwi.com/projects/480457503878659073) ainda tinha o `sketch.ino` da Etapa 2 salvo. Quem abrir o link do README vê a versão antiga até alguém salvar os arquivos de `edge/`, sempre sem a chave.
- **Descrição do field6 no canal:** ainda diz "0 nenhum, 1 calcada, 2 calor, 3 ambos". Falta acrescentar "5 buraco", o que só o dono do canal pode fazer.
- **Teste de ponta a ponta:** esta rodada não incluiu a API de visão. O teste foto → API → MQTT → ESP32 → ThingSpeak continua pendente.
