# Contrato de integração da C.E.L.I.A.

Este arquivo é a fonte única do que uma parte do projeto espera da outra. Quem mudar algo aqui avisa o grupo e atualiza o código dos dois lados.

Atualizado em 13/11/2026.

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
- `GET /` (14/11/2026): `status`, `projeto` e `modelo_pronto`. Logo que liga, a API aquece o modelo em segundo plano (a primeira análise passava de 1 minuto). Enquanto `modelo_pronto` for `false`, o app mostra "Preparando a IA" e pergunta de novo a cada 2 s.

## 5. App para a API: mini trilha de IA (HTTP) [HU-19, RF18, RNF08]

Decisão do João (13/11/2026): o conteúdo da trilha fica na API; o app só monta a tela.

- `GET /trilha`: `titulo`, `observacao` e `licoes` (lista).
- `GET /trilha/{id}`: uma lição. 404 se não existir.
- Cada lição: `id`, `titulo`, `duracao_minutos`, `paragrafos` (lista de frases curtas), `exemplo_no_trabalho`, `pratique`.
- Acessibilidade (RNF08) é do app: títulos como cabeçalhos, leitura por leitor de tela, navegação por teclado.
- Texto das lições: rascunho em `visao/api/dados/trilha.json`, para a equipe revisar.

## 6. App para a API: assistente de IA com Gemini Flash (HTTP) [HU-21, HU-22, RF20, RF21]

Provedor Gemini Flash (decisão do João, 13/11/2026). RN06 e RN07 aprovadas (13/11/2026). A chave fica só no `.env` da API; o app nunca fala direto com o Gemini.

**PROPOSTA (Isaac confirma):** o app envia os trechos e locais que já tem junto com a pergunta, e o assistente responde só com eles.

`POST /assistente/trajeto` (JSON) [RF20, RN06]:

```json
{
  "pergunta": "Tem banheiro acessível perto do ponto?",
  "trechos": [{"id": "t1", "nome": "Ponto Av. Presidente Vargas", "status": 2, "motivo": 5}],
  "locais": [{"id": "b1", "nome": "Banheiro da estação", "tipo": "banheiro", "acessibilidade": "barras e porta larga", "endereco": ""}]
}
```

- `status` e `motivo` são o field5 e o field6 do ThingSpeak. `tipo`: `banheiro`, `descanso` ou `capacite-se`.
- Resposta: `resposta` (texto curto, sem markdown, pronto para leitor de tela), `trechos_citados` e `locais_citados` (ids, para destacar no mapa).
- Se o modelo citar um id que o app não mandou, a API descarta a resposta e devolve um aviso (RN06).

`POST /assistente/descricao` (JSON) [RF21, RN05, RN07]:

```json
{"resultado_ia": "buraco", "local": "Rua X, 100, Centro", "texto_cidadao": "buraco grande perto da rampa"}
```

- Resposta: `descricao_sugerida` e `revisar_antes_de_enviar: true`.
- RN07: o app mostra o texto num campo editável e só envia o report depois que o cidadão confirma. A API não envia nada à prefeitura e nunca recebe a foto.

Modelos (14/11/2026): a API tenta `gemini-3.8-flash`, depois `gemini-3.5-flash` e por último `gemini-3.5-flash-lite` (até 20 s cada). A mensagem do 503 diz o motivo da última falha (sobrecarga, cota ou demora). O app tenta de novo sozinho até 3 vezes (espera 3, 6 e 12 s) e depois oferece "Tentar de novo".

Erros: 422 (dados fora do formato), 413 (trechos e locais somando mais de 8.000 caracteres, PROPOSTA: mandar só os itens perto da rota), 429 (mais de 5 perguntas por minuto do mesmo IP, PROPOSTA), 503 (Gemini sem chave, sem cota ou fora do ar; o app mostra a mensagem do campo `detail`).

LGPD: antes de ir ao Gemini, a API troca CPF, telefone e e-mail por marcadores. Nada da conversa é guardado (RNF09). Na cota grátis, o Google pode usar o conteúdo para melhorar os produtos dele: por isso só dados simulados (RNF04).

## 7. Guia passo a passo para o app (Isaac) [HU-05, HU-19, HU-21, HU-22, RNF06]

O app só conversa com a API da equipe por HTTP (`fetch`). Ele nunca fala com o Gemini nem guarda chave nenhuma. O código abaixo é um exemplo de referência: o Isaac adapta do jeito dele e registra no `AI.md`.

### Passo 1. Rodar a API na sua máquina (enquanto não houver VPS)

```powershell
cd visao\api
python -m venv .venv
.venv\Scripts\python -m pip install -r requirements.txt
copy .env.example .env
```

Abra o `.env` e cole a sua chave do Gemini (gratuita em https://aistudio.google.com, "Get API key"). Depois:

```powershell
.venv\Scripts\python -m uvicorn main:app --reload
```

Teste em http://127.0.0.1:8000/docs. Sem chave, a análise de foto e a trilha funcionam; só o assistente responde 503.

### Passo 2. Endereço da API no app

No projeto React (Vite), crie um `.env` na raiz do app:

```
VITE_URL_API=http://127.0.0.1:8000
```

Quando a API for para a VPS (HU-17), só esse endereço muda. O app precisa rodar em `localhost:5173` ou `localhost:3000` (CORS liberado). Outro endereço: avisar o João para incluir em `ORIGENS_PERMITIDAS`.

### Passo 3. Uma função para chamar a API

Fica num arquivo separado da tela (separação de dados e apresentação):

```js
// src/servicos/api.js
const URL_API = import.meta.env.VITE_URL_API;

// Chama a API e devolve o JSON. Em erro, lança a mensagem em português que a API manda em "detail".
export async function chamarApi(caminho, opcoes = {}) {
  const resp = await fetch(`${URL_API}${caminho}`, opcoes);
  const dados = await resp.json();
  if (!resp.ok) {
    const mensagem = typeof dados.detail === "string" ? dados.detail : "Dados enviados fora do formato.";
    throw new Error(mensagem);
  }
  return dados;
}
```

### Passo 4. Custom hook com carregando e erro

```js
// src/hooks/useApi.js
import { useState } from "react";
import { chamarApi } from "../servicos/api";

// Guarda o estado de qualquer chamada: carregando, erro e o último resultado.
export function useApi() {
  const [carregando, setCarregando] = useState(false);
  const [erro, setErro] = useState(null);
  const [dados, setDados] = useState(null);

  async function executar(caminho, opcoes) {
    setCarregando(true);
    setErro(null);
    try {
      const resultado = await chamarApi(caminho, opcoes);
      setDados(resultado);
      return resultado;
    } catch (e) {
      setErro(e.message);
      return null;
    } finally {
      setCarregando(false);
    }
  }

  return { executar, carregando, erro, dados };
}
```

### Passo 5. Analisar a foto (seção 4) [HU-05, RF08]

A foto vai como formulário (`FormData`), não como JSON:

```jsx
const { executar, carregando, erro, dados } = useApi();

async function enviarFoto(arquivo) {        // arquivo vem do <input type="file" accept="image/*">
  const formulario = new FormData();
  formulario.append("foto", arquivo);
  await executar("/analisar", { method: "POST", body: formulario });
}
// dados.resultado: "livre" ou "buraco"
// dados.caixas_buracos: desenhar sobre a foto, escalando por largura e altura
```

### Passo 6. Mini trilha de IA (seção 5) [HU-19, RF18]

```jsx
const { executar, dados } = useApi();
useEffect(() => { executar("/trilha"); }, []);
// dados.licoes.map(...) monta a lista; cada lição tem titulo, paragrafos, exemplo_no_trabalho e pratique
```

### Passo 7. Assistente do trajeto (seção 6) [HU-21, RF20, RN06]

O cidadão só escreve (ou fala) a pergunta. O app monta o JSON com os dados que já tem:

```jsx
const { executar, carregando, erro, dados } = useApi();

async function perguntar(textoDoCidadao) {
  await executar("/assistente/trajeto", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      pergunta: textoDoCidadao,
      trechos,   // do ThingSpeak: [{ id, nome, status: field5, motivo: field6 }]
      locais,    // da lista do app: [{ id, nome, tipo, acessibilidade, endereco }]
    }),
  });
}
// Mostrar só dados.resposta num balão. trechos_citados e locais_citados: destacar no mapa (opcional).
// Enquanto carregando: "Pensando..." (pode levar até uns 20 s).
```

### Passo 8. Descrição do report (seção 6) [HU-22, RF21, RN07]

```jsx
const [texto, setTexto] = useState("");

async function sugerir(resultadoDaFoto, local, textoDoCidadao) {
  const r = await executar("/assistente/descricao", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ resultado_ia: resultadoDaFoto, local, texto_cidadao: textoDoCidadao }),
  });
  if (r) setTexto(r.descricao_sugerida);
}
// RN07: mostrar o texto num <textarea value={texto} onChange=...> editável.
// O botão "Enviar à prefeitura" só envia depois que o cidadão revisou e confirmou.
```

### Passo 9. Mensagens de erro na tela

Mostre o `erro` do hook. A API já manda o texto pronto em português:

| Código | Quando | O que o app faz |
| --- | --- | --- |
| 400 | Arquivo com extensão de imagem, mas corrompido | Pedir outra foto |
| 404 | Lição da trilha que não existe | Voltar para a lista |
| 413 | Foto maior que 5 MB, ou trechos e locais demais | Pedir foto menor; mandar só itens perto da rota |
| 415 | Arquivo que não é JPG, PNG ou WEBP | Pedir uma foto |
| 422 | JSON fora do formato | Erro do app: conferir os campos acima |
| 429 | Mais de 5 perguntas por minuto | "Espere um minuto" |
| 503 | Gemini sem chave, sem cota ou fora do ar | "Assistente indisponível, tente de novo" |

### Passo 10. O que o Isaac precisa confirmar com o João

1. Se consegue montar `trechos` e `locais` no formato da seção 6 (PROPOSTA).
2. Em que endereço o app vai ficar hospedado (para o CORS).

## Divergências a resolver

| Tema | Como está em `AGENTS.md` | Como está na API hoje | Por quê |
| --- | --- | --- | --- |
| Framework da API | Flask | FastAPI | Mudança feita em 12/11 |
| Valores do MQTT | livre, obstaculo, buraco | livre, buraco | Detecção de carro e moto removida em 12/11: o modelo marcava carros estacionados na rua |
| Horário na mensagem | resultado com o horário junto, para um retido antigo não valer como novo | só o texto, sem retain | Sem retain, o broker não guarda mensagem antiga, então o problema do retido antigo não acontece. Se o Caio ainda quiser o horário, combinamos o formato juntos |
| field6 = 4 (veículo) | existe | removido | mesmo motivo dos carros |
