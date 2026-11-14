"""Experimento de confianca do modelo de buracos v2, sem retreinar [HU-06, RF09].

Mede precisao, recall e F1 no MESMO conjunto de teste do treino (67 imagens do
"Pothole Dataset", KugaN, v2, CC BY 4.0, https://universe.roboflow.com/kugan/pothole-mftcc)
em 4 confiancas minimas: 0,25 (atual da API), 0,30, 0,35 e 0,40.
Tambem mostra o recall por tamanho do buraco (pequeno, medio, grande).

Acerto = caixa prevista que cobre pelo menos 50% de uma caixa real (IoU >= 0,5),
o mesmo criterio do mAP@50.

Como rodar (na pasta visao/):
    1. Crie o arquivo visao/treino/.env com a linha ROBOFLOW_API_KEY=sua_chave
       (o .env nao vai para o GitHub).
    2. api\\.venv\\Scripts\\python treino\\experimento_confianca.py
"""

import glob
import os

from dotenv import load_dotenv
from ultralytics import YOLO

PASTA = os.path.dirname(os.path.abspath(__file__))
CONFIG = {
    "modelo": os.path.join(PASTA, "..", "api", "modelos", "buracos.pt"),  # v2, o mesmo da API
    "pasta_dataset": os.path.join(PASTA, "dataset_teste"),  # fora do Git (.gitignore)
    "confiancas": (0.25, 0.30, 0.35, 0.40),
    "iou_minimo": 0.5,
    "limite_pequeno": 0.05,  # buraco ocupa menos de 5% da foto
    "limite_medio": 0.15,  # de 5% a 15%; acima disso, grande
}


def baixar_dataset() -> str:
    """Baixa o dataset do Roboflow (so na primeira vez) e devolve a pasta de teste."""
    pasta_teste = os.path.join(CONFIG["pasta_dataset"], "test")
    if os.path.isdir(pasta_teste):
        return pasta_teste
    load_dotenv(os.path.join(PASTA, ".env"))
    chave = os.getenv("ROBOFLOW_API_KEY")
    if not chave:
        raise SystemExit("Falta ROBOFLOW_API_KEY no arquivo visao/treino/.env")
    from roboflow import Roboflow

    projeto = Roboflow(api_key=chave).workspace("kugan").project("pothole-mftcc")
    projeto.version(2).download("yolov8", location=CONFIG["pasta_dataset"])
    return pasta_teste


def caixas_reais(pasta_teste: str, imagem: str, largura: int, altura: int) -> list[list[float]]:
    """Le o rotulo YOLO da imagem e devolve as caixas reais em pixels."""
    nome = os.path.splitext(os.path.basename(imagem))[0]
    arquivo = os.path.join(pasta_teste, "labels", nome + ".txt")
    caixas = []
    if not os.path.exists(arquivo):
        return caixas
    with open(arquivo, encoding="utf-8") as f:
        for linha in f:
            v = list(map(float, linha.split()[1:]))
            if len(v) == 4:  # caixa: centro x, centro y, largura, altura (0 a 1)
                xc, yc, bw, bh = v
                caixas.append([(xc - bw / 2) * largura, (yc - bh / 2) * altura,
                               (xc + bw / 2) * largura, (yc + bh / 2) * altura])
            elif len(v) > 4:  # poligono: vira a caixa que o envolve
                xs, ys = v[0::2], v[1::2]
                caixas.append([min(xs) * largura, min(ys) * altura,
                               max(xs) * largura, max(ys) * altura])
    return caixas


def iou(a: list[float], b: list[float]) -> float:
    """Sobreposicao entre duas caixas, de 0 (nada) a 1 (iguais)."""
    x1, y1 = max(a[0], b[0]), max(a[1], b[1])
    x2, y2 = min(a[2], b[2]), min(a[3], b[3])
    inter = max(0, x2 - x1) * max(0, y2 - y1)
    uniao = (a[2] - a[0]) * (a[3] - a[1]) + (b[2] - b[0]) * (b[3] - b[1]) - inter
    return inter / uniao if uniao else 0


def tamanho(caixa: list[float], largura: int, altura: int) -> str:
    """Classifica o buraco real pela area que ocupa na foto."""
    area = (caixa[2] - caixa[0]) * (caixa[3] - caixa[1]) / (largura * altura)
    if area < CONFIG["limite_pequeno"]:
        return "pequeno"
    return "medio" if area < CONFIG["limite_medio"] else "grande"


def avaliar(dados: list, conf: float) -> dict:
    """Conta acertos, falsos positivos e buracos perdidos numa confianca minima."""
    vp = fp = fn = 0
    por_tamanho = {"pequeno": [0, 0], "medio": [0, 0], "grande": [0, 0]}  # [achados, total]
    for reais, previstas, largura, altura in dados:
        usadas = set()
        for _, caixa in sorted([p for p in previstas if p[0] >= conf], reverse=True):
            melhor, indice = 0.0, -1
            for i, real in enumerate(reais):
                valor = iou(caixa, real)
                if i not in usadas and valor > melhor:
                    melhor, indice = valor, i
            if melhor >= CONFIG["iou_minimo"]:
                vp += 1
                usadas.add(indice)
            else:
                fp += 1
        fn += len(reais) - len(usadas)
        for i, real in enumerate(reais):
            t = tamanho(real, largura, altura)
            por_tamanho[t][1] += 1
            if i in usadas:
                por_tamanho[t][0] += 1
    precisao = vp / (vp + fp) if vp + fp else 0
    recall = vp / (vp + fn) if vp + fn else 0
    f1 = 2 * precisao * recall / (precisao + recall) if precisao + recall else 0
    return {"vp": vp, "fp": fp, "fn": fn, "precisao": precisao, "recall": recall,
            "f1": f1, "por_tamanho": por_tamanho}


def main() -> None:
    """Roda o modelo uma vez em 0,25 e filtra para as confiancas maiores (mesmo resultado)."""
    pasta_teste = baixar_dataset()
    imagens = sorted(glob.glob(os.path.join(pasta_teste, "images", "*")))
    print(len(imagens), "imagens de teste")
    modelo = YOLO(CONFIG["modelo"])
    menor = min(CONFIG["confiancas"])
    dados = []
    for imagem in imagens:
        r = modelo.predict(imagem, conf=menor, verbose=False)[0]
        altura, largura = r.orig_shape
        previstas = [(float(c), b.tolist()) for c, b in zip(r.boxes.conf, r.boxes.xyxy)]
        dados.append((caixas_reais(pasta_teste, imagem, largura, altura), previstas, largura, altura))

    print(f"{'Conf':<6}{'Precisao':>10}{'Recall':>9}{'F1':>8}{'VP':>5}{'FP':>5}{'FN':>5}"
          "   recall por tamanho (pequeno / medio / grande)")
    for conf in CONFIG["confiancas"]:
        m = avaliar(dados, conf)
        tam = " / ".join(f"{a}/{t}" for a, t in m["por_tamanho"].values())
        print(f"{conf:<6}{m['precisao']:>10.1%}{m['recall']:>9.1%}{m['f1']:>8.1%}"
              f"{m['vp']:>5}{m['fp']:>5}{m['fn']:>5}   {tam}")


if __name__ == "__main__":
    main()
