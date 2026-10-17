#!/usr/bin/env python3
"""gen_addrs.py — genera include/addrs.h desde work/names.txt.

Única fuente de los globales: las líneas `var 0x<addr> <nombre>` de
`work/names.txt`. Cada una produce `#define A_<NOMBRE> 0x<addr>` (nombre en
mayúsculas). Las líneas de sección de names.txt (`# ---- ... ----`) que
preceden a vars se copian como comentarios; las secciones de funciones (fn) se
ignoran, así que en addrs.h solo aparecen los globales.

Uso:
    python3 tools/gen_addrs.py            # escribe en stdout
    python3 tools/gen_addrs.py --write    # reescribe include/addrs.h
    python3 tools/gen_addrs.py --check    # falla si include/addrs.h no coincide

`make test` incluye test/test_conventions.py, que llama a `generate()` y
compara con el fichero versionado: si editas names.txt, regenera y commitea.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NAMES = ROOT / "work" / "names.txt"
ADDRS = ROOT / "include" / "addrs.h"

_VAR_RE = re.compile(r"^var\s+0x([0-9a-fA-F]+)\s+(\S+)(?:\s+#\s*(.*))?$")
_HEADER_RE = re.compile(r"^#\s?(.*\S)\s*$")

_BANNER = """/* addrs.h — direcciones con nombre de los globales de KULT.PRG (base 0x10000).
 *
 * GENERADO por tools/gen_addrs.py desde work/names.txt. NO EDITAR A MANO.
 * Para añadir/mover un global: edita work/names.txt y ejecuta
 *     python3 tools/gen_addrs.py --write
 * (test/test_conventions.py falla si este fichero no coincide con names.txt).
 */
#ifndef KULT_ADDRS_H
#define KULT_ADDRS_H
"""


def _entries(names_text: str):
    """(kind, payload) en orden: ('header', texto) o ('def', nombre, addr, comentario).

    Un `#` fija el encabezado pendiente; una línea `fn` lo descarta; la primera
    `var` que lo sigue lo emite (y lo consume). Así solo se generan las
    secciones que contienen globales.
    """
    pending: str | None = None
    for line in names_text.splitlines():
        if line.startswith("fn "):
            pending = None
            continue
        m = _HEADER_RE.match(line)
        if m:
            pending = m.group(1)
            continue
        m = _VAR_RE.match(line)
        if m:
            if pending is not None:
                yield ("header", pending)
                pending = None
            yield ("def", m.group(2), int(m.group(1), 16), (m.group(3) or "").strip())


def generate(names_text: str) -> str:
    entries = list(_entries(names_text))
    defs = [e for e in entries if e[0] == "def"]
    width = max((len("A_" + e[1].upper()) for e in defs), default=0)
    out = [_BANNER]
    for e in entries:
        if e[0] == "header":
            out.append(f"/* {e[1]} */")
        else:
            _, name, addr, comment = e
            macro = "A_" + name.upper()
            line = f"#define {macro:<{width}} 0x{addr:x}"
            if comment:
                line += f"  /* {comment} */"
            out.append(line)
    out.append("")
    out.append("#endif")
    return "\n".join(out) + "\n"


def main(argv: list[str]) -> int:
    text = generate(NAMES.read_text())
    if "--check" in argv:
        if ADDRS.read_text() != text:
            print("include/addrs.h está desactualizado; ejecuta:", file=sys.stderr)
            print("    python3 tools/gen_addrs.py --write", file=sys.stderr)
            return 1
        return 0
    if "--write" in argv:
        ADDRS.write_text(text)
        print(f"escrito {ADDRS.relative_to(ROOT)}")
        return 0
    sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
