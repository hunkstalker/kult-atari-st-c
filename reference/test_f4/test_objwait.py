"""Tests del bucle de espera con hit-test (0x167aa/0x167be/0x1680c).

0x167be guarda (D2,D3) en 0x1a74e/0x1a74f y gira: 0x10522 (frame + cursor),
0x13698 (frame), 0x14886 (hit-test de objetos), 0x109fe si cambia el selector A,
0x1031c (procesa el frame) y sale cuando el paquete IKBD trae el clic (bit2 de
0x1a456). Las esperas se modelan con `schedule`/`wait_sites`: 0x136b0 (paquete
IKBD, una llegada por `wait_frame`) y 0x1359a (contador de VBL).
"""
import ctypes

import harness
from harness import differential, report

_u8p = ctypes.POINTER(ctypes.c_uint8)
FUN_167AA = 0x167AA
FUN_167BE = 0x167BE
FUN_1680C = 0x1680C
IKBD_SITE = 0x136B0
VBL_SITE = 0x1359A
VBL_COUNTER = 0x1ACD4
PACKET = 0x1A44F
OBJ = 0x40000
A6 = 0x40010

harness._lib.g_fun_167aa.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_fun_167aa.restype = ctypes.c_uint32
harness._lib.g_fun_167be.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_167be.restype = None
harness._lib.g_fun_1680c.argtypes = [_u8p]
harness._lib.g_fun_1680c.restype = ctypes.c_uint32


def _pokes():
    """Un unico objeto (rect nulo -> miss), selectores ya pintados y bypass de frame."""
    pokes = {
        PACKET: bytes([0xFF]),
        0x1A456: bytes([0x00]),
        0x1A570: bytes([0x00]),
        0x1AB4F: bytes([0x00]),      # g_fun_1031c no repite la espera de VBL
        VBL_COUNTER: bytes([0x10]),
        0x1AB44: bytes([0x75]), 0x1AB45: bytes([0x75]),
        0x1AB46: bytes([0x64]), 0x1AB47: bytes([0x64]),
        0x1AB48: bytes([0x75]),
        0x1A772: (0xDEADBEEF).to_bytes(4, "big"),
        0x1A7CA: OBJ.to_bytes(4, "big"),
        0x1A7CE: (OBJ + 8).to_bytes(4, "big"),
    }
    pokes[OBJ] = bytes(8)            # rect [0,0,0,0]: el cursor (0,0) nunca cae
    return pokes


def _schedule():
    """Dos llegadas de paquete (bit0 -> clic) y una de VBL."""
    return [
        {"pc": IKBD_SITE, "nth": 1, "addr": PACKET, "width": 1, "value": 0x01},
        {"pc": IKBD_SITE, "nth": 2, "addr": PACKET, "width": 1, "value": 0x01},
        {"pc": VBL_SITE, "nth": 1, "addr": VBL_COUNTER, "width": 1, "value": 0x11},
    ]


def test_fun_167be():
    diffs, _ = differential(
        FUN_167BE, {"d2": 0xFF, "d3": 0x10, "_pokes": _pokes()},
        lambda lib, buf: lib.g_fun_167be(buf, 0xFF, 0x10),
        schedule=_schedule(), wait_sites=[IKBD_SITE, VBL_SITE])
    assert not diffs, report(diffs)


def test_fun_167aa():
    pokes = _pokes()
    pokes[A6] = bytes([0x00, 0x05, 0xFF, 0x10])
    diffs, info = differential(
        FUN_167AA, {"a6": A6, "_pokes": pokes},
        lambda lib, buf: lib.g_fun_167aa(buf, A6),
        schedule=_schedule(), wait_sites=[IKBD_SITE, VBL_SITE])
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


def test_fun_1680c():
    diffs, info = differential(
        FUN_1680C, {"_pokes": _pokes()},
        lambda lib, buf: lib.g_fun_1680c(buf),
        schedule=_schedule(), wait_sites=[IKBD_SITE, VBL_SITE])
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a0"]
