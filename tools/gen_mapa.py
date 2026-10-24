#!/usr/bin/env python3
"""gen_mapa.py — genera docs/mapa/ (fichas por modulo + mapa de modulos).

Fuente: src/*.c (cabecera, funciones y globals A_* que usan), symbols/names.txt
(direccion de cada funcion) y test/test_*.py (que prueba cada funcion).

Uso:
    python3 tools/gen_mapa.py            # escribe docs/mapa/
    python3 tools/gen_mapa.py --check    # falla si docs/mapa/ no coincide

test/test_mapa.py regenera y compara (mantiene las fichas sincronizadas).
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
TEST = ROOT / "test"
NAMES = ROOT / "symbols" / "names.txt"
OUT = ROOT / "docs" / "mapa"

_FUNC_RE = re.compile(
    r"(?m)^(?!\s)(?!static\b)(?:void|int|uint32_t|int32_t|uint16_t|long)\s+\**\s*([a-z_]\w*)\s*\(")
_A_RE = re.compile(r"\bA_[A-Z0-9_]+\b")
_HDR_RE = re.compile(r"^/\*(.*?)\*/", re.S)


def _names():
    addr2name, name2addr = {}, {}
    for ln in NAMES.read_text(encoding="utf-8").splitlines():
        m = re.match(r"fn\s+0x([0-9a-fA-F]+)\s+(\S+)", ln)
        if m:
            addr2name[int(m.group(1), 16)] = m.group(2)
            name2addr.setdefault(m.group(2), int(m.group(1), 16))
    return addr2name, name2addr


def _func_spans(text):
    """[(nombre, inicio, fin)] de las definiciones no static."""
    spans = []
    for m in _FUNC_RE.finditer(text):
        i = text.find("{", m.end())
        if i < 0 or ";" in text[m.end():i]:
            continue
        depth, j = 0, i
        while j < len(text):
            if text[j] == "{":
                depth += 1
            elif text[j] == "}":
                depth -= 1
                if depth == 0:
                    break
            j += 1
        spans.append((m.group(1), m.start(), j + 1))
    return spans


def _preceding_comment(text, start):
    lines = text[:start].splitlines()
    i = len(lines) - 1
    while i >= 0 and lines[i].strip() == "":
        i -= 1
    block = []
    if i >= 0 and lines[i].strip().endswith("*/"):
        while i >= 0 and not lines[i].strip().startswith("/*"):
            block.insert(0, lines[i].strip())
            i -= 1
        if i >= 0:
            block.insert(0, lines[i].strip())
    txt = " ".join(re.sub(r"^[/*\s]+", "", x).rstrip("*/").strip() for x in block)
    txt = re.sub(r"^0x[0-9a-fA-F]+\s*[—-]\s*", "", txt)
    return txt


def _tests_for(func_name):
    hits = []
    for f in sorted(TEST.glob("test_*.py")):
        if re.search(r"\b" + re.escape(func_name) + r"\b", f.read_text(encoding="utf-8")):
            hits.append(f.stem)
    return hits


def generate_all():
    addr2name, name2addr = _names()
    files = sorted(SRC.glob("*.c"))
    result = {}
    index_rows = []
    for f in files:
        text = f.read_text(encoding="utf-8")
        hdr = _HDR_RE.match(text)
        intro = " ".join(re.sub(r"^[/*\s]+", "", l).rstrip("*/").strip()
                         for l in hdr.group(1).splitlines()) if hdr else ""
        rows = []
        globals_used = set()
        for name, s, e in _func_spans(text):
            body = text[s:e]
            globals_used |= set(_A_RE.findall(body))
            addr = name2addr.get(name)
            cmt = _preceding_comment(text, s)
            tests = _tests_for(name)
            rows.append((addr, name, cmt, tests))
        ficha = [f"# {f.name}", ""]
        if intro:
            ficha += [intro, ""]
        ficha += ["## Funciones", "", "| dir | nombre | qué hace | pruebas |",
                  "|---|---|---|---|"]
        for addr, name, cmt, tests in rows:
            a = f"0x{addr:x}" if addr is not None else "—"
            t = ", ".join(tests) if tests else "—"
            ficha.append(f"| `{a}` | `{name}` | {cmt or '—'} | {t} |")
        ficha += ["", "## Estado (globales A_* usadas)", ""]
        if globals_used:
            ficha += [f"- `{g}`" for g in sorted(globals_used)]
        else:
            ficha.append("(ninguna)")
        ficha.append("")
        result[OUT / f"{f.stem}.md"] = "\n".join(ficha) + "\n"
        tested = sum(1 for _, _, _, t in rows if t)
        index_rows.append((f.name, len(rows), len(globals_used), tested))

    idx = ["# Mapa de módulos", "",
           "Fichas por subsistema (generadas por `tools/gen_mapa.py`). Una por",
           "fichero de `src/`; cada una lista sus funciones (con dirección de",
           "origen), los globales `A_*` que toca y las pruebas que la cubren.", "",
           "| módulo | funciones | globales | con prueba | ficha |",
           "|---|---|---|---|---|"]
    for name, nf, ng, nt in index_rows:
        idx.append(f"| `{name}` | {nf} | {ng} | {nt} | [{name[:-2]}]({name[:-2]}.md) |")
    result[OUT / "README.md"] = "\n".join(idx) + "\n"
    return result


def main(argv):
    files = generate_all()
    if "--check" in argv:
        bad = [p for p, t in files.items() if not p.exists() or p.read_text(encoding="utf-8") != t]
        if bad:
            print("docs/mapa/ desactualizado:", ", ".join(p.name for p in bad), file=sys.stderr)
            print("    python3 tools/gen_mapa.py", file=sys.stderr)
            return 1
        return 0
    OUT.mkdir(parents=True, exist_ok=True)
    for p, t in files.items():
        p.write_text(t, encoding="utf-8")
    print(f"escritas {len(files)} fichas en docs/mapa/")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
