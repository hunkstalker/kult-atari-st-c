"""Differential test for FUN_000172ba @ 0x172ba: clear an indirect byte and a word."""
import ctypes

import harness
from harness import differential, report

CLEAR_FLAG = 0x172ba

_u8p = ctypes.POINTER(ctypes.c_uint8)
harness._lib.g_clear_flag.argtypes = [_u8p]
harness._lib.g_clear_flag.restype = None


def test_clear_flag():
    pokes = {
        0x1a806: (0x30000).to_bytes(4, "big"),
        0x30000: b"\xff",
        0x1acaa: b"\xff\xff",
    }
    diffs, _ = differential(CLEAR_FLAG, {"_pokes": pokes},
                            lambda lib, buf: lib.g_clear_flag(buf))
    assert not diffs, report(diffs)


FUN_15192 = 0x15192
harness._lib.g_fun_15192.argtypes = [_u8p]
harness._lib.g_fun_15192.restype = None


def test_fun_15192():
    pokes = {0x1acd3: bytes([0x55])}
    for i in range(0x29 * 5 + 5):
        pokes[0x1aa72 + i] = bytes([(0x80 + i * 7) & 0xFF])
    diffs, _ = differential(FUN_15192, {"_pokes": pokes},
                            lambda lib, buf: lib.g_fun_15192(buf))
    assert not diffs, report(diffs)


FUN_14A98 = 0x14A98
FUN_14BDC = 0x14BDC
harness._lib.g_fun_14a98.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_14a98.restype = None
harness._lib.g_fun_14bdc.argtypes = [_u8p]
harness._lib.g_fun_14bdc.restype = None


def test_fun_14a98():
    for d0 in (0, 1, 5, 9):
        pokes = {}
        for i in range(0x196dc, 0x196dc + 0x40 * 4):
            pokes[i] = bytes([(i * 3 + 1) & 0xFF])
        for i in range(0x1949c, 0x1949c + 0x40 * 64):
            pokes[i] = bytes([(i * 5 + 2) & 0xFF])
        diffs, _ = differential(FUN_14A98, {"a0": 0x12345678, "d0": d0, "_pokes": pokes},
                                lambda lib, buf, v=d0: lib.g_fun_14a98(buf, 0x12345678, v))
        assert not diffs, f"d0={d0}\n{report(diffs)}"


def test_fun_14bdc():
    pokes = {}
    for i in range(0x3f * 6 + 8):
        pokes[0x1a8f0 + i] = bytes([0x81 if i % 6 == 0 else (0x80 + i) & 0xFF])
    diffs, _ = differential(FUN_14BDC, {"_pokes": pokes},
                            lambda lib, buf: lib.g_fun_14bdc(buf))
    assert not diffs, report(diffs)
