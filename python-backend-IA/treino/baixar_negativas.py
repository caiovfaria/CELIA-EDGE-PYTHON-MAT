import urllib.error, json, os, re, sys, time, urllib.parse, urllib.request

DESTINO = sys.argv[1]
os.makedirs(DESTINO, exist_ok=True)
UA = {"User-Agent": "celia-gs-student-project/0.1 (educational)"}
CATS = ["Category:Portuguese pavement in Rio de Janeiro",
        "Category:Calçadão de Copacabana",
        "Category:Calçadão de Ipanema"]

def api(params: dict) -> dict:
    """Consulta a API do Wikimedia Commons e devolve a resposta JSON."""
    url = "https://commons.wikimedia.org/w/api.php?" + urllib.parse.urlencode(params)
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=30) as r:
        return json.load(r)

def limpa(html: str | None) -> str:
    """Tira as tags HTML do texto de autor e licenca."""
    return re.sub(r"<[^>]+>", "", html or "").strip()

itens = {}
for cat in CATS:
    d = api({"action": "query", "format": "json", "generator": "categorymembers",
             "gcmtitle": cat, "gcmtype": "file", "gcmlimit": "100",
             "prop": "imageinfo", "iiprop": "url|mime|extmetadata", "iiurlwidth": "1024"})
    for p in d.get("query", {}).get("pages", {}).values():
        ii = p["imageinfo"][0]
        if ii["mime"] not in ("image/jpeg", "image/png"):
            continue
        m = ii.get("extmetadata", {})
        itens[p["title"]] = {
            "categoria": cat, "thumb": ii.get("thumburl") or ii["url"],
            "pagina": ii["descriptionurl"],
            "autor": limpa(m.get("Artist", {}).get("value")),
            "licenca": limpa(m.get("LicenseShortName", {}).get("value")),
        }

for i, (titulo, it) in enumerate(sorted(itens.items()), 1):
    nome = f"wm_{i:02d}.jpg"
    it["arquivo"] = nome
    alvo = os.path.join(DESTINO, nome)
    if os.path.exists(alvo):
        continue
    for tentativa in range(5):
        try:
            with urllib.request.urlopen(urllib.request.Request(it["thumb"], headers=UA), timeout=60) as r:
                open(alvo, "wb").write(r.read())
            break
        except urllib.error.HTTPError as e:
            if e.code != 429:
                raise
            time.sleep(10 * (tentativa + 1))
    time.sleep(3)

json.dump(itens, open(os.path.join(DESTINO, "_fontes.json"), "w", encoding="utf-8"),
          ensure_ascii=False, indent=1)
print(len(itens), "arquivos baixados")
