# C.E.L.I.A. · Programa de Lógica (Computational Thinking with Python)

Programa de terminal que lê as leituras do poste 01 no canal público do ThingSpeak, calcula o índice de risco de cada leitura e mostra a mesma faixa que o cidadão vê no app: **Acessível**, **Atenção** ou **Evite**.

- Canal público: https://thingspeak.mathworks.com/channels/3577155
- Rastreabilidade: HU-02, RF16, RN01

## Como rodar

```
cd python
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
| Distância livre na calçada (cm) | field4 | 120 cm | 90 cm (invertida) |

Fórmulas (a configuração fica no dicionário `CONFIG`):

```
nivel = (valor - min) / (max - min) * 100      (limitado entre 0 e 100)
risco = 0,5 * nivel_calor + 0,5 * nivel_calcada
```

**Por que esses valores:** são os mesmos limites do firmware do poste (`edge/celia_logica.h`), para o programa, o ESP32 e o app classificarem do mesmo jeito.

- **Calor:** 32 °C é onde o poste passa a mostrar Atenção, e 41 °C é onde passa a mostrar Evite. O índice de calor é calculado no ESP32 a partir da temperatura e da umidade (fórmula de Rothfusz, NOAA); ele representa a sensação térmica no corpo.
- **Calçada:** 120 cm é a faixa livre mínima da NBR 9050. Abaixo de 90 cm o poste considera a calçada bloqueada para cadeira de rodas. No `CONFIG` ela fica com mínimo 90 e máximo 120 e a marca `"invertida": True`: quanto menos espaço, maior o risco (o nível vira `100 - nível`).
- **Pesos iguais (0,5 e 0,5):** calor forte e calçada bloqueada impedem a passagem do mesmo jeito.

Faixas do risco:

| Risco | Faixa | Cor (a mesma do app) |
| --- | --- | --- |
| 0 | ✓ Acessível | verde #1E8E3E |
| acima de 0 e abaixo de 50 | ! Atenção | laranja #B26A00 |
| 50 ou mais | ✕ Evite | vermelho #C62828 |

Com pesos 0,5, uma medida sozinha no limite de Evite do poste já dá risco 50. Assim o programa concorda com o poste quando só uma medida está ruim. Quando as duas estão em Atenção ao mesmo tempo, o risco pode passar de 50 e virar Evite: o programa soma os dois problemas.

Configuração inválida (pesos que não somam 1, mínimo maior ou igual ao máximo, cortes fora de ordem, faixa sem aviso) dispara `ValueError`.

**Avisos:** cada faixa tem um aviso no `CONFIG`, com o mesmo texto que o app mostra. O resumo (opção 3) mostra a faixa da leitura mais recente e o aviso dela.

## Outras regras

- Leituras guardadas como dicionários `{id, horario, medidas, risco, faixa}`; o `id` é o `entry_id` do ThingSpeak.
- Campo vazio (sensor falhou) ou valor absurdo (registros antigos de teste) é descartado.
- Tendência: **PIORANDO** quando o risco sobe 3 vezes seguidas nas últimas leituras.
- Maior episódio em Evite: a maior sequência seguida de leituras em Evite.
- `historico.json` não repete leituras: os `entry_id` já salvos ficam num `set`.
- Plano B: toda busca que dá certo salva os dados em `backup.json`; se a internet ou o ThingSpeak falharem (timeout, status diferente de 200 ou `RequestException`), o programa usa esse arquivo.
- Nada derruba o programa:
    - **rede:** sem internet, timeout, status diferente de 200 ou resposta em formato estranho (ex.: `-1` de canal privado), usa o `backup.json`;
    - **arquivo:** `backup.json` ausente, corrompido ou com registros estranhos vira lista vazia (ou pula o registro) com aviso; `historico.json` corrompido é guardado como `historico_corrompido.json` e um novo começa; erro ao gravar (ex.: `relatorio.csv` aberto no Excel) mostra aviso;
    - **entrada:** opção inválida pede de novo, e Ctrl+C (no menu ou durante a busca) sai com "Até logo!" sem erro.

## Limitação

A foto da calçada analisada pela IA (buraco) entra no status do poste (field5 e field6), mas não entra neste índice: aqui só entram as duas medidas dos sensores.

## Print

(adicionar print do programa rodando)

## Uso de IA

Prompts em [prompts.md](prompts.md).
