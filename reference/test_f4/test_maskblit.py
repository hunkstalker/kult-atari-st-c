"""Differential tests for the masked screen-row blitter family.

FUN_00012698 @ 0x12698: precompute the edge masks (0x1a538/0x1a53a/0x1a53c) for a blit
of D2 pixels at bit offset D7.
FUN_000123c0 @ 0x123c0: blit one masked row from A1 to A0.
FUN_0001215e @ 0x1215e: run the row blitter over D3 rows, source A0-0x8000, stride 0xa0.
"""
import ctypes

import harness
from harness import differential, report

MASK = 0x12698
ROW = 0x123c0
ROWS = 0x1215e

MASK_FIRST = 0x1a538
MASK_LAST = 0x1a53a
MASK_GROUPS = 0x1a53c
LOW_D8EA = 0x0d8ea
DAT_1D402 = 0x1d402

DEST = 0x90000
SRC = DEST - 0x8000

_u8p = ctypes.POINTER(ctypes.c_uint8)
harness._lib.g_fun_12698.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_12698.restype = None
harness._lib.g_fun_123c0.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_123c0.restype = None
harness._lib.g_fun_1215e.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_1215e.restype = None


def test_fun_12698():
    """The mask precompute writes three words; the byte diff sees the result."""
    for d2 in (0, 1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 31, 32, 39, 40, 63, 64, 100, 320):
        for d7 in (0, 1, 2, 3, 4, 7, 8, 11, 12, 15):
            diffs, _ = differential(
                MASK, {"d2": d2, "d7": d7},
                lambda lib, buf, w=d2, b=d7: lib.g_fun_12698(buf, w, b))
            assert not diffs, f"d2={d2} d7={d7}\n{report(diffs)}"


def _row_pokes(left, right, groups):
    pokes = {
        MASK_FIRST: left.to_bytes(2, "big"),
        MASK_LAST: right.to_bytes(2, "big"),
        MASK_GROUPS: groups.to_bytes(2, "big"),
    }
    for i in range(0x200):
        pokes[SRC + i] = bytes([(i * 7 + 3) & 0xff])
        pokes[DEST + i] = bytes([(i * 13 + 5) & 0xff])
    return pokes


def test_fun_123c0():
    """One row: first/last 4-word groups merged, middle longwords copied."""
    for groups in (1, 2, 3, 4, 5, 8):
        for left, right in ((0x0000, 0x0000), (0xffff, 0xffff), (0x8000, 0x0001),
                            (0xf000, 0x000f), (0x00f0, 0x0f00), (0xaaaa, 0x5555)):
            diffs, _ = differential(
                ROW, {"a0": DEST, "a1": SRC, "_pokes": _row_pokes(left, right, groups)},
                lambda lib, buf: lib.g_fun_123c0(buf, DEST, SRC))
            assert not diffs, f"groups={groups} L={left:#x} R={right:#x}\n{report(diffs)}"


def _rows_pokes(mode_byte):
    pokes = {LOW_D8EA: bytes([mode_byte]), DAT_1D402: bytes([0x5a])}
    for i in range(0x800):
        pokes[SRC + i] = bytes([(i * 3 + 1) & 0xff])
        pokes[DEST + i] = bytes([(i * 11 + 7) & 0xff])
    return pokes


def test_fun_1215e():
    """D3 rows, back-buffer source, 0xa0 stride, plus the 0xd8ea/0x1d402 side note."""
    for d3 in (1, 2, 3, 5):
        for d2 in (1, 8, 16, 17, 32, 40):
            for d7 in (0, 1, 4, 8, 12, 15):
                for mode_byte in (0x41, 0x00):
                    diffs, _ = differential(
                        ROWS, {"a0": DEST, "d2": d2, "d3": d3, "d7": d7,
                               "_pokes": _rows_pokes(mode_byte)},
                        lambda lib, buf, w=d2, h=d3, b=d7:
                        lib.g_fun_1215e(buf, DEST, w, h, b))
                    assert not diffs, (
                        f"d2={d2} d3={d3} d7={d7} mode={mode_byte:#x}\n{report(diffs)}")


FUN_12466 = 0x12466
harness._lib.g_fun_12466.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_12466.restype = None
VBL_COUNTER = 0x1acd4
VBL_SITE = 0x1359a


def _row4_pokes(left, right, groups):
    pokes = {0x1a538: left.to_bytes(2, "big"),
             0x1a53a: right.to_bytes(2, "big"),
             0x1a53c: groups.to_bytes(2, "big"),
             VBL_COUNTER: bytes([0x00])}
    for i in range(0x400):
        pokes[SRC + i] = bytes([(i * 7 + 3) & 0xFF])
        pokes[DEST + i] = bytes([(i * 13 + 5) & 0xFF])
    return pokes


def test_fun_12466():
    for groups in (2, 3, 4, 5):
        for left, right in ((0x0000, 0x0000), (0xffff, 0xffff), (0xf000, 0x000f)):
            for d3 in (1, 2, 4):
                diffs, _ = differential(
                    FUN_12466, {"a0": DEST, "a1": SRC, "d3": d3,
                                "_pokes": _row4_pokes(left, right, groups)},
                    lambda lib, buf, h=d3: lib.g_fun_12466(buf, DEST, SRC, h),
                    schedule=[{"pc": VBL_SITE, "nth": 1, "addr": VBL_COUNTER,
                               "width": 1, "value": 0x01}],
                    wait_sites=[VBL_SITE])
                assert not diffs, (f"groups={groups} L={left:#x} R={right:#x} d3={d3}\n"
                                   f"{report(diffs)}")


FUN_120A8 = 0x120A8
FUN_120DC = 0x120DC
FUN_120E4 = 0x120E4
FUN_1213A = 0x1213A
FUN_1222E = 0x1222E
_u32p = ctypes.POINTER(ctypes.c_uint32)
harness._lib.g_fun_120a8.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32]
harness._lib.g_fun_120a8.restype = None
for _n in ("g_fun_120dc", "g_fun_120e4", "g_fun_1213a"):
    getattr(harness._lib, _n).argtypes = [_u8p, ctypes.c_uint32, _u32p]
    getattr(harness._lib, _n).restype = None
harness._lib.g_fun_1222e.argtypes = [_u8p, ctypes.c_uint32, _u32p]
harness._lib.g_fun_1222e.restype = None


def _hint_table_pokes():
    pokes = {0x1a851: bytes([0x11]), 0x1ad0a: bytes([0x00]),
             0x1a45e: (0x90000).to_bytes(4, "big")}
    for i in range(10):
        base = 0x1a850 + i * 0x10
        word = (i + 1) & 0xffff
        pokes[base] = word.to_bytes(2, "big")
        pokes[base + 2] = (0x90000 + i * 0x10).to_bytes(4, "big")   # long destino
        for k, v in enumerate((1, 2, 3, 4, 5)):
            pokes[base + 6 + k * 2] = v.to_bytes(2, "big")
    return pokes


def test_fun_120a8():
    for first_free in (0, 3, 9):
        pokes = {}
        for i in range(11):
            base = 0x1a850 + i * 0x10
            w = 0 if i == first_free else (i + 1)
            pokes[base] = w.to_bytes(2, "big")
            for k in range(2, 16):
                pokes[base + k] = bytes([(i * 3 + k) & 0xFF])
        pokes[0x1a851] = bytes([0x77])
        pokes[0x1ad0a] = bytes([0x00])
        diffs, _ = differential(
            FUN_120A8, {"d4": 0x1234, "a0": 0x0abcde, "d7": 0x5678, "d0": 0x1111,
                        "d1": 0x2222, "d2": 0x3333, "d3": 0x4444, "_pokes": pokes},
            lambda lib, buf: lib.g_fun_120a8(buf, 0x1234, 0x0abcde, 0x5678, 0x1111,
                                             0x2222, 0x3333, 0x4444))
        assert not diffs, f"first_free={first_free}\n{report(diffs)}"


def _run_hint(entry, fn, d0, check_d5=False):
    o = (ctypes.c_uint32 * 8)()
    diffs, info = differential(entry, {"d0": d0, "_pokes": _hint_table_pokes()},
                               lambda lib, buf, n=d0, p=o: fn(buf, n, p))
    assert not diffs, f"{entry:#x} d0={d0}\n{report(diffs)}"
    for i, reg in enumerate(("a0", "d4", "d7", "d0", "d1", "d2", "d3")):
        assert o[i] == info["regs"][reg], f"{entry:#x} d0={d0} out[{i}]({reg})"


def test_fun_120dc_120e4_1213a():
    for d0 in (1, 3, 10):
        _run_hint(FUN_120DC, harness._lib.g_fun_120dc, d0)
        _run_hint(FUN_120E4, harness._lib.g_fun_120e4, d0)
    for d0 in (1, 5, 11):                     # 11 no está: d5=1
        o = (ctypes.c_uint32 * 8)()
        diffs, info = differential(FUN_1213A, {"d0": d0, "_pokes": _hint_table_pokes()},
                                   lambda lib, buf, n=d0, p=o: lib.g_fun_1213a(buf, n, p))
        assert not diffs, f"d0={d0}\n{report(diffs)}"
        assert o[7] == info["regs"]["d5"], f"d0={d0} d5"


def test_fun_1222e():
    pokes = _hint_table_pokes()
    pokes[0x30000] = bytes([0, 3])
    for i in range(0x400):
        pokes[0x88000 + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x90000 + i] = bytes([(i * 11 + 7) & 0xFF])
    a6 = ctypes.c_uint32(0)
    diffs, info = differential(FUN_1222E, {"a6": 0x30000, "_pokes": pokes},
                               lambda lib, buf, p=a6: lib.g_fun_1222e(buf, 0x30000,
                                                                      ctypes.byref(p)))
    assert not diffs, report(diffs)
    assert a6.value == info["regs"]["a6"]


FUN_1232E = 0x1232E
harness._lib.g_fun_1232e.argtypes = [_u8p, _u32p]
harness._lib.g_fun_1232e.restype = None


def test_fun_1232e():
    pokes = _hint_table_pokes()
    pokes[0x1a850] = (1).to_bytes(2, "big")          # la entrada 0 tiene el word 1
    pokes[0x1a484] = bytes([0x55])
    for i in range(0x400):
        pokes[0x88000 + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x90000 + i] = bytes([(i * 11 + 7) & 0xFF])
    a6 = ctypes.c_uint32(0x30000)
    diffs, info = differential(FUN_1232E, {"a6": 0x30000, "_pokes": pokes},
                               lambda lib, buf, p=a6: lib.g_fun_1232e(buf, ctypes.byref(p)))
    assert not diffs, report(diffs)
    assert a6.value == info["regs"]["a6"]


FUN_12368 = 0x12368
FUN_12384 = 0x12384
for _n in ("g_fun_12368", "g_fun_12384"):
    getattr(harness._lib, _n).argtypes = [_u8p, _u32p]
    getattr(harness._lib, _n).restype = None


def test_fun_12368_12384():
    for entry, fn, word in ((FUN_12368, harness._lib.g_fun_12368, 2),
                            (FUN_12384, harness._lib.g_fun_12384, 3)):
        pokes = _hint_table_pokes()
        pokes[0x1a850] = word.to_bytes(2, "big")
        for i in range(0x400):
            pokes[0x88000 + i] = bytes([(i * 3 + 1) & 0xFF])
            pokes[0x90000 + i] = bytes([(i * 11 + 7) & 0xFF])
        a6 = ctypes.c_uint32(0x30000)
        diffs, info = differential(entry, {"a6": 0x30000, "_pokes": pokes},
                                   lambda lib, buf, p=a6, f=fn: f(buf, ctypes.byref(p)))
        assert not diffs, f"{entry:#x}\n{report(diffs)}"
        assert a6.value == info["regs"]["a6"]
