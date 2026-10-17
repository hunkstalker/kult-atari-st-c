"""Pruebas diferenciales de las primitivas de la caja de texto (boxdraw.c).

box_hline 0x12ffa, box_vline 0x13216, box_hline_copy 0x130a4, box_vline_copy
0x13162, box_hline_back 0x12fee, box_vline_back 0x1320a.
"""
import ctypes

import harness
from harness import differential, report

_u8p = ctypes.POINTER(ctypes.c_uint8)
for _n in ("box_hline", "box_vline", "box_hline_back", "box_vline_back"):
    getattr(harness._lib, _n).argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                          ctypes.c_uint32, ctypes.c_uint32]
    getattr(harness._lib, _n).restype = None
for _n in ("box_hline_copy", "box_vline_copy"):
    getattr(harness._lib, _n).argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]
    getattr(harness._lib, _n).restype = None

BOX_HLINE = 0x12ffa
BOX_VLINE = 0x13216
BOX_HLINE_COPY = 0x130a4
BOX_VLINE_COPY = 0x13162
BOX_HLINE_BACK = 0x12fee
BOX_VLINE_BACK = 0x1320a

SCREEN = 0x80000
BACK = 0x78000


def _pokes():
    buf = bytes((0x5A + i * 7) & 0xff for i in range(0x4000))
    return {0x1a45e: SCREEN.to_bytes(4, "big"),
            0x1a46a: (0x8000).to_bytes(4, "big"),
            SCREEN: buf}


def test_box_hline():
    for d0 in (0, 2, 16, 0x42, 0x3FE):
        for d1 in (0, 1, 5):
            for d2 in (0, 1, 5, 15):
                for d3 in (1, 3, 20, 40):
                    diffs, _ = differential(
                        BOX_HLINE, {"d0": d0, "d1": d1, "d2": d2, "d3": d3, "_pokes": _pokes()},
                        lambda lib, buf, a=d0, b=d1, c=d2, e=d3:
                        lib.box_hline(buf, a, b, c, e))
                    assert not diffs, f"d0={d0} d1={d1} d2={d2} d3={d3}\n{report(diffs)}"


def test_box_vline():
    for d0 in (0, 2, 16, 0x42, 0x3FE):
        for d1 in (0, 1, 5):
            for d2 in (0, 1, 5, 15):
                for d3 in (1, 3, 20, 40):
                    diffs, _ = differential(
                        BOX_VLINE, {"d0": d0, "d1": d1, "d2": d2, "d3": d3, "_pokes": _pokes()},
                        lambda lib, buf, a=d0, b=d1, c=d2, e=d3:
                        lib.box_vline(buf, a, b, c, e))
                    assert not diffs, f"d0={d0} d1={d1} d2={d2} d3={d3}\n{report(diffs)}"


def _copy_pokes():
    front = bytes((0x11 * i + 0x05) & 0xff for i in range(0x4000))
    back = bytes((0x07 * i + 0x80) & 0xff for i in range(0x4000))
    return {0x1a45e: SCREEN.to_bytes(4, "big"),
            0x1a462: BACK.to_bytes(4, "big"),
            SCREEN: front, BACK: back}


def test_box_hline_copy():
    for d0 in (0, 2, 16, 0x42, 0x3FE):
        for d1 in (0, 1, 5):
            for d3 in (1, 3, 20, 40):
                diffs, _ = differential(
                    BOX_HLINE_COPY, {"d0": d0, "d1": d1, "d3": d3, "_pokes": _copy_pokes()},
                    lambda lib, buf, a=d0, b=d1, e=d3: lib.box_hline_copy(buf, a, b, e))
                assert not diffs, f"d0={d0} d1={d1} d3={d3}\n{report(diffs)}"


def test_box_vline_copy():
    for d0 in (0, 2, 16, 0x42, 0x3FE):
        for d1 in (0, 1, 5):
            for d3 in (1, 3, 20, 40):
                diffs, _ = differential(
                    BOX_VLINE_COPY, {"d0": d0, "d1": d1, "d3": d3, "_pokes": _copy_pokes()},
                    lambda lib, buf, a=d0, b=d1, e=d3: lib.box_vline_copy(buf, a, b, e))
                assert not diffs, f"d0={d0} d1={d1} d3={d3}\n{report(diffs)}"


def test_box_back_variants():
    for entry, fn in ((BOX_HLINE_BACK, harness._lib.box_hline_back),
                      (BOX_VLINE_BACK, harness._lib.box_vline_back)):
        for d0 in (0, 2, 16, 0x42):
            for d1 in (0, 5):
                for d2 in (0, 5, 15):
                    for d3 in (1, 20, 40):
                        diffs, _ = differential(
                            entry, {"d0": d0, "d1": d1, "d2": d2, "d3": d3, "_pokes": _pokes()},
                            lambda lib, buf, f=fn, a=d0, b=d1, c=d2, e=d3:
                            f(buf, a, b, c, e))
                        assert not diffs, (f"entry={entry:#x} d0={d0} d1={d1} "
                                           f"d2={d2} d3={d3}\n{report(diffs)}")
