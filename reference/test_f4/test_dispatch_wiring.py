"""Verifica el cableado del byte_handler (tabla 0x19f62) contra el binario.

La VM `g_fun_10630` despacha el byte b por la tabla de datos del binario:
`b-1 -> offset desde 0x10000 -> direccion de la funcion`. La reconstruccion C
usa un `switch` (`byte_handler`, en src/dispatch.c). Este test comprueba, byte a
byte (1..0x6a), que cada entrada esta cableada y que apunta a la MISMA funcion
que la tabla del PRG original. Es el guardian del "1:1": si alguien toca el
cableado o la tabla, esto lo detecta sin depender de fixtures por handler.

NO comprueba el avance de A6 de cada handler (eso depende del cuerpo original y
lo cubren los tests por funcion + la traza contra el emulador).
"""
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = 0x10000
TABLE = 0x19F62
N_ENTRIES = 0x6B


def _prg_path():
    txt = open(os.path.join(ROOT, "project.toml"), encoding="utf-8").read()
    m = re.search(r'prg\s*=\s*"([^"]+)"', txt)
    assert m, "project.toml sin 'prg'"
    return os.path.normpath(os.path.join(ROOT, m.group(1)))


def _table_targets():
    d = open(_prg_path(), "rb").read()
    off = TABLE - BASE + 28                 # texto del PRG empieza en +28
    return [BASE + int.from_bytes(d[off + i * 4:off + i * 4 + 4], "big")
            for i in range(N_ENTRIES)]


def _switch_cases():
    src = open(os.path.join(ROOT, "src", "dispatch.c"), encoding="utf-8").read()
    i = src.index("static uint32_t byte_handler")
    j = src.index("uint32_t g_fun_10630")
    body = src[i:j]
    cases = {}
    pending = []
    for ln in body.splitlines():
        m = re.match(r"\s*case (\d+):", ln)
        if m:
            pending.append(int(m.group(1)))
            rest = ln.split(":", 1)[1]
        else:
            rest = ln
        fn = re.search(r"g_fun_([0-9a-f]{3,6})", rest)
        if fn and pending:
            target = int(fn.group(1), 16)
            for b in pending:
                cases[b] = target
            pending = []
    return cases


def test_byte_handler_wiring_matches_rom_table():
    targets = _table_targets()
    cases = _switch_cases()
    missing = [b for b in range(1, N_ENTRIES) if b not in cases]
    assert not missing, f"bytes sin cablear: {[hex(b) for b in missing]}"
    bad = []
    for b in range(1, N_ENTRIES):
        want = targets[b - 1]
        got = cases[b]
        if got != want:
            bad.append((hex(b), hex(want), hex(got)))
    assert not bad, f"cableado distinto de la tabla (byte, tabla, C): {bad}"
