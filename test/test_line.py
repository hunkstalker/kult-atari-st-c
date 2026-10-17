"""Pruebas diferenciales del trazo de línea Bresenham (line.c).

reveal_line 0x13d0e, reveal_lines_h 0x13e14, reveal_lines_v 0x13eae.
Las esperas de VBL se liberan con `schedule` (TRAP_MODEL.md Fase 8).
"""
import ctypes

import harness
from harness import differential, report

_u8p = ctypes.POINTER(ctypes.c_uint8)
harness._lib.reveal_line.argtypes = [_u8p]
harness._lib.reveal_line.restype = None
harness._lib.reveal_lines_h.argtypes = [_u8p] + [ctypes.c_uint32] * 4
harness._lib.reveal_lines_h.restype = None
harness._lib.reveal_lines_v.argtypes = [_u8p] + [ctypes.c_uint32] * 4
harness._lib.reveal_lines_v.restype = None

REVEAL_LINE = 0x13d0e
REVEAL_LINES_H = 0x13e14
REVEAL_LINES_V = 0x13eae
SCREEN = 0x90000
BACK = 0x88000
VBL_COUNTER = 0x1acd4
VBL_SITE = 0x1359a


def _screen_pokes():
    pokes = {0x1a45e: SCREEN.to_bytes(4, "big")}
    for i in range(0x10000):
        pokes[SCREEN + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[BACK + i] = bytes([(i * 11 + 7) & 0xFF])
    return pokes


def test_reveal_line():
    for x0, x1, y0, y1 in ((0x40, 0x50, 0x14, 0x20), (0x50, 0x40, 0x20, 0x14),
                           (0x40, 0x40, 0x14, 0x30), (0x40, 0x70, 0x14, 0x14)):
        pokes = _screen_pokes()
        pokes[0x1a516] = x0.to_bytes(2, "big")
        pokes[0x1a518] = x1.to_bytes(2, "big")
        pokes[0x1a51a] = y0.to_bytes(2, "big")
        pokes[0x1a51c] = y1.to_bytes(2, "big")
        diffs, _ = differential(REVEAL_LINE, {"_pokes": pokes},
                                lambda lib, buf: lib.reveal_line(buf))
        assert not diffs, f"({x0:#x},{y0:#x})->({x1:#x},{y1:#x})\n{report(diffs)}"


def _vbl_k(n):
    return [{"pc": VBL_SITE, "nth": k, "addr": VBL_COUNTER, "width": 1, "value": k}
            for k in range(1, n + 1)]


def test_reveal_lines_h():
    # d2=1 -> 4 trazos + *(0x1a52c) en la 2ª tanda; 0x1a52c=0 es degenerado (0x10000).
    for n2 in (1, 2):
        pokes = _screen_pokes()
        pokes[0x1a52c] = n2.to_bytes(2, "big")
        pokes[VBL_COUNTER] = bytes([0x00])
        diffs, _ = differential(
            REVEAL_LINES_H, {"d0": 4, "d1": 4, "d2": 1, "d3": 2, "_pokes": pokes},
            lambda lib, buf: lib.reveal_lines_h(buf, 4, 4, 1, 2),
            schedule=_vbl_k(4 + n2), wait_sites=[VBL_SITE])
        assert not diffs, f"n2={n2}\n{report(diffs)}"


def test_reveal_lines_v():
    pokes = _screen_pokes()
    pokes[0x1a52c] = (1).to_bytes(2, "big")
    pokes[0x1a52e] = (1).to_bytes(2, "big")
    pokes[VBL_COUNTER] = bytes([0x00])
    diffs, _ = differential(
        REVEAL_LINES_V, {"d0": 4, "d1": 4, "d2": 1, "d3": 1, "_pokes": pokes},
        lambda lib, buf: lib.reveal_lines_v(buf, 4, 4, 1, 1),
        schedule=_vbl_k(1 + 4 + 2), wait_sites=[VBL_SITE])
    assert not diffs, report(diffs)
