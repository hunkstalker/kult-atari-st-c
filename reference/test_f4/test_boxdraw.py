"""Tests diferenciales de las primitivas de dibujo de la caja de texto (boxdraw.c).

FUN_00012ffa @ 0x12ffa: línea horizontal de D3+1 píxeles en (D0, D1) con color D2
(un bit por plano) sobre screen_ptr_a; grupos de 8 bytes por 16 píxeles.
FUN_00013216 @ 0x13216: línea vertical, con a0 avanzando 0xa0 por píxel.
"""
import ctypes

import harness
from harness import differential, report

FUN_12FFA = 0x12FFA
FUN_13216 = 0x13216
FUN_130A4 = 0x130A4
FUN_13162 = 0x13162
SCREEN = 0x80000
BACK = 0x78000

_u8p = ctypes.POINTER(ctypes.c_uint8)
for _name in ("g_fun_12ffa", "g_fun_13216"):
    getattr(harness._lib, _name).argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                             ctypes.c_uint32, ctypes.c_uint32]
    getattr(harness._lib, _name).restype = None
for _name in ("g_fun_130a4", "g_fun_13162"):
    getattr(harness._lib, _name).argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                             ctypes.c_uint32]
    getattr(harness._lib, _name).restype = None


def _pokes():
    buf = bytes((0x5A + i * 7) & 0xff for i in range(0x4000))
    return {0x1a45e: SCREEN.to_bytes(4, "big"),
            0x1a46a: (0x8000).to_bytes(4, "big"),
            SCREEN: buf}


def test_fun_12ffa():
    for d0 in (0, 2, 16, 0x42, 0x3FE):
        for d1 in (0, 1, 5):
            for d2 in (0, 1, 5, 15):
                for d3 in (1, 3, 20, 40):
                    diffs, _ = differential(
                        FUN_12FFA, {"d0": d0, "d1": d1, "d2": d2, "d3": d3,
                                    "_pokes": _pokes()},
                        lambda lib, buf, a=d0, b=d1, c=d2, e=d3:
                        lib.g_fun_12ffa(buf, a, b, c, e))
                    assert not diffs, (f"d0={d0} d1={d1} d2={d2} d3={d3}\n{report(diffs)}")


def test_fun_13216():
    for d0 in (0, 2, 16, 0x42, 0x3FE):
        for d1 in (0, 1, 5):
            for d2 in (0, 1, 5, 15):
                for d3 in (1, 3, 20, 40):
                    diffs, _ = differential(
                        FUN_13216, {"d0": d0, "d1": d1, "d2": d2, "d3": d3,
                                    "_pokes": _pokes()},
                        lambda lib, buf, a=d0, b=d1, c=d2, e=d3:
                        lib.g_fun_13216(buf, a, b, c, e))
                    assert not diffs, (f"d0={d0} d1={d1} d2={d2} d3={d3}\n{report(diffs)}")


def _copy_pokes():
    front = bytes((0x11 * i + 0x05) & 0xff for i in range(0x4000))
    back = bytes((0x07 * i + 0x80) & 0xff for i in range(0x4000))
    return {0x1a45e: SCREEN.to_bytes(4, "big"),
            0x1a462: BACK.to_bytes(4, "big"),
            SCREEN: front, BACK: back}


def test_fun_130a4():
    for d0 in (0, 2, 16, 0x42, 0x3FE):
        for d1 in (0, 1, 5):
            for d3 in (1, 3, 20, 40):
                diffs, _ = differential(
                    FUN_130A4, {"d0": d0, "d1": d1, "d3": d3, "_pokes": _copy_pokes()},
                    lambda lib, buf, a=d0, b=d1, e=d3: lib.g_fun_130a4(buf, a, b, e))
                assert not diffs, (f"d0={d0} d1={d1} d3={d3}\n{report(diffs)}")


def test_fun_13162():
    for d0 in (0, 2, 16, 0x42, 0x3FE):
        for d1 in (0, 1, 5):
            for d3 in (1, 3, 20, 40):
                diffs, _ = differential(
                    FUN_13162, {"d0": d0, "d1": d1, "d3": d3, "_pokes": _copy_pokes()},
                    lambda lib, buf, a=d0, b=d1, e=d3: lib.g_fun_13162(buf, a, b, e))
                assert not diffs, (f"d0={d0} d1={d1} d3={d3}\n{report(diffs)}")


FUN_12FEE = 0x12FEE
FUN_1320A = 0x1320A
for _name in ("g_fun_12fee", "g_fun_1320a"):
    getattr(harness._lib, _name).argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                             ctypes.c_uint32, ctypes.c_uint32]
    getattr(harness._lib, _name).restype = None


def test_fun_12fee_1320a():
    for entry, fn in ((FUN_12FEE, harness._lib.g_fun_12fee),
                      (FUN_1320A, harness._lib.g_fun_1320a)):
        for d0 in (0, 2, 16, 0x42):
            for d1 in (0, 5):
                for d2 in (0, 5, 15):
                    for d3 in (1, 20, 40):
                        pokes = _pokes()
                        diffs, _ = differential(
                            entry, {"d0": d0, "d1": d1, "d2": d2, "d3": d3, "_pokes": pokes},
                            lambda lib, buf, f=fn, a=d0, b=d1, c=d2, e=d3:
                            f(buf, a, b, c, e))
                        assert not diffs, (f"entry={entry:#x} d0={d0} d1={d1} "
                                           f"d2={d2} d3={d3}\n{report(diffs)}")
