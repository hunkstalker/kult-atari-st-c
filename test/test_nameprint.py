"""Tests diferenciales de la impresion del nombre del objeto seleccionado:
0x1687e (sin sonido), 0x168a0 (sonido 0xe6 + espera) y 0x168d4 (sin sonido, bit 6
del color limpio). Las tres posicionan el nombre con 0x10fb4 y lo imprimen con
0x10b6c; la espera 0x1228a sale por Cnecin en modo 0x87 con una tecla staged.
"""
import ctypes

import harness
from harness import console_key, differential, report

_u8p = ctypes.POINTER(ctypes.c_uint8)

FUN_1687E = 0x1687E
FUN_168A0 = 0x168A0
FUN_168D4 = 0x168D4
SCRIPT = 0x50000

harness._lib.obj_name.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.obj_name.restype = None
harness._lib.obj_name_snd.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.obj_name_snd.restype = None
harness._lib.obj_name_clean.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.obj_name_clean.restype = None


def _pokes(byte):
    pokes = {
        0x1ACC6: bytes([0x87]),          # modo sin overlay -> Cnecin directo
        0x1AB4F: bytes([0x00]),          # Supexec activo
        0x1ACD4: bytes([0x00]),          # contador de VBL
        0x1A45E: (0x80000).to_bytes(4, "big"),
        SCRIPT: bytes([byte, byte]),
    }
    for i in range(140):                 # banco de textos [len=3][c1][c2]
        pokes[0x3C534 + i * 3] = bytes([3, 0x21 + (i % 0x40), 0x22])
    pokes.update(console_key("a"))
    return pokes


def test_fun_1687e_nombre():
    for byte in (5, 0x12, 0x81):
        diffs, _ = differential(
            FUN_1687E, {"a6": SCRIPT, "_pokes": _pokes(byte)},
            lambda lib, buf: lib.obj_name(buf, SCRIPT))
        assert not diffs, f"byte={byte:#x}\n{report(diffs)}"


def test_fun_168a0_nombre_con_sonido():
    for byte in (5, 0x12):
        diffs, _ = differential(
            FUN_168A0, {"a6": SCRIPT, "_pokes": _pokes(byte)},
            lambda lib, buf: lib.obj_name_snd(buf, SCRIPT))
        assert not diffs, f"byte={byte:#x}\n{report(diffs)}"


def test_fun_168d4_nombre_filtrado():
    for byte in (5, 0x12):
        diffs, _ = differential(
            FUN_168D4, {"a6": SCRIPT, "_pokes": _pokes(byte)},
            lambda lib, buf: lib.obj_name_clean(buf, SCRIPT))
        assert not diffs, f"byte={byte:#x}\n{report(diffs)}"
