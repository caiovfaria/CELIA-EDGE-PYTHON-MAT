"""Banco de dados da camera do poste (SQLite) [HU-24, RF23].

SQLite e um banco que mora num arquivo so (dados/celia.db) e ja vem com o Python:
nao precisa instalar nem ligar nenhum servidor.

Duas tabelas:
- capturas: cada foto que o ESP32-CAM mandou (quando chegou, onde esta o arquivo e,
  depois da analise, o resultado do YOLO).
- reportes: os reports que o Gemini escreveu quando a camera encontrou buraco.

Cada funcao abre a sua propria conexao e fecha no fim. Assim duas requisicoes ao mesmo
tempo (ESP32 mandando foto e o site lendo a lista) nao dividem a mesma conexao.
"""
import os
import sqlite3

PASTA_DADOS = os.path.join(os.path.dirname(__file__), "dados")
CAMINHO_BANCO = os.getenv("BANCO_CAMINHO", os.path.join(PASTA_DADOS, "celia.db"))


def conectar() -> sqlite3.Connection:
    """Abre o banco. row_factory = Row deixa ler as colunas pelo nome (linha["poste"])."""
    conexao = sqlite3.connect(CAMINHO_BANCO)
    conexao.row_factory = sqlite3.Row
    return conexao


def criar_tabelas() -> None:
    """Cria o arquivo do banco e as tabelas, se ainda nao existirem. Roda quando a API liga."""
    os.makedirs(os.path.dirname(CAMINHO_BANCO), exist_ok=True)
    with conectar() as conexao:
        conexao.executescript(
            """
            CREATE TABLE IF NOT EXISTS capturas (
                id           INTEGER PRIMARY KEY AUTOINCREMENT,
                poste        TEXT NOT NULL,
                recebida_em  TEXT NOT NULL,       -- data e hora em UTC (ISO 8601)
                arquivo      TEXT NOT NULL,       -- caminho do JPEG dentro de capturas/
                resultado    TEXT,                -- "livre" ou "buraco" (vazio ate a analise)
                caixas       TEXT,                -- lista de caixas do YOLO, em JSON
                mqtt_enviado INTEGER,             -- 1 se avisou o ESP32 do Wokwi, 0 se nao
                largura      INTEGER,             -- tamanho da foto em pixels: o site precisa
                altura       INTEGER              -- dele para desenhar as caixas por cima
            );
            CREATE TABLE IF NOT EXISTS reportes (
                id          INTEGER PRIMARY KEY AUTOINCREMENT,
                captura_id  INTEGER NOT NULL REFERENCES capturas(id),
                poste       TEXT NOT NULL,
                texto       TEXT NOT NULL,        -- o report escrito pelo Gemini
                criado_em   TEXT NOT NULL
            );
            """
        )
        # Banco criado antes destas colunas existirem: acrescenta sem apagar o que ja tem
        colunas = {linha["name"] for linha in conexao.execute("PRAGMA table_info(capturas)")}
        for coluna in ("largura", "altura"):
            if coluna not in colunas:
                conexao.execute(f"ALTER TABLE capturas ADD COLUMN {coluna} INTEGER")
    conexao.close()


def inserir_captura(poste: str, recebida_em: str, arquivo: str, largura: int, altura: int) -> int:
    """Guarda uma captura nova (ainda sem analise) e devolve o id que o banco deu a ela.

    Os valores vao como parametros (?), nunca colados no texto do SQL: isso impede
    SQL injection, mesmo que alguem mande um nome de poste malicioso.
    """
    with conectar() as conexao:
        cursor = conexao.execute(
            "INSERT INTO capturas (poste, recebida_em, arquivo, largura, altura) VALUES (?, ?, ?, ?, ?)",
            (poste, recebida_em, arquivo, largura, altura),
        )
        novo_id = cursor.lastrowid
    conexao.close()
    return novo_id


def atualizar_analise(captura_id: int, resultado: str, caixas_json: str, mqtt_enviado: bool) -> None:
    """Grava na captura o que o YOLO encontrou (resultado e caixas) e se o MQTT saiu."""
    with conectar() as conexao:
        conexao.execute(
            "UPDATE capturas SET resultado = ?, caixas = ?, mqtt_enviado = ? WHERE id = ?",
            (resultado, caixas_json, int(mqtt_enviado), captura_id),
        )
    conexao.close()


def inserir_reporte(captura_id: int, poste: str, texto: str, criado_em: str) -> int:
    """Guarda um report escrito pelo Gemini e devolve o id dele."""
    with conectar() as conexao:
        cursor = conexao.execute(
            "INSERT INTO reportes (captura_id, poste, texto, criado_em) VALUES (?, ?, ?, ?)",
            (captura_id, poste, texto, criado_em),
        )
        novo_id = cursor.lastrowid
    conexao.close()
    return novo_id


def ultimo_reporte_em(poste: str) -> str | None:
    """Data e hora (ISO, UTC) do report mais recente deste poste, ou None se nunca houve."""
    with conectar() as conexao:
        linha = conexao.execute(
            "SELECT MAX(criado_em) AS ultimo FROM reportes WHERE poste = ?", (poste,)
        ).fetchone()
    conexao.close()
    return linha["ultimo"] if linha else None


def buscar_captura(captura_id: int) -> dict | None:
    """Devolve uma captura pelo id, como dicionario, ou None se nao existir."""
    with conectar() as conexao:
        linha = conexao.execute("SELECT * FROM capturas WHERE id = ?", (captura_id,)).fetchone()
    conexao.close()
    return dict(linha) if linha else None


def listar_capturas(poste: str, limite: int) -> list[dict]:
    """As capturas mais recentes de um poste, da mais nova para a mais velha."""
    with conectar() as conexao:
        linhas = conexao.execute(
            "SELECT * FROM capturas WHERE poste = ? ORDER BY id DESC LIMIT ?", (poste, limite)
        ).fetchall()
    conexao.close()
    return [dict(linha) for linha in linhas]


def listar_reportes(limite: int) -> list[dict]:
    """Os reports mais recentes (de todos os postes), cada um com os dados da sua captura.

    JOIN junta cada report com a linha da captura que ele cita (foto, caixas, tamanho).
    """
    with conectar() as conexao:
        linhas = conexao.execute(
            """
            SELECT r.id, r.captura_id, r.poste, r.texto, r.criado_em,
                   c.recebida_em, c.caixas, c.largura, c.altura
            FROM reportes AS r
            JOIN capturas AS c ON c.id = r.captura_id
            ORDER BY r.id DESC
            LIMIT ?
            """,
            (limite,),
        ).fetchall()
    conexao.close()
    return [dict(linha) for linha in linhas]
