"""Guardián de legibilidad (PLAN §4): comentario con dirección y sin nombres
de decompilador.

- Toda función del core (definición, no `static`) lleva un comentario inmediato
  con su dirección original `0x…` (lo exige la convención).
- El código no usa identificadores estilo decompilador (`uVar11`, `bVar23`,
  `local_18`, `puVar…`), que son ilegibles; deben renombrarse a algo semántico.

No falla por `goto` (algunas rutinas transcriben fielmente el flujo del ASM),
pero `test/test_readability.py` lo cuenta en el mensaje para seguimiento.
"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

_SIG = re.compile(
    r"(?m)^(?!\s)(?!static\b)(?:void|int|uint32_t|int32_t|uint16_t|long)\s+\**\s*([a-z_]\w*)\s*\(")
_DECOMP = re.compile(r"\b([a-z]{1,3}Var[0-9]+|local_[0-9a-f]+|puVar[0-9]+|uStack_[0-9a-f]+)\b")


def _definitions(text):
    """(linea, nombre) de definiciones (con `{` antes de `;`)."""
    res = []
    for m in _SIG.finditer(text):
        i = text.find("{", m.end())
        if i < 0 or ";" in text[m.end():i]:
            continue
        res.append((text.count("\n", 0, m.start()), m.group(1)))
    return res


def _has_addr_comment(lines, ln):
    j = ln - 1
    while j >= 0 and lines[j].strip() == "":
        j -= 1
    block = []
    if j >= 0 and lines[j].strip().endswith("*/"):
        while j >= 0 and not lines[j].strip().startswith("/*"):
            block.insert(0, lines[j])
            j -= 1
        if j >= 0:
            block.insert(0, lines[j])
    return bool(re.search(r"0x[0-9a-fA-F]+", " ".join(block)))


def test_every_function_has_address_comment():
    missing = []
    for f in sorted((ROOT / "src").glob("*.c")):
        lines = f.read_text(encoding="utf-8").splitlines()
        for ln, name in _definitions("\n".join(lines)):
            if not _has_addr_comment(lines, ln):
                missing.append(f"{f.name}:{name}")
    assert not missing, "funciones sin comentario con dirección: " + ", ".join(missing)


def test_no_decompiler_identifiers():
    bad = {}
    for f in sorted((ROOT / "src").glob("*.c")):
        hits = _DECOMP.findall(f.read_text(encoding="utf-8"))
        if hits:
            bad[f.name] = len(hits)
    assert not bad, f"identificadores de decompilador (renómbralos): {bad}"
