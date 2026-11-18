# C.E.L.I.A. · Python do projeto (Lógica, API e IA)

Todo o Python da C.E.L.I.A. fica nesta pasta (a modelagem de Cálculo fica em `calculo/`). Desde 18/11/2026 ela junta as antigas pastas `python/` e `visao/`.

| Caminho | O que é |
| --- | --- |
| `main.py` | Programa da disciplina de Lógica (Computational Thinking with Python), explicado abaixo |
| `api/main.py` | API de visão computacional (FastAPI): `POST /analisar`, `GET /trilha` e publicação no MQTT |
| `api/assistente.py` | Assistente com o Gemini (LLM): `/assistente/trajeto` e `/assistente/descricao` |
| `api/poste.py` e `api/banco.py` | Fotos da câmera do poste (`/poste/...`) guardadas no SQLite |
| `api/modelos/buracos.pt` | Modelo próprio de buracos (YOLOv8, v2) |
| `treino/treino_buracos.ipynb` | Notebook de treino do modelo (Colab) |
| `STATUS.md` | Estado da visão computacional e como rodar a API |
| `prompts.md` | Registro dos prompts de IA de todas as partes desta pasta |

A chave do Gemini fica só no `api/.env` (modelo em `api/.env.example`), que nunca vai para o GitHub.

## Integrantes

| Nome completo | RM | Parte no projeto |
| --- | --- | --- |
| Caio Viana de Faria | 570634 | Edge (ESP32 no Wokwi e ThingSpeak), câmera do poste e Cálculo |
| Isaac Israel Rosa Coimbra | 570072 | Front-end (app React) e Web Development |
| João Thees Castro Santiago | 572829 | Python: programa de Lógica, API de visão computacional, modelo de buracos e assistente Gemini |

# Programa de Lógica (Computational Thinking with Python)

Programa de terminal que lê as leituras do poste 01 no canal público do ThingSpeak, calcula o índice de risco de cada leitura e mostra a mesma faixa que o cidadão vê no app: **Acessível**, **Atenção** ou **Evite**.

- Canal público: https://thingspeak.mathworks.com/channels/3577155
- Rastreabilidade: HU-02, RF16, RN01

## Como rodar

```
cd python-backend-IA
pip install -r requirements.txt
python main.py
```

Menu (repete até escolher Sair):

1. Buscar leituras do poste (ThingSpeak; se falhar, usa o `backup.json`)
2. Ver leituras e faixas
3. Ver resumo: contagem por faixa, tendência e maior episódio em Evite
4. Salvar `historico.json` (sem repetir leituras) e `relatorio.csv`
0. Sair

## Medidas e índice de risco

O programa usa 2 campos do canal:

| Medida | Campo | Nível 0 | Nível 100 |
| --- | --- | --- | --- |
| Índice de calor (°C) | field3 | 32 °C | 41 °C |
| Luminosidade (lux), medida pelo LDR | field7 | 10 lux ou mais | 0 lux (invertida) |

Fórmulas (a configuração fica no dicionário `CONFIG`):

```
nivel = (valor - min) / (max - min) * 100      (limitado entre 0 e 100)
risco = 0,5 * nivel_calor + 0,5 * nivel_luz
```

**Por que esses valores:** são os mesmos limites do firmware do poste (`edge/celia_logica.h`), para o programa, o ESP32 e o app classificarem do mesmo jeito.

- **Calor:** 32 °C é onde o poste passa a mostrar Atenção, e 41 °C é onde passa a mostrar Evite. O índice de calor é calculado no ESP32 a partir da temperatura e da umidade (fórmula de Rothfusz, NOAA); ele representa a sensação térmica no corpo.
- **Luz:** abaixo de 10 lux o poste considera a rua escura. No escuro a pessoa não vê buraco, degrau ou desnível na calçada. No `CONFIG` a luz fica com mínimo 0 e máximo 10 e a marca `"invertida": True`: quanto menos luz, maior o risco (o nível vira `100 - nível`). Ex.: 5 lux dá nível 50; 10 lux ou mais dá nível 0.
- **Pesos iguais (0,5 e 0,5):** calor forte e rua escura atrapalham o trajeto do mesmo jeito.
- **Regra do sol:** o poste deixa o trecho em Evite quando há sol direto (25.000 lux ou mais) junto com calor (índice de 32 °C ou mais): calçada sem sombra no calor. A fórmula linear só mede o escuro e não enxerga o sol, então o programa aplica essa regra depois da fórmula (função `aplicar_regra_sol`). O sol sozinho não piora o trecho, igual ao poste.

Desde 18/11/2026 o poste mede a luz (LDR, field7) no lugar da distância na calçada (HC-SR04, field4). O buraco na calçada é visto pela câmera com o modelo de IA da equipe.

Faixas do risco:

| Risco | Faixa | Cor (a mesma do app) |
| --- | --- | --- |
| 0 | ✓ Acessível | verde #1E8E3E |
| acima de 0 e abaixo de 50 | ! Atenção | laranja #B26A00 |
| 50 ou mais | ✕ Evite | vermelho #C62828 |

Nos 4 cenários do roteiro do poste (500 lux e 26 °C; 5 lux e 26 °C; 500 lux e 33 °C; 50.000 lux e 33 °C) o programa dá a mesma faixa que o LED: Acessível, Atenção, Atenção e Evite.

Diferença conhecida: o índice soma os problemas. Com uma medida no extremo (0 lux, escuro total, ou índice de calor de 41 °C ou mais), ou com as duas ruins ao mesmo tempo, o risco chega a 50 e o programa mostra Evite. O poste, nesses casos, mostra Atenção. A cor que o cidadão vê no app vem do status do poste (field5).

Configuração inválida (pesos que não somam 1, mínimo maior ou igual ao máximo, cortes fora de ordem, faixa sem aviso) dispara `ValueError`.

**Avisos:** cada faixa tem um aviso no `CONFIG`, com o mesmo texto que o app mostra. O resumo (opção 3) mostra a faixa da leitura mais recente e o aviso dela.

## Outras regras

- Leituras guardadas como dicionários `{id, horario, medidas, risco, faixa}`; o `id` é o `entry_id` do ThingSpeak.
- Campo vazio (sensor falhou) ou valor absurdo (registros antigos de teste) é descartado. Os registros 1 a 39 do canal são da versão antiga do poste (distância no field4, sem luz) e ficam de fora.
- Tendência: **PIORANDO** quando o risco sobe 3 vezes seguidas nas últimas leituras.
- Maior episódio em Evite: a maior sequência seguida de leituras em Evite.
- `historico.json` não repete leituras: os `entry_id` já salvos ficam num `set`.
- Plano B: toda busca que dá certo salva os dados em `backup.json`; se a internet ou o ThingSpeak falharem (timeout, status diferente de 200 ou `RequestException`), o programa usa esse arquivo.
- Nada derruba o programa:
    - **rede:** sem internet, timeout, status diferente de 200 ou resposta em formato estranho (ex.: `-1` de canal privado), usa o `backup.json`;
    - **arquivo:** `backup.json` ausente, corrompido ou com registros estranhos vira lista vazia (ou pula o registro) com aviso; `historico.json` corrompido é guardado como `historico_corrompido.json` e um novo começa; erro ao gravar (ex.: `relatorio.csv` aberto no Excel) mostra aviso;
    - **entrada:** opção inválida pede de novo, e Ctrl+C (no menu ou durante a busca) sai com "Até logo!" sem erro.

## Limitação

A foto da calçada analisada pela IA (buraco) entra no status do poste (field5 e field6), mas não entra neste índice: aqui só entram as duas medidas dos sensores (calor e luz).

## Print

(adicionar print do programa rodando)

## Uso de IA

Prompts em [prompts.md](prompts.md).
