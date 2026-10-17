"""Tests diferenciales del formulario de texto (0x142cc/0x143e2/0x141f4..0x1424c).

0x142cc prepara la lista de blits en $d8f0 desde la tabla 0x1a7b6 y las entradas de
texto 0x41fb0; 0x143e2 la dibuja (blit de texto + espera de VBL 0x13594 por linea y
blit enmascarado de la caja). Las entradas 0x141f4/0x14246 cargan el indice de A6[1]
y arrancan el sample (0x18f8e); las 0x141fa/0x1424c repiten el bloque desplazando el
destino $d904.
"""
import ctypes

import harness
from harness import differential, report

_u8p = ctypes.POINTER(ctypes.c_uint8)

FUN_142CC = 0x142CC
FUN_143E2 = 0x143E2
FUN_141F4 = 0x141F4
FUN_141FA = 0x141FA
FUN_14246 = 0x14246
FUN_1424C = 0x1424C

VBL_SITE = 0x1359A
VBL_COUNTER = 0x1ACD4
MODE = 0xD8F0
LIST = 0xD8F2
FORM0 = 0xD900
FORM1 = 0xD902
FORM2 = 0xD904
BOX = 0x1A56C
TABLE = 0x1A7B6
ENTRIES = 0x41FB0
RECORD = 0x50000
SCREEN = 0x80000
SRC = 0x60000

harness._lib.g_fun_142cc.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_fun_142cc.restype = None
harness._lib.g_fun_143e2.argtypes = [_u8p]
harness._lib.g_fun_143e2.restype = None
harness._lib.g_fun_141f4.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_fun_141f4.restype = None
harness._lib.g_fun_141fa.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_141fa.restype = None
harness._lib.g_fun_14246.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_fun_14246.restype = None
harness._lib.g_fun_1424c.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_1424c.restype = None


def _form_pokes(lines=((0, 2, 1), (1, 5, 1), (2, 8, 1)), widths=(4, 4, 4),
                heights=(2, 2, 2), index=1):
    """Tabla 0x1a7b6 -> RECORD, entradas de texto y la pantalla apuntada."""
    pokes = {0x1A45E: SCREEN.to_bytes(4, "big")}
    pokes[TABLE + (index - 1) * 4] = RECORD.to_bytes(4, "big")
    for i in range(4):
        pokes[ENTRIES + i * 6] = (6).to_bytes(2, "big")
        pokes[ENTRIES + i * 6 + 2] = bytes([heights[i % 3]])
        pokes[ENTRIES + i * 6 + 3] = bytes([widths[i % 3]])
    for i, (idx, x, y) in enumerate(lines):
        pokes[RECORD + i * 3] = bytes([idx])
        pokes[RECORD + i * 3 + 1] = bytes([x])
        pokes[RECORD + i * 3 + 2] = bytes([y])
    for i in range(0x300):
        pokes[SCREEN - 0x8000 + i] = bytes([(i * 5 + 1) & 0xFF])
        pokes[SRC + i] = bytes([(i * 7 + 3) & 0xFF])
    return pokes


def test_fun_142cc_lista_forward():
    for d0 in (1, 4, 16):
        diffs, _ = differential(
            FUN_142CC, {"d0": d0, "_pokes": _form_pokes(index=d0)},
            lambda lib, buf, v=d0: lib.g_fun_142cc(buf, v))
        assert not diffs, f"d0={d0}\n{report(diffs)}"


def test_fun_142cc_bit7_reversa():
    """x con bit7: modo 0xffff, D0 ajustado (+ancho-1) y caja tomada de D6."""
    pokes = _form_pokes(lines=((0, 0x82, 3), (1, 0x05, 3), (2, 0x88, 3)))
    diffs, _ = differential(
        FUN_142CC, {"d0": 1, "_pokes": pokes},
        lambda lib, buf: lib.g_fun_142cc(buf, 1))
    assert not diffs, report(diffs)


def _list_pokes(mode=0, w=4, h=2):
    """Lista ya preparada en $d8f2: 3 lineas + caja final."""
    pokes = {VBL_COUNTER: bytes([0])}
    pokes[MODE] = mode.to_bytes(2, "big")
    a = LIST
    for i in range(3):
        pokes[a] = w.to_bytes(2, "big")
        pokes[a + 2] = h.to_bytes(2, "big")
        pokes[a + 4] = (SRC + i * 0x100).to_bytes(4, "big")
        pokes[a + 8] = (SCREEN + i * 0x80).to_bytes(4, "big")
        pokes[a + 12] = (0).to_bytes(2, "big")
        a += 14
    pokes[a] = w.to_bytes(2, "big")
    pokes[a + 2] = h.to_bytes(2, "big")
    pokes[a + 4] = SCREEN.to_bytes(4, "big")
    pokes[a + 8] = (0).to_bytes(2, "big")
    for i in range(0x400):
        pokes[SRC + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[SCREEN + i] = bytes([(i * 9 + 7) & 0xFF])
    return pokes


def _vbl_schedule(n):
    return [{"pc": VBL_SITE, "nth": k, "addr": VBL_COUNTER, "width": 1, "value": k}
            for k in range(1, n + 1)]


def test_fun_143e2_dibuja():
    for mode in (0, 0xFFFF):
        diffs, _ = differential(
            FUN_143E2, {"_pokes": _list_pokes(mode=mode)},
            lambda lib, buf: lib.g_fun_143e2(buf),
            schedule=_vbl_schedule(3), wait_sites=[VBL_SITE])
        assert not diffs, f"mode={mode:#x}\n{report(diffs)}"


def _script_pokes():
    """Estado del script: indice en el buffer de entrada y contadores de linea."""
    pokes = _form_pokes()
    pokes[0x1AB4F] = bytes([0x00])
    pokes[FORM0] = (1).to_bytes(2, "big")
    pokes[FORM1] = (2).to_bytes(2, "big")
    pokes[FORM2] = (0x40000).to_bytes(4, "big")
    pokes[RECORD + 1] = bytes([1])       # indice para 0x141f4/0x14246
    return pokes


def test_fun_141fa_cuerpo():
    pokes = _script_pokes()
    diffs, _ = differential(
        FUN_141FA, {"a6": RECORD + 2, "d0": 1, "_pokes": pokes},
        lambda lib, buf: lib.g_fun_141fa(buf, RECORD + 2, 1),
        schedule=_vbl_schedule(3), wait_sites=[VBL_SITE])
    assert not diffs, report(diffs)


def test_fun_141f4_entrada():
    pokes = _script_pokes()
    diffs, _ = differential(
        FUN_141F4, {"a6": RECORD, "_pokes": pokes},
        lambda lib, buf: lib.g_fun_141f4(buf, RECORD),
        schedule=_vbl_schedule(3), wait_sites=[VBL_SITE])
    assert not diffs, report(diffs)


def test_fun_1424c_cuerpo():
    pokes = _script_pokes()
    diffs, _ = differential(
        FUN_1424C, {"a6": RECORD + 2, "d0": 1, "_pokes": pokes},
        lambda lib, buf: lib.g_fun_1424c(buf, RECORD + 2, 1),
        schedule=_vbl_schedule(6), wait_sites=[VBL_SITE])
    assert not diffs, report(diffs)


def test_fun_14246_entrada():
    pokes = _script_pokes()
    diffs, _ = differential(
        FUN_14246, {"a6": RECORD, "_pokes": pokes},
        lambda lib, buf: lib.g_fun_14246(buf, RECORD),
        schedule=_vbl_schedule(6), wait_sites=[VBL_SITE])
    assert not diffs, report(diffs)
