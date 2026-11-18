"""
C.E.L.I.A. - Modelagem matematica (Differentiated Problem Solving)
=====================================================================
Este programa faz as contas do relatorio tecnico de calculo da equipe:

1. LIMITES   - a regra da calcada (120 cm e 90 cm) e descontinua: os limites
               laterais sao diferentes. Por isso o poste confirma uma mudanca
               por 15 s (demonstracao) ou 60 s antes de trocar a cor.
               Tambem mostra o comportamento extremo da formula do indice de
               calor (limite quando a temperatura vai ao infinito).
2. DERIVADAS - modelo do indice de calor ao longo do dia, IC(t). A derivada
               IC'(t) diz quando o calor sobe mais rapido e quando chega ao pico.
               As derivadas parciais dizem quanto o erro do DHT22 muda o IC.
3. INTEGRAIS - quanto calor acima de 32 C uma pessoa acumula esperando o
               onibus (em graus-hora), conforme o horario em que chega ao ponto.

Dados (pasta dados/):
- open_meteo_rio_verao_2023-2024.json: temperatura e umidade horarias do Rio
  (dados publicos da Open-Meteo, licenca CC BY 4.0).
- canal_3577155_registros_1a39.json: os 39 registros do canal ThingSpeak da
  equipe (valores simulados no Wokwi, nao sao medicoes reais).

Como rodar:   python calculo_celia.py
Precisa de:   numpy e matplotlib   (pip install numpy matplotlib)
Saidas:       resultados.txt e os graficos da pasta graficos/
"""

from __future__ import annotations

import json
import math
from pathlib import Path
from typing import Callable

import numpy as np
import matplotlib

matplotlib.use("Agg")  # gera os PNG sem abrir janela
import matplotlib.pyplot as plt
import matplotlib.ticker

PASTA = Path(__file__).resolve().parent
DADOS = PASTA / "dados"
GRAFICOS = PASTA / "graficos"

# ---------- Limites iguais aos do poste (edge/celia_logica.h) ----------
CALCADA_LIVRE_CM = 120.0     # a partir de 120 cm: calcada livre
CALCADA_ESTREITA_CM = 90.0   # de 90 a 119 cm: estreita; abaixo de 90: bloqueada
CALOR_ATENCAO_C = 32.0       # indice de calor a partir de 32 C: atencao
CALOR_PERIGO_C = 41.0        # a partir de 41 C: perigo
US_POR_CM = 58.8             # HC-SR04: microssegundos de eco por centimetro
FOLGA_ECO_CM = 0.08          # folga usada no poste antes de arredondar para baixo

# ---------- Erro do sensor DHT22 informado pelo fabricante ----------
ERRO_DHT22_T = 0.5   # +-0,5 C
ERRO_DHT22_U = 2.0   # +-2 % de umidade relativa

# ---------- Modelo do dia ----------
W = 2 * math.pi / 24   # um ciclo por dia, em radianos por hora
HARMONICOS = 2         # 1 = so o ciclo de 24 h; 2 = ciclo de 24 h + ciclo de 12 h
ESPERA_H = 0.25        # espera no ponto de onibus: 15 min = 0,25 h

COR_QUENTE = "#c0392b"   # dia da onda de calor
COR_MEDIO = "#2c6fbb"    # verao medio
COR_LIMIAR = "#7f8c8d"   # linhas de limite

saida: list[str] = []


def escrever(texto: str = "") -> None:
    """Mostra uma linha na tela e guarda para o resultados.txt."""
    print(texto)
    saida.append(texto)


def carregar_json(nome: str) -> dict:
    """Le um arquivo JSON da pasta dados/."""
    with open(DADOS / nome, encoding="utf-8") as arquivo:
        return json.load(arquivo)


# =====================================================================
# Indice de calor: a MESMA conta que o ESP32 faz (biblioteca DHTesp)
# =====================================================================
def indice_de_calor(t_c: float, u: float) -> float:
    """Indice de calor (graus C) a partir da temperatura (C) e da umidade (%).

    Formula do servico meteorologico dos EUA (NOAA): primeiro uma conta
    simples; se der 80 F ou mais, usa a regressao de Rothfusz com os ajustes
    de umidade muito baixa ou muito alta. A conta e feita em Fahrenheit.
    """
    t = t_c * 1.8 + 32.0
    hi = 0.5 * (t + 61.0 + (t - 68.0) * 1.2 + u * 0.094)
    if hi > 79:
        hi = (-42.379 + 2.04901523 * t + 10.14333127 * u - 0.22475541 * t * u
              - 0.00683783 * t ** 2 - 0.05481717 * u ** 2 + 0.00122874 * t ** 2 * u
              + 0.00085282 * t * u ** 2 - 0.00000199 * t ** 2 * u ** 2)
        if u < 13 and 80 <= t <= 112:
            hi -= ((13 - u) * 0.25) * math.sqrt((17 - abs(t - 95)) * 0.05882)
        elif u > 85 and 80 <= t <= 87:
            hi += ((u - 85) * 0.1) * ((87 - t) * 0.2)
    return (hi - 32.0) / 1.8


def conferir_formula_com_canal(canal: dict) -> int:
    """Compara a formula acima com o field3 que o ESP32 publicou. Devolve quantos pares bateram."""
    pares: dict[tuple[float, float], tuple[float, int]] = {}
    for registro in canal["feeds"]:
        if registro.get("field1") and registro.get("field3"):
            chave = (float(registro["field1"]), float(registro["field2"]))
            pares.setdefault(chave, (float(registro["field3"]), registro["entry_id"]))
    escrever("  T (C)  U (%)   IC do canal   IC calculado   registro")
    iguais = 0
    for (t_c, u), (ic_canal, entrada) in sorted(pares.items()):
        ic_calc = round(indice_de_calor(t_c, u), 1)
        iguais += ic_calc == ic_canal
        escrever(f"  {t_c:5.1f}  {u:5.1f}   {ic_canal:11.1f}   {ic_calc:12.1f}   {entrada}")
    escrever(f"  {iguais} de {len(pares)} pares diferentes batem com o ESP32 (1 casa decimal).")
    return iguais


# =====================================================================
# 1. LIMITES
# =====================================================================
def status_pela_distancia(d_cm: float) -> int:
    """Status do trecho so pela calcada, com calor normal: 0 acessivel, 1 atencao, 2 evite."""
    if d_cm >= CALCADA_LIVRE_CM:
        return 0
    if d_cm >= CALCADA_ESTREITA_CM:
        return 1
    return 2


def distancia_medida(eco_us: float) -> int:
    """Distancia que o poste registra a partir do tempo do eco (igual ao celia_logica.h)."""
    return math.floor(eco_us / US_POR_CM + FOLGA_ECO_CM)


def mostrar_limites_laterais() -> None:
    """Aproxima 120 cm e 90 cm pela esquerda e pela direita e mostra o status."""
    passos = [1, 0.1, 0.01, 0.001]
    for ponto in (CALCADA_LIVRE_CM, CALCADA_ESTREITA_CM):
        esquerda = [status_pela_distancia(ponto - h) for h in passos]
        direita = [status_pela_distancia(ponto + h) for h in passos]
        escrever(f"  d -> {ponto:.0f} pela esquerda (h = {passos}): status {esquerda}")
        escrever(f"  d -> {ponto:.0f} pela direita  (h = {passos}): status {direita}")
        escrever(f"  status no ponto: {status_pela_distancia(ponto)}  ->  limites laterais "
                 f"{esquerda[-1]} e {direita[-1]}: diferentes, entao nao existe limite em {ponto:.0f} cm")


def mostrar_oscilacao_perto_do_limite(d_real: float = 119.93) -> None:
    """Mostra que um ruido de poucos microssegundos no eco troca o status perto de 120 cm."""
    eco = d_real * US_POR_CM
    escrever(f"  Calcada real com {d_real} cm: eco ideal de {eco:.1f} us.")
    escrever("  ruido no eco (us)   distancia registrada (cm)   status")
    for ruido in (-3.0, -1.5, 0.0, 1.5, 3.0):
        d = distancia_medida(eco + ruido)
        escrever(f"  {ruido:17.1f}   {d:25d}   {status_pela_distancia(d)}")


def comportamento_extremo_do_ic(canal: dict) -> dict:
    """Limite do indice de calor quando T vai ao infinito (U fixa).

    Na regressao de Rothfusz (em F), o termo que manda quando T e enorme e o de T^2:
        c2(U) = -0.00683783 + 0.00122874*U - 0.00000199*U^2
    Se c2(U) > 0, IC -> +infinito; se c2(U) < 0, IC -> -infinito.
    """
    a, b, c = -0.00000199, 0.00122874, -0.00683783
    delta = b * b - 4 * a * c
    u_troca = (-b + math.sqrt(delta)) / (2 * a)   # a raiz que fica entre 0 e 100 %
    # Com U = 0, o IC (Rothfusz, sem ajuste fora de 80-112 F) e uma parabola em T:
    # derivada zero em T = 2.04901523 / (2 * 0.00683783) F
    t_max_f = 2.04901523 / (2 * 0.00683783)
    t_max_c = (t_max_f - 32) / 1.8
    escrever(f"  c2(U) muda de sinal em U = {u_troca:.2f} %.")
    escrever(f"  U < {u_troca:.1f} %: lim T->inf IC = -infinito;  U > {u_troca:.1f} %: lim T->inf IC = +infinito.")
    escrever(f"  Com U = 0 %, IC'(T) = 0 em T = {t_max_c:.1f} C (maximo): acima disso o IC CAI quando a temperatura sobe.")
    for t_c, u in ((60, 0), (65.5, 0), (70, 0), (80, 0), (80, 100)):
        escrever(f"    IC({t_c} C, {u} %) = {indice_de_calor(t_c, u):.1f} C")
    escrever("  No canal (testes de 12/11 com o DHT22 no maximo do Wokwi):")
    for registro in canal["feeds"]:
        if registro.get("field1") == "80.0":
            escrever(f"    registro {registro['entry_id']}: T = {registro['field1']} C, U = {registro['field2']} %"
                     f" -> IC = {registro['field3']} C")
    return {"u_troca": u_troca, "t_max_c": t_max_c}


# =====================================================================
# 2. MODELO DO DIA:  IC(t) = a0 + soma[ ak*cos(k*w*t) + bk*sin(k*w*t) ]
# =====================================================================
def ajustar_modelo(horas: np.ndarray, valores: np.ndarray, harmonicos: int) -> np.ndarray:
    """Ajusta o modelo por minimos quadrados. Devolve [a0, a1, b1, a2, b2, ...]."""
    colunas = [np.ones_like(horas)]
    for k in range(1, harmonicos + 1):
        colunas += [np.cos(k * W * horas), np.sin(k * W * horas)]
    matriz = np.column_stack(colunas)
    coef, *_ = np.linalg.lstsq(matriz, valores, rcond=None)
    return coef


def modelo(coef: np.ndarray, t):
    """IC(t) do modelo, em graus C (t em horas)."""
    total = coef[0] + 0 * np.asarray(t, dtype=float)
    for k in range(1, (len(coef) - 1) // 2 + 1):
        a, b = coef[2 * k - 1], coef[2 * k]
        total = total + a * np.cos(k * W * t) + b * np.sin(k * W * t)
    return total


def derivada(coef: np.ndarray, t):
    """IC'(t): taxa de variacao do indice de calor, em graus C por hora."""
    total = 0 * np.asarray(t, dtype=float)
    for k in range(1, (len(coef) - 1) // 2 + 1):
        a, b = coef[2 * k - 1], coef[2 * k]
        total = total + k * W * (-a * np.sin(k * W * t) + b * np.cos(k * W * t))
    return total


def segunda_derivada(coef: np.ndarray, t):
    """IC''(t), em graus C por hora ao quadrado."""
    total = 0 * np.asarray(t, dtype=float)
    for k in range(1, (len(coef) - 1) // 2 + 1):
        a, b = coef[2 * k - 1], coef[2 * k]
        total = total - (k * W) ** 2 * (a * np.cos(k * W * t) + b * np.sin(k * W * t))
    return total


def primitiva(coef: np.ndarray, t):
    """F(t), uma primitiva de IC(t): F'(t) = IC(t). Usada nas integrais exatas."""
    total = coef[0] * np.asarray(t, dtype=float)
    for k in range(1, (len(coef) - 1) // 2 + 1):
        a, b = coef[2 * k - 1], coef[2 * k]
        total = total + (a * np.sin(k * W * t) - b * np.cos(k * W * t)) / (k * W)
    return total


def erro_validacao_cruzada(horas: np.ndarray, valores: np.ndarray, harmonicos: int) -> float:
    """Erro medio (C) ao prever cada hora com um modelo ajustado SEM aquela hora (deixa um de fora)."""
    erros = []
    for i in range(len(horas)):
        resto = np.arange(len(horas)) != i
        coef = ajustar_modelo(horas[resto], valores[resto], harmonicos)
        erros.append(float(modelo(coef, horas[i])) - valores[i])
    return float(np.sqrt(np.mean(np.square(erros))))


def qualidade_do_ajuste(coef: np.ndarray, horas: np.ndarray, valores: np.ndarray) -> tuple[float, float]:
    """Devolve (R2, RMSE) do modelo contra os dados."""
    previsto = modelo(coef, horas)
    erro = valores - previsto
    r2 = 1 - np.sum(erro ** 2) / np.sum((valores - valores.mean()) ** 2)
    return float(r2), float(np.sqrt(np.mean(erro ** 2)))


def raizes(funcao: Callable, inicio: float, fim: float, passo: float = 0.01) -> list[float]:
    """Acha os pontos onde a funcao vale zero: procura troca de sinal e refina por bissecao."""
    achadas = []
    xs = np.arange(inicio, fim + passo / 2, passo)
    ys = [float(funcao(x)) for x in xs]
    for i in range(len(xs) - 1):
        a, b, fa, fb = xs[i], xs[i + 1], ys[i], ys[i + 1]
        if fa == 0:
            achadas.append(float(a))
        elif fa * fb < 0:
            for _ in range(60):
                meio = (a + b) / 2
                fm = float(funcao(meio))
                if fa * fm <= 0:
                    b = meio
                else:
                    a, fa = meio, fm
            achadas.append((a + b) / 2)
    return achadas


def br(numero: float, casas: int = 1) -> str:
    """Numero com virgula decimal, para os textos dos graficos (ex.: 65,5)."""
    return f"{numero:.{casas}f}".replace(".", ",")


def hora_texto(t: float) -> str:
    """Converte 13.5 em '13h30'."""
    minutos = int(round(t * 60))
    if minutos >= 24 * 60:
        return "24h00"
    return f"{minutos // 60:02d}h{minutos % 60:02d}"


def intervalos_acima(coef: np.ndarray, limiar: float, inicio: float = 0.0, fim: float = 24.0) -> list[tuple[float, float]]:
    """Intervalos de [inicio, fim] em que IC(t) >= limiar."""
    cortes = [inicio] + raizes(lambda x: modelo(coef, x) - limiar, inicio, fim) + [fim]
    intervalos = []
    for a, b in zip(cortes[:-1], cortes[1:]):
        if b > a and modelo(coef, (a + b) / 2) > limiar:
            intervalos.append((a, b))
    return intervalos


# =====================================================================
# 3. INTEGRAIS
# =====================================================================
def excesso_exato(coef: np.ndarray, inicio: float, fim: float, limiar: float = CALOR_ATENCAO_C) -> float:
    """Integral de max(IC(t) - limiar, 0) de inicio a fim, pelo Teorema Fundamental do Calculo.

    So conta os trechos em que IC esta acima do limiar; em cada um usa
    integral = [F(b) - F(a)] - limiar * (b - a).
    """
    total = 0.0
    for a, b in intervalos_acima(coef, limiar, inicio, fim):
        total += float(primitiva(coef, b) - primitiva(coef, a)) - limiar * (b - a)
    return total


def trapezios(valores: np.ndarray, passo: float) -> float:
    """Regra dos trapezios: soma das areas (y[i] + y[i+1]) / 2 * passo."""
    return float(sum((valores[i] + valores[i + 1]) / 2 * passo for i in range(len(valores) - 1)))


def valor_medio(coef: np.ndarray, inicio: float, fim: float) -> float:
    """Valor medio de IC(t) no intervalo: (1 / (fim - inicio)) * integral de IC(t) dt."""
    return float(primitiva(coef, fim) - primitiva(coef, inicio)) / (fim - inicio)


def simpson(funcao: Callable, inicio: float, fim: float, partes: int = 2400) -> float:
    """Regra de Simpson (partes par): conferencia numerica da integral exata."""
    h = (fim - inicio) / partes
    xs = inicio + h * np.arange(partes + 1)
    ys = np.array([float(funcao(x)) for x in xs])
    return float(h / 3 * (ys[0] + ys[-1] + 4 * ys[1:-1:2].sum() + 2 * ys[2:-1:2].sum()))


# =====================================================================
# Sensibilidade: derivadas parciais do indice de calor
# =====================================================================
def derivadas_parciais(t_c: float, u: float) -> tuple[float, float]:
    """(dIC/dT, dIC/dU) exatas na regressao de Rothfusz, em C/C e C/%.

    Vale onde a regressao e usada sem ajuste (13 % <= U <= 85 %).
    Regra da cadeia: T_F = 1,8*T_C + 32 e IC_C = (HI_F - 32) / 1,8.
    """
    t = t_c * 1.8 + 32.0
    d_hi_dt = (2.04901523 - 0.22475541 * u - 2 * 0.00683783 * t + 2 * 0.00122874 * t * u
               + 0.00085282 * u ** 2 - 2 * 0.00000199 * t * u ** 2)
    d_hi_du = (10.14333127 - 0.22475541 * t - 2 * 0.05481717 * u + 0.00122874 * t ** 2
               + 2 * 0.00085282 * t * u - 2 * 0.00000199 * t ** 2 * u)
    return d_hi_dt * 1.8 / 1.8, d_hi_du / 1.8


def analisar_sensibilidade(t_c: float, u: float, rotulo: str) -> None:
    """Mostra as derivadas parciais e o erro que o DHT22 provoca no IC."""
    d_t, d_u = derivadas_parciais(t_c, u)
    h = 1e-4   # conferencia: quociente de diferencas centrado
    n_t = (indice_de_calor(t_c + h, u) - indice_de_calor(t_c - h, u)) / (2 * h)
    n_u = (indice_de_calor(t_c, u + h) - indice_de_calor(t_c, u - h)) / (2 * h)
    erro = abs(d_t) * ERRO_DHT22_T + abs(d_u) * ERRO_DHT22_U
    ic = indice_de_calor(t_c, u)
    escrever(f"  {rotulo}: T = {t_c} C, U = {u} %  ->  IC = {ic:.1f} C")
    escrever(f"    dIC/dT = {d_t:.3f} C por C (numerico {n_t:.3f});  dIC/dU = {d_u:.3f} C por % (numerico {n_u:.3f})")
    escrever(f"    erro do DHT22 (+-{ERRO_DHT22_T} C, +-{ERRO_DHT22_U} %): dIC ~ {abs(d_t):.2f}*{ERRO_DHT22_T}"
             f" + {abs(d_u):.2f}*{ERRO_DHT22_U} = {erro:.2f} C  ->  IC entre {ic - erro:.1f} e {ic + erro:.1f} C")


# =====================================================================
# Graficos
# =====================================================================
def estilo(eixo) -> None:
    """Deixa o grafico limpo: sem bordas de cima e da direita, grade leve."""
    eixo.spines["top"].set_visible(False)
    eixo.spines["right"].set_visible(False)
    eixo.grid(True, color="#e5e5e5", linewidth=0.8)
    eixo.set_axisbelow(True)
    virgula = matplotlib.ticker.FuncFormatter(lambda v, _: f"{v:g}".replace(".", ","))
    eixo.yaxis.set_major_formatter(virgula)   # numeros dos eixos com virgula decimal


def grafico_limites() -> None:
    """Status x distancia: degraus em 90 e 120 cm com os limites laterais marcados."""
    fig, ax = plt.subplots(figsize=(8, 4.2))
    cores = {0: "#27ae60", 1: "#e1a100", 2: COR_QUENTE}
    trechos = [(60, 90, 2), (90, 120, 1), (120, 160, 0)]
    for a, b, s in trechos:
        ax.plot([a, b], [s, s], color=cores[s], linewidth=3)
    for x, s_esq, s_dir in ((90, 2, 1), (120, 1, 0)):
        ax.plot(x, s_esq, "o", markersize=9, markerfacecolor="white", markeredgecolor=cores[s_esq], markeredgewidth=2)
        ax.plot(x, s_dir, "o", markersize=9, color=cores[s_dir])
        ax.plot([x, x], [s_dir, s_esq], ":", color=COR_LIMIAR)
    ax.annotate("lim d->120⁻ = 1 (atenção)\nlim d->120⁺ = 0 (acessível)", xy=(120, 0.5), xytext=(126, 1.25),
                fontsize=9, arrowprops=dict(arrowstyle="->", color=COR_LIMIAR))
    ax.annotate("lim d->90⁻ = 2 (evite)\nlim d->90⁺ = 1 (atenção)", xy=(90, 1.5), xytext=(63, 0.55),
                fontsize=9, arrowprops=dict(arrowstyle="->", color=COR_LIMIAR))
    for d, texto in ((89, "Wokwi: 89 cm"), (119, "Wokwi: 119 cm"), (120, "Wokwi: 120 cm")):
        s = status_pela_distancia(d)
        ax.plot(d, s, "s", color="black", markersize=4)
        a_direita = d == 120
        ax.text(d + (1 if a_direita else -1), s - 0.17, texto, fontsize=8,
                ha="left" if a_direita else "right", va="top")
    ax.set_xlim(60, 160)
    ax.set_ylim(-0.5, 2.5)
    estilo(ax)
    ax.set_yticks([0, 1, 2], ["0 acessível", "1 atenção", "2 evite"])
    ax.set_xlabel("largura livre da calçada, d (cm)")
    ax.set_title("A regra da calçada é descontínua em 90 cm e 120 cm (calor normal)", fontsize=11, loc="left")
    fig.tight_layout()
    fig.savefig(GRAFICOS / "fig2_limites_calcada.png", dpi=150)
    plt.close(fig)


def grafico_extremo(canal: dict, extremo: dict) -> None:
    """IC(T) para U fixa: a formula explode ou despenca fora da faixa real."""
    fig, ax = plt.subplots(figsize=(8, 4.4))
    ts = np.linspace(20, 80, 601)
    for u, tom in ((0, 0.35), (50, 0.65), (100, 1.0)):
        ax.plot(ts, [indice_de_calor(t, u) for t in ts], color=COR_QUENTE, alpha=tom, linewidth=2)
        ax.text(80.6, indice_de_calor(80, u), f"U = {u} %", va="center", fontsize=9, color=COR_QUENTE, alpha=max(tom, 0.6))
    ax.axvspan(20, 43, color="#dfe9f5")
    ax.text(21, 640, "faixa possível no Rio\n(até ~43 °C)", fontsize=9, color=COR_MEDIO, va="top")
    for registro in canal["feeds"]:
        if registro.get("field1") == "80.0" and registro["entry_id"] in (9, 10):
            ic = float(registro["field3"])
            ax.plot(80, ic, "s", color="black", markersize=5)
            ax.text(78.5, ic + (25 if ic > 100 else 30), f"registro {registro['entry_id']}: {br(ic)} °C",
                    fontsize=8, ha="right")
    t_max = extremo["t_max_c"]
    ax.plot(t_max, indice_de_calor(t_max, 0), "o", color="black", markersize=4)
    ax.annotate(f"U = 0 %: máximo em {br(t_max)} °C\n(depois o IC cai)", xy=(t_max, indice_de_calor(t_max, 0)),
                xytext=(61, 95), fontsize=8, arrowprops=dict(arrowstyle="->", color=COR_LIMIAR))
    ax.set_xlim(20, 86)
    ax.set_ylim(0, 750)
    ax.set_xlabel("temperatura do ar, T (°C)")
    ax.set_ylabel("índice de calor calculado (°C)")
    ax.set_title("Fora da faixa real, a fórmula do índice de calor perde o sentido", fontsize=11, loc="left")
    estilo(ax)
    fig.tight_layout()
    fig.savefig(GRAFICOS / "fig3_ic_extremo.png", dpi=150)
    plt.close(fig)


def grafico_modelo(dias: list[dict]) -> None:
    """Dados horarios do IC e o modelo ajustado para os dois dias."""
    fig, ax = plt.subplots(figsize=(8, 4.4))
    fino = np.linspace(0, 24, 481)
    for dia in dias:
        ax.plot(dia["horas"], dia["ic"], "o", color=dia["cor"], markersize=4)
        ax.plot(fino, modelo(dia["coef"], fino), color=dia["cor"], linewidth=2,
                label=f"{dia['nome']} (R² = {br(dia['r2'], 2)})")
    for limiar, texto in ((CALOR_ATENCAO_C, "32 °C: atenção"), (CALOR_PERIGO_C, "41 °C: perigo")):
        ax.axhline(limiar, color=COR_LIMIAR, linestyle="--", linewidth=1)
        ax.text(23.8, limiar + 0.4, texto, fontsize=8, color=COR_LIMIAR, ha="right")
    ax.set_xlim(0, 24)
    ax.set_xticks(range(0, 25, 3))
    ax.set_xlabel("hora do dia, t (h)")
    ax.set_ylabel("índice de calor, IC (°C)")
    ax.set_title("Índice de calor ao longo do dia: dados (pontos) e modelo IC(t) (linha)", fontsize=11, loc="left")
    ax.legend(frameon=False, loc="upper left", fontsize=9)
    estilo(ax)
    fig.tight_layout()
    fig.savefig(GRAFICOS / "fig1_modelo_ic.png", dpi=150)
    plt.close(fig)


def grafico_derivada(dias: list[dict]) -> None:
    """IC'(t): onde e positiva o calor sobe; o zero marca o pico."""
    fig, ax = plt.subplots(figsize=(8, 4.4))
    fino = np.linspace(0, 24, 481)
    ax.axhline(0, color="black", linewidth=0.8)
    for dia in dias:
        ax.plot(fino, derivada(dia["coef"], fino), color=dia["cor"], linewidth=2, label=dia["nome"])
        tp, ts = dia["pico"], dia["subida"]
        quente = dia["cor"] == COR_QUENTE
        ax.plot(tp, 0, "o", color=dia["cor"])
        ax.text(tp - 0.3 if quente else tp + 0.3, -0.45 if quente else 0.3, f"pico {hora_texto(tp)}",
                fontsize=8, color=dia["cor"], ha="right" if quente else "left")
        taxa = float(derivada(dia["coef"], ts))
        ax.plot(ts, taxa, "o", color=dia["cor"])
        ax.annotate(f"sobe mais rápido às {hora_texto(ts)}\n{'+' if taxa > 0 else ''}{br(taxa)} °C/h",
                    xy=(ts, taxa), xytext=(9.2, 3.55) if quente else (0.3, 2.15), fontsize=8, color=dia["cor"],
                    arrowprops=None if quente else dict(arrowstyle="->", color=dia["cor"]))
    ax.set_xlim(0, 24)
    ax.set_xticks(range(0, 25, 3))
    ax.set_xlabel("hora do dia, t (h)")
    ax.set_ylim(-3.2, 4.3)
    ax.set_ylabel("IC'(t) (°C por hora)")
    ax.set_title("Derivada do índice de calor: quanto ele sobe ou desce por hora", fontsize=11, loc="left")
    ax.legend(frameon=False, loc="lower left", fontsize=9)
    estilo(ax)
    fig.tight_layout()
    fig.savefig(GRAFICOS / "fig4_derivada.png", dpi=150)
    plt.close(fig)


def grafico_integral(dias: list[dict], horarios: np.ndarray) -> None:
    """(a) area acima de 32 C no dia quente, com duas esperas de 15 min; (b) exposicao por horario."""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(11, 4.4))
    fino = np.linspace(0, 24, 961)
    quente = dias[0]
    ic = modelo(quente["coef"], fino)
    ax1.plot(fino, ic, color=COR_QUENTE, linewidth=2)
    ax1.fill_between(fino, CALOR_ATENCAO_C, ic, where=ic > CALOR_ATENCAO_C, color=COR_QUENTE, alpha=0.15)
    ax1.axhline(CALOR_ATENCAO_C, color=COR_LIMIAR, linestyle="--", linewidth=1)
    ax1.text(0.2, CALOR_ATENCAO_C - 1.3, "32 °C", fontsize=8, color=COR_LIMIAR)
    for t0 in (9.0, 14.0):
        janela = np.linspace(t0, t0 + ESPERA_H, 30)
        ax1.fill_between(janela, CALOR_ATENCAO_C, modelo(quente["coef"], janela), color=COR_QUENTE, alpha=0.75)
        e = excesso_exato(quente["coef"], t0, t0 + ESPERA_H)
        ax1.annotate(f"espera às {hora_texto(t0)}\n{br(e, 2)} °C·h", xy=(t0 + 0.12, CALOR_ATENCAO_C + 1),
                     xytext=(t0 - 4.2 if t0 < 12 else t0 + 1.2, 25.5), fontsize=8,
                     arrowprops=dict(arrowstyle="->", color=COR_LIMIAR))
    ax1.set_xlim(0, 24)
    ax1.set_ylim(24, 50)
    ax1.set_xticks(range(0, 25, 3))
    ax1.set_xlabel("hora do dia, t (h)")
    ax1.set_ylabel("IC(t) (°C)")
    ax1.set_title(f"(a) {quente['nome']}: área acima de 32 °C", fontsize=10, loc="left")
    estilo(ax1)
    for dia in dias:
        ax2.plot(horarios, dia["exposicao"], color=dia["cor"], linewidth=2, label=dia["nome"])
    ax2.set_xlim(horarios[0], horarios[-1])
    ax2.set_xticks(range(6, 19, 2))
    ax2.set_xlabel("horário em que chega ao ponto (h)")
    ax2.set_ylabel("exposição numa espera de 15 min (°C·h)")
    ax2.set_title("(b) Calor acumulado acima de 32 °C por horário de chegada", fontsize=10, loc="left")
    ax2.legend(frameon=False, loc="center", bbox_to_anchor=(0.5, 0.27), fontsize=9)
    estilo(ax2)
    fig.tight_layout()
    fig.savefig(GRAFICOS / "fig5_integral_exposicao.png", dpi=150)
    plt.close(fig)


# =====================================================================
# Programa principal
# =====================================================================
def preparar_dia(nome: str, serie: dict, cor: str) -> dict:
    """Calcula o IC horario de um dia e ajusta o modelo IC(t)."""
    horas = np.array(serie["hora"], dtype=float)
    ic = np.array([indice_de_calor(t, u) for t, u in zip(serie["temperatura_c"], serie["umidade_pct"])])
    coef = ajustar_modelo(horas, ic, HARMONICOS)
    r2, rmse = qualidade_do_ajuste(coef, horas, ic)
    return {"nome": nome, "cor": cor, "horas": horas, "ic": ic, "coef": coef, "r2": r2, "rmse": rmse}


def main() -> None:
    GRAFICOS.mkdir(exist_ok=True)
    clima = carregar_json("open_meteo_rio_verao_2023-2024.json")
    canal = carregar_json("canal_3577155_registros_1a39.json")

    escrever("C.E.L.I.A. - modelagem matematica")
    escrever("=" * 60)
    escrever("\n[0] A formula do indice de calor e a mesma do ESP32?")
    conferir_formula_com_canal(canal)

    # ---------------- 1. Limites ----------------
    escrever("\n[1a] Limites laterais da regra da calcada (calor normal)")
    mostrar_limites_laterais()
    escrever("\n[1b] Perto do limite, um ruido minimo troca o status")
    mostrar_oscilacao_perto_do_limite()
    escrever("\n[1c] Comportamento extremo do indice de calor (limite no infinito)")
    extremo = comportamento_extremo_do_ic(canal)

    # ---------------- 2. Modelo e derivadas ----------------
    quente = preparar_dia("18/11/2023 (onda de calor)", clima["dia_mais_quente"], COR_QUENTE)
    medio = preparar_dia("verão médio (dez/2023 a fev/2024)", clima["verao_medio"], COR_MEDIO)
    dias = [quente, medio]

    escrever("\n[2a] Modelo IC(t) = a0 + a1 cos(wt) + b1 sen(wt) + a2 cos(2wt) + b2 sen(2wt),  w = 2*pi/24")
    for dia in dias:
        c = dia["coef"]
        escrever(f"  {dia['nome']}: IC horario de {dia['ic'].min():.1f} a {dia['ic'].max():.1f} C "
                 f"(maximo as {int(dia['horas'][dia['ic'].argmax()])}h)")
        escrever(f"    a0 = {c[0]:.3f}  a1 = {c[1]:.3f}  b1 = {c[2]:.3f}  a2 = {c[3]:.3f}  b2 = {c[4]:.3f}")
        escrever("    harmonicos   R2      erro medio (C)   erro deixando uma hora de fora (C)")
        for k in (1, 2, 3, 4):
            cf = ajustar_modelo(dia["horas"], dia["ic"], k)
            r2, rmse = qualidade_do_ajuste(cf, dia["horas"], dia["ic"])
            loo = erro_validacao_cruzada(dia["horas"], dia["ic"], k)
            marca = "  <- usado" if k == HARMONICOS else ""
            escrever(f"    {k:10d}   {r2:.3f}   {rmse:14.2f}   {loo:34.2f}{marca}")

    escrever("\n[2b] Derivada: pico (IC' = 0 e IC'' < 0) e subida mais rapida (IC'' = 0 e IC' maximo)")
    for dia in dias:
        c = dia["coef"]
        criticos = raizes(lambda x: derivada(c, x), 0, 24)
        maximos = [t for t in criticos if segunda_derivada(c, t) < 0]
        dia["pico"] = max(maximos, key=lambda t: float(modelo(c, t)))
        inflexoes = raizes(lambda x: segunda_derivada(c, x), 0, 24)
        dia["subida"] = max(inflexoes, key=lambda t: float(derivada(c, t)))
        dia["descida"] = min(inflexoes, key=lambda t: float(derivada(c, t)))
        escrever(f"  {dia['nome']}:")
        escrever(f"    pico as {hora_texto(dia['pico'])}: IC = {float(modelo(c, dia['pico'])):.1f} C,"
                 f" IC'' = {float(segunda_derivada(c, dia['pico'])):.2f} C/h2 (< 0, e maximo)")
        escrever(f"    sobe mais rapido as {hora_texto(dia['subida'])}: IC' = {float(derivada(c, dia['subida'])):+.2f} C/h")
        escrever(f"    desce mais rapido as {hora_texto(dia['descida'])}: IC' = {float(derivada(c, dia['descida'])):+.2f} C/h")
        for limiar, nome in ((CALOR_ATENCAO_C, "atencao (>= 32 C)"), (CALOR_PERIGO_C, "perigo (>= 41 C)")):
            faixas = intervalos_acima(c, limiar)
            texto = ", ".join(f"{hora_texto(a)} a {hora_texto(b)}" for a, b in faixas) or "nunca"
            horas = sum(b - a for a, b in faixas)
            escrever(f"    {nome}: {texto}  ({horas:.1f} h)")

    escrever("\n[2c] Limite do quociente de diferencas -> derivada (dia quente, na hora da subida mais rapida)")
    c, t0 = quente["coef"], quente["subida"]
    escrever(f"  t0 = {hora_texto(t0)};  IC'(t0) pela formula = {float(derivada(c, t0)):.4f} C/h")
    escrever("        h (horas)   [IC(t0+h) - IC(t0)] / h")
    for h in (1, 0.5, 0.25, 0.1, 0.01, 0.001):
        q = (float(modelo(c, t0 + h)) - float(modelo(c, t0))) / h
        escrever(f"  {h:13g}   {q:.4f}")

    escrever("\n[2d] Derivadas parciais: quanto o erro do DHT22 mexe no IC")
    t_dados = clima["dia_mais_quente"]["temperatura_c"]
    u_dados = clima["dia_mais_quente"]["umidade_pct"]
    analisar_sensibilidade(t_dados[9], u_dados[9], "18/11/2023 as 9h (perto de 41 C)")
    analisar_sensibilidade(t_dados[6], u_dados[6], "18/11/2023 as 6h (perto de 32 C)")

    # ---------------- 3. Integrais ----------------
    escrever("\n[3a] Calor acumulado acima de 32 C no dia (graus-hora), de 0h a 23h")
    for dia in dias:
        exato = excesso_exato(dia["coef"], 0, 23)
        conf = simpson(lambda x: max(float(modelo(dia["coef"], x)) - CALOR_ATENCAO_C, 0.0), 0, 23)
        trap = trapezios(np.maximum(dia["ic"] - CALOR_ATENCAO_C, 0), 1.0)
        dia["excesso_dia"] = exato
        escrever(f"  {dia['nome']}: exata (modelo) = {exato:.2f} C.h;  Simpson (modelo) = {conf:.2f};"
                 f"  trapezios (dados horarios) = {trap:.2f}")
    razao = quente["excesso_dia"] / medio["excesso_dia"]
    escrever(f"  o dia da onda de calor acumulou {razao:.0f} vezes o calor acima de 32 C do verao medio")

    escrever("\n[3b] Espera de 15 min no ponto: integral de max(IC - 32, 0) de t0 a t0 + 0,25 h")
    horarios = np.arange(6.0, 18.01, 0.25)
    for dia in dias:
        dia["exposicao"] = np.array([excesso_exato(dia["coef"], t, t + ESPERA_H) for t in horarios])
        melhor = horarios[int(np.argmax(dia["exposicao"]))]
        escrever(f"  {dia['nome']}: pior horario para chegar = {hora_texto(melhor)}"
                 f" ({dia['exposicao'].max():.2f} C.h)")
        for t in (7.0, 9.0, 12.0, 14.0, 17.0):
            e = excesso_exato(dia["coef"], t, t + ESPERA_H)
            media = valor_medio(dia["coef"], t, t + ESPERA_H)
            escrever(f"    chegando as {hora_texto(t)}: {e:.2f} C.h  (IC medio na espera = {media:.1f} C)")

    # ---------------- Graficos e arquivo ----------------
    grafico_limites()
    grafico_extremo(canal, extremo)
    grafico_modelo(dias)
    grafico_derivada(dias)
    grafico_integral(dias, horarios)
    escrever("\nGraficos salvos em graficos/: fig1 a fig5.")
    (PASTA / "resultados.txt").write_text("\n".join(saida) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
