"""Test del trazo de linea Bresenham (line.c): 0x13d0e, 0x13e14, 0x13eae."""
import ctypes

import harness
from harness import differential, report

_u8p = ctypes.POINTER(ctypes.c_uint8)
SCREEN = 0x90000
BACK = 0x88000
VBL_COUNTER = 0x1acd4
VBL_SITE = 0x1359a

harness._lib.g_fun_13d0e.argtypes = [_u8p]
harness._lib.g_fun_13d0e.restype = None


def _screen_pokes():
    pokes = {0x1a45e: SCREEN.to_bytes(4, "big")}
    for i in range(0x10000):
        pokes[SCREEN + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[BACK + i] = bytes([(i * 11 + 7) & 0xFF])
    return pokes


def test_fun_13d0e():
    for x0, x1, y0, y1 in ((0x40, 0x50, 0x14, 0x20), (0x50, 0x40, 0x20, 0x14),
                           (0x40, 0x40, 0x14, 0x30), (0x40, 0x70, 0x14, 0x14)):
        pokes = _screen_pokes()
        pokes[0x1a516] = x0.to_bytes(2, "big")
        pokes[0x1a518] = x1.to_bytes(2, "big")
        pokes[0x1a51a] = y0.to_bytes(2, "big")
        pokes[0x1a51c] = y1.to_bytes(2, "big")
        diffs, _ = differential(0x13d0e, {"_pokes": pokes},
                                lambda lib, buf: lib.g_fun_13d0e(buf))
        assert not diffs, f"({x0:#x},{y0:#x})->({x1:#x},{y1:#x})\n{report(diffs)}"


FUN_13E14 = 0x13E14
FUN_13EAE = 0x13EAE
harness._lib.g_fun_13e14.argtypes = [_u8p] + [ctypes.c_uint32] * 4
harness._lib.g_fun_13e14.restype = None
harness._lib.g_fun_13eae.argtypes = [_u8p] + [ctypes.c_uint32] * 4
harness._lib.g_fun_13eae.restype = None


def _vbl_k(n):
    return [{"pc": VBL_SITE, "nth": k, "addr": VBL_COUNTER, "width": 1, "value": k}
            for k in range(1, n + 1)]


def test_fun_13e14():
    # d2=1 -> 4 trazos (loop1) + *(0x1a52c) en loop2; 0x1a52c=0 es degenerado
    # (dbf con 0xffff -> 65536), asi que se usa >=1.
    for n2 in (1, 2):
        pokes = _screen_pokes()
        pokes[0x1a52c] = n2.to_bytes(2, "big")
        pokes[VBL_COUNTER] = bytes([0x00])
        diffs, _ = differential(
            FUN_13E14, {"d0": 4, "d1": 4, "d2": 1, "d3": 2, "_pokes": pokes},
            lambda lib, buf: lib.g_fun_13e14(buf, 4, 4, 1, 2),
            schedule=_vbl_k(4 + n2), wait_sites=[VBL_SITE])
        assert not diffs, f"n2={n2}\n{report(diffs)}"


def test_fun_13eae():
    # d2 (=d3) = 1 -> 1 trazo; (*(0x1a52e)<<2)=4 -> 4 trazos; *(0x1a52c)=1 -> 2
    pokes = _screen_pokes()
    pokes[0x1a52c] = (1).to_bytes(2, "big")
    pokes[0x1a52e] = (1).to_bytes(2, "big")
    pokes[VBL_COUNTER] = bytes([0x00])
    diffs, _ = differential(
        FUN_13EAE, {"d0": 4, "d1": 4, "d2": 1, "d3": 1, "_pokes": pokes},
        lambda lib, buf: lib.g_fun_13eae(buf, 4, 4, 1, 1),
        schedule=_vbl_k(1 + 4 + 2), wait_sites=[VBL_SITE])
    assert not diffs, report(diffs)


FUN_13490 = 0x13490
FUN_134B2 = 0x134B2
harness._lib.g_fun_13490.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_fun_13490.restype = ctypes.c_uint32
harness._lib.g_fun_134b2.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_fun_134b2.restype = ctypes.c_uint32


def _icon_pokes(d2, d3):
    pokes = {0x1a851: bytes([0x11]), 0x1ad0a: bytes([0]),
             0x1a45e: SCREEN.to_bytes(4, "big"),
             0x1a462: BACK.to_bytes(4, "big"), VBL_COUNTER: bytes([0x00])}
    for i in range(10):
        base = 0x1a850 + i * 0x10
        pokes[base] = ((i + 1) & 0xffff).to_bytes(2, "big")
        pokes[base + 2] = (SCREEN + i * 0x10).to_bytes(4, "big")
        pokes[base + 6] = (0).to_bytes(2, "big")
        pokes[base + 8] = (2).to_bytes(2, "big")     # d0
        pokes[base + 10] = (3).to_bytes(2, "big")    # d1
        pokes[base + 12] = d2.to_bytes(2, "big")
        pokes[base + 14] = d3.to_bytes(2, "big")
    for i in range(0x10000):
        pokes[SCREEN + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[BACK + i] = bytes([(i * 11 + 7) & 0xFF])
    return pokes


def _run_icon(entry, fn, waits):
    for skip in (0, 1):
        pokes = _icon_pokes(1, 1)
        pokes[0x30000] = bytes([0, 1])
        pokes[0x1a570] = bytes([skip])
        sched = _vbl_k(waits) if (skip == 0 and waits) else []
        diffs, info = differential(
            entry, {"a6": 0x30000, "_pokes": pokes},
            lambda lib, buf, f=fn: f(buf, 0x30000),
            schedule=sched, wait_sites=[VBL_SITE] if sched else None)
        assert not diffs, f"{entry:#x} skip={skip}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], f"{entry:#x} skip={skip} a6"


def test_fun_13490():
    # 13e14: loop1 = d2<<2 = 4, loop2 = *(0x1a52c) = 1 -> 5 esperas
    _run_icon(FUN_13490, harness._lib.g_fun_13490, 5)


def test_fun_134b2():
    # 13eae: 1 (loop1) + *(0x1a52e)<<2 = 4 + *(0x1a52c) = 1 +1 -> 7 esperas
    _run_icon(FUN_134B2, harness._lib.g_fun_134b2, 7)
