"""Differential tests for the sprite blitter: blit_sprite @ 0x124f8.

Descriptor at A1: [u8 outer][u8 inner][long dest][word flags][long src_stride][src words...].
The whole effect is the image (destination planes + the 0x55b02 scratch), so the byte diff
verifies it end to end.
"""
import ctypes

import harness
from harness import differential, report

BLIT_SPRITE = 0x124f8
DESC = 0x30000
DEST = 0x78000

_u8p = ctypes.POINTER(ctypes.c_uint8)
harness._lib.g_blit_sprite.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_blit_sprite.restype = None


def _desc(outer, inner, dest, flags, stride, data):
    e = bytearray([outer, inner])
    e += dest.to_bytes(4, "big")
    e += flags.to_bytes(2, "big")
    e += stride.to_bytes(4, "big")
    e += data
    return bytes(e)


def test_blit_sprite_null():
    diffs, _ = differential(BLIT_SPRITE, {"a1": 0, "_pokes": {}},
                            lambda lib, buf: lib.g_blit_sprite(buf, 0))
    assert not diffs, report(diffs)


def test_blit_sprite():
    data = bytes(range(0x10, 0x40))
    for outer in (1, 2, 3, 5):
        for inner in (1, 3):
            for flags in (0, 4, 8, 12, 3, 15):
                for stride in (2, 6):
                    desc = _desc(outer, inner, DEST, flags, stride, data)
                    diffs, _ = differential(BLIT_SPRITE, {"a1": DESC, "_pokes": {DESC: desc}},
                                            lambda lib, buf: lib.g_blit_sprite(buf, DESC))
                    assert not diffs, (
                        f"outer={outer} inner={inner} flags={flags} stride={stride}\n{report(diffs)}")


SAVE = 0x1258a
RESTORE = 0x124f2
SAVE_DESC = 0xd8f0
SRC_SCREEN = 0x70000


harness._lib.g_fun_1258a.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_1258a.restype = ctypes.c_uint32
harness._lib.g_fun_124f2.argtypes = [_u8p]
harness._lib.g_fun_124f2.restype = None


def _screen_pokes(cols, rows, a0=SRC_SCREEN):
    pokes = {}
    for i in range(0xA0 * (rows + 1) + 0x20):
        pokes[a0 + i] = bytes([(0x11 * i + 0x37) & 0xff])
    return pokes


def test_fun_1258a_save():
    """0x1258a packs the screen rect into the descriptor at 0xd8f0 (inverse of the blit)."""
    for cols in (1, 2, 3, 5):
        for rows in (1, 2, 3):
            for d7 in (0, 4, 8, 12):
                pokes = _screen_pokes(cols, rows)
                diffs, info = differential(
                    SAVE, {"d2": cols, "d3": rows, "a0": SRC_SCREEN, "d7": d7,
                           "_pokes": pokes},
                    lambda lib, buf, c=cols, r=rows, p=d7:
                    lib.g_fun_1258a(buf, c, r, SRC_SCREEN, p))
                assert not diffs, (f"cols={cols} rows={rows} d7={d7}\n{report(diffs)}")
                assert info["ret"] == info["regs"]["a1"], (f"cols={cols} rows={rows} d7={d7}")


def test_fun_124f2_restore():
    """0x124f2 blits back the descriptor at 0xd8f0 (round-trip with the save)."""
    pokes = {SAVE_DESC: _desc(2, 3, DEST, 4, 4, bytes(range(0x40, 0x60)))}
    pokes.update({DEST + i: bytes([0x0F if i % 2 else 0xF0]) for i in range(0x300)})
    diffs, _ = differential(RESTORE, {"_pokes": pokes},
                            lambda lib, buf: lib.g_fun_124f2(buf))
    assert not diffs, report(diffs)


FUN_12510 = 0x12510
FUN_124DA = 0x124DA
SRC = 0x70000
harness._lib.g_fun_12510.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_12510.restype = None
harness._lib.g_fun_124da.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_124da.restype = None


def test_fun_12510():
    for outer in (1, 2, 3):
        for inner in (1, 3):
            for d7 in (0, 4, 8, 12):
                pokes = {SRC + i: bytes([(i * 7 + 3) & 0xFF]) for i in range(0x40)}
                pokes.update({DEST + i: bytes([(i * 11 + 5) & 0xFF]) for i in range(0x200)})
                diffs, _ = differential(
                    FUN_12510, {"a0": DEST, "a1": SRC, "d2": outer, "d3": inner, "d7": d7,
                                "a3": 2, "_pokes": pokes},
                    lambda lib, buf, o=outer, i=inner, p=d7:
                    lib.g_fun_12510(buf, DEST, SRC, o, i, p, 2))
                assert not diffs, (f"outer={outer} inner={inner} d7={d7}\n{report(diffs)}")


def test_fun_124da():
    VBL_COUNTER = 0x1acd4
    pokes = {0x1a534: (2).to_bytes(4, "big"), VBL_COUNTER: bytes([0x00])}
    pokes.update({SRC + i: bytes([(i * 7 + 3) & 0xFF]) for i in range(0x40)})
    pokes.update({DEST + i: bytes([(i * 11 + 5) & 0xFF]) for i in range(0x200)})
    diffs, _ = differential(
        FUN_124DA, {"a0": DEST, "a1": SRC, "d2": 2, "d3": 2, "d7": 0, "_pokes": pokes},
        lambda lib, buf: lib.g_fun_124da(buf, DEST, SRC, 2, 2, 0),
        schedule=[{"pc": 0x1359a, "nth": 1, "addr": VBL_COUNTER, "width": 1, "value": 0x01}],
        wait_sites=[0x1359a])
    assert not diffs, report(diffs)


FUN_123A0 = 0x123A0
harness._lib.g_fun_123a0.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_123a0.restype = None


def test_fun_123a0():
    for d4 in (0, 1, 4):
        for d5 in (0, 1, 3):
            pokes = {0xd8f0 + i: bytes([(i * 5 + 1) & 0xFF]) for i in range(0x100)}
            pokes.update({DEST + i: bytes([(i * 11 + 5) & 0xFF]) for i in range(0x400)})
            diffs, _ = differential(
                FUN_123A0, {"a0": DEST, "d4": d4, "d5": d5, "_pokes": pokes},
                lambda lib, buf, w=d4, h=d5: lib.g_fun_123a0(buf, DEST, w, h))
            assert not diffs, f"d4={d4} d5={d5}\n{report(diffs)}"
