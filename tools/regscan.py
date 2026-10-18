#!/usr/bin/env python3
"""regscan.py — mide la deuda de legibilidad por módulo (seguimiento de Fase B/C).

Cuenta, por fichero de src/ (solo código, sin comentarios):
  - `ids_reg`: ocurrencias de identificadores tipo registro (`d0`..`d7`, `a0`..`a6`
    y variantes con sufijo `d2b`, `a0b`; `u1`, `u2`…).
  - `goto`: sentencias `goto` (flujo heredado del ASM).

Uso:
    python3 tools/regscan.py            # tabla por módulo
    python3 tools/regscan.py --total    # solo totales
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"

_IDENT = re.compile(r"\b([da][0-7][a-z]?|u\d+[a-z]?)\b")
_SIG = re.compile(
    r"(?m)^(?!\s)(?!static\b)(?:void|int|uint32_t|int32_t|uint16_t|long)\s+\**\s*([a-z_]\w*)\s*\(")


def _strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def _functions(text: str) -> int:
    n = 0
    for m in _SIG.finditer(text):
        i = text.find("{", m.end())
        if i >= 0 and ";" not in text[m.end():i]:
            n += 1
    return n


def scan() -> list[tuple[str, int, int, int, int]]:
    rows = []
    for f in sorted(SRC.glob("*.c")):
        text = f.read_text(encoding="utf-8")
        code = _strip_comments(text)
        rows.append((f.name, text.count("\n"), _functions(text),
                     len(_IDENT.findall(code)), len(re.findall(r"\bgoto\b", code))))
    rows.sort(key=lambda r: -r[3])
    return rows


def main(argv: list[str]) -> int:
    rows = scan()
    if "--total" not in argv:
        print(f"{'modulo':16}{'lineas':>7}{'funcs':>7}{'ids_reg':>9}{'goto':>6}")
        for m, ln, fn, ids, g in rows:
            print(f"{m:16}{ln:7}{fn:7}{ids:9}{g:6}")
    print(f"TOTAL ids_reg={sum(r[3] for r in rows)} goto={sum(r[4] for r in rows)} "
          f"lineas={sum(r[1] for r in rows)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
