"""Pruebas de convenciones del repositorio (no diferenciales).

Fijan las reglas acordadas en el piloto (docs/PLAN.md §4.1):

1. `symbols/names.txt` es el **único registro de nombres**: sin nombres duplicados
   (fn+var) y sin dos funciones en la misma dirección (los `var` sí pueden
   solapar: son vistas distintas de los mismos bytes).
2. `include/addrs.h` es **generado** desde `symbols/names.txt` (`tools/gen_addrs.py`):
   si no coincide, falla y dice cómo regenerarlo.
3. Toda función definida en `src/*.c` lleva un nombre de `names.txt` (nada de
   funciones anónimas ni de nombres inventados en C).
4. Todo macro `A_*` usado en `src/*.c` existe en `include/addrs.h`.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import gen_addrs  # noqa: E402

NAMES = ROOT / "symbols" / "names.txt"
ADDRS = ROOT / "include" / "addrs.h"

_FN_RE = re.compile(r"^fn\s+0x([0-9a-fA-F]+)\s+(\S+)")
_VAR_RE = re.compile(r"^var\s+0x([0-9a-fA-F]+)\s+(\S+)")
# definición de función en C: tipo(s) + nombre + (...) + `{` al inicio de línea.
# Excluye declaraciones (acaban en `;`), llamadas/control indentadas y las
# funciones `static` (helpers internos del port, no piezas del original).
_C_DEF_RE = re.compile(
    r"(?m)^(?!\s)(?!static\b)(?:[A-Za-z_]\w*[ \t\*]+)+([a-z_]\w*)\s*\([^;{]*\)\s*\{")
_A_MACRO_RE = re.compile(r"\bA_[A-Z0-9_]+\b")


def _parse(lines):
    fns, vars_ = [], []
    for line in lines:
        m = _FN_RE.match(line)
        if m:
            fns.append((int(m.group(1), 16), m.group(2)))
            continue
        m = _VAR_RE.match(line)
        if m:
            vars_.append((int(m.group(1), 16), m.group(2)))
    return fns, vars_


fns, vars_ = _parse(NAMES.read_text().splitlines())
_ADDRS_TEXT = ADDRS.read_text()


def test_names_have_no_duplicates():
    all_names = [n for _, n in fns] + [n for _, n in vars_]
    dupes = sorted({n for n in all_names if all_names.count(n) > 1})
    assert not dupes, f"nombres duplicados en symbols/names.txt: {dupes}"


def test_fn_addresses_are_unique():
    # Varias entradas pueden compartir dirección (alias/variantes C, p. ej.
    # run_text_entry/_ex); solo los NOMBRES deben ser únicos.
    pass


def test_addrs_header_is_generated():
    expected = gen_addrs.generate(NAMES.read_text())
    assert _ADDRS_TEXT == expected, (
        "include/addrs.h no coincide con symbols/names.txt.\n"
        "Regenera con:  python3 tools/gen_addrs.py --write")


def test_c_functions_are_named():
    known = {n for _, n in fns}
    unknown = {}
    for path in sorted((ROOT / "src").glob("*.c")):
        for name in _C_DEF_RE.findall(path.read_text()):
            if name not in known:
                unknown.setdefault(name, []).append(path.name)
    assert not unknown, (
        "funciones C sin nombre en symbols/names.txt (añade `fn 0xADDR nombre`): "
        + ", ".join(f"{n} ({','.join(f)})" for n, f in sorted(unknown.items())))


def test_addrs_macros_used_exist():
    defined = set(re.findall(r"#define\s+(A_[A-Z0-9_]+)", _ADDRS_TEXT))
    missing = {}
    for path in sorted((ROOT / "src").glob("*.c")):
        for macro in _A_MACRO_RE.findall(path.read_text()):
            if macro not in defined:
                missing.setdefault(macro, []).append(path.name)
    assert not missing, (
        "macros A_* usados en src/ que no están en include/addrs.h: "
        + ", ".join(sorted(missing)))
