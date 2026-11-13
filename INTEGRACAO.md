# Contrato de integração da C.E.L.I.A.

Este arquivo é a fonte única do que uma parte do projeto espera da outra. Quem mudar algo aqui avisa o grupo e atualiza o código dos dois lados.

Atualizado em 12/11/2026.

## Partes e responsáveis

| Pasta | Parte | Responsável |
| --- | --- | --- |
| `edge/` | ESP32 no Wokwi, ThingSpeak | Caio |
| `visao/` | API de visão (FastAPI), modelo de buracos, MQTT | João |
| `python/` | Programa da disciplina de Lógica (lê o ThingSpeak) | João |
| `app/` | Front em React e interações (Web Dev) | Isaac |

## 1. API de visão para o ESP32 (MQTT) [HU-07, RF10, RN02]

| Item | Valor |
| --- | --- |
| Broker | `broker.hivemq.com`, porta 1883 (público, só para o protótipo) |
| Tópico | `celia/poste01/camera` |
| Quem publica | API de visão (`visao/api/main.py`), depois de cada foto analisada |
| Quem assina | ESP32 do poste 01 (`edge/sketch.ino`, Etapa 4) |
| Mensagem | Só o texto `livre` ou `buraco`. Nada de foto nem dado pessoal (RNF01) |
| QoS | 1 |
| Retain | **Não.** O broker não guarda a mensagem. O ESP32 só recebe resultado de foto nova; nada chega quando ele liga |
| Efeito no poste | `buraco`: status 2 (Evite) e motivo 5 por 10 min (validade ainda em proposta). `livre`: não força nada, vale a regra dos sensores |
| Mensagem desconhecida | Ignorar. No broker público qualquer pessoa pode publicar no tópico |

Teste sem a API: no HiveMQ WebSocket Client, publicar `buraco` em `celia/poste01/camera`.
Teste sem o Wokwi: `visao/api/ouvir_mqtt.py` assina o tópico e mostra o que chega.

Segurança: broker público serve só para o protótipo (qualquer pessoa lê e publica). Versão real: broker próprio com usuário, senha e TLS (porta 8883).

## 2. ESP32 para o app e para o Python (ThingSpeak) [HU-02, RF02, RF16]

Canal público `3577155`, leitura sem chave:

- Últimos registros: https://api.thingspeak.com/channels/3577155/feeds.json?results=20
- Último status: https://api.thingspeak.com/channels/3577155/fields/5/last.json

| Campo | Conteúdo |
| --- | --- |
| field1 | Temperatura (°C, 1 casa decimal) |
| field2 | Umidade (%, 1 casa decimal) |
| field3 | Índice de calor (°C, 1 casa decimal) |
| field4 | Distância livre na calçada (cm, inteiro) |
| field5 | Status do trecho: 0 Acessível (verde), 1 Atenção (amarelo), 2 Evite (vermelho) |
| field6 | Motivo: 0 nenhum, 1 calçada, 2 calor, 3 calçada e calor, 5 buraco (câmera). O 4 (veículo) foi removido em 12/11/2026 |

Cuidados de quem lê:

- Os valores vêm como texto (`"26.0"`, `"2"`): converter para número.
- Se um sensor falhar, o campo dele vem `null` naquele registro. Status e motivo continuam vindo.
- O poste envia a cada 20 s e não envia sem dado novo. Sem registro há mais de 10 min (pelo `created_at`, em UTC), o trecho fica cinza.
- Registros antigos de teste têm valores absurdos (80 °C, índice de calor 708). Quem lê não pode quebrar com isso.
- A cor do trecho vem direto do field5, sem recalcular, para nunca ficar diferente do LED.

## 3. Nível de calor (índice de calor, field3)

Normal abaixo de 32 °C, Atenção a partir de 32 °C, Perigo a partir de 41 °C. Valores em validação (a equipe pode trocar Perigo para 39 °C; no ESP32 é uma constante só). O calor sozinho não deixa o trecho vermelho.

## 4. App para a API de visão (HTTP) [HU-05, RF08]

- `POST /analisar`, campo de formulário `foto` (JPG, PNG ou WEBP; limite de 5 MB em proposta).
- Resposta: `resultado` (`livre` ou `buraco`), `caixas_buracos` (x1, y1, x2, y2, confianca), `mqtt_enviado`, `largura`, `altura`.
- Local: http://127.0.0.1:8000 (página de testes em `/docs`). Endereço público quando a API for para a VPS (HU-17).

## Divergências a resolver

| Tema | Como está em `AGENTS.md` | Como está na API hoje | Por quê |
| --- | --- | --- | --- |
| Framework da API | Flask | FastAPI | Mudança feita em 12/11 |
| Valores do MQTT | livre, obstaculo, buraco | livre, buraco | Detecção de carro e moto removida em 12/11: o modelo marcava carros estacionados na rua |
| Horário na mensagem | resultado com o horário junto, para um retido antigo não valer como novo | só o texto, sem retain | Sem retain, o broker não guarda mensagem antiga, então o problema do retido antigo não acontece. Se o Caio ainda quiser o horário, combinamos o formato juntos |
| field6 = 4 (veículo) | existe | removido | mesmo motivo dos carros |
