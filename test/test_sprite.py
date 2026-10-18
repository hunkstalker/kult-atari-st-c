"""Pruebas diferenciales del blitter de sprites y save/restore (sprite.c).

blit_sprite 0x124f8, save_rect 0x1258a, restore_rect 0x124f2, blit_rect 0x12510,
blit_rect_vbl 0x124da, copy_scratch_rows 0x123a0.
"""
import ctypes

import harness
from harness import differential, report

_u8p = ctypes.POINTER(ctypes.c_uint8)
harness._lib.blit_sprite.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.blit_sprite.restype = None
harness._lib.save_rect.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                   ctypes.c_uint32, ctypes.c_uint32]
harness._lib.save_rect.restype = ctypes.c_uint32
harness._lib.restore_rect.argtypes = [_u8p]
harness._lib.restore_rect.restype = None
harness._lib.blit_rect.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32,
                                   ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.blit_rect.restype = None
harness._lib.blit_rect_vbl.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                       ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.blit_rect_vbl.restype = None
harness._lib.copy_scratch_rows.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.copy_scratch_rows.restype = None

BLIT_SPRITE = 0x124f8
SAVE_RECT = 0x1258a
RESTORE_RECT = 0x124f2
BLIT_RECT = 0x12510
BLIT_RECT_VBL = 0x124da
COPY_SCRATCH_ROWS = 0x123a0
DESC = 0x30000
DEST = 0x78000
SAVE_DESC = 0xd8f0
SRC_SCREEN = 0x70000
VBL_COUNTER = 0x1acd4
VBL_SITE = 0x1359a


def _desc(outer, inner, dest, flags, stride, data):
    e = bytearray([outer, inner])
    e += dest.to_bytes(4, "big")
    e += flags.to_bytes(2, "big")
    e += stride.to_bytes(4, "big")
    e += data
    return bytes(e)


def test_blit_sprite_null():
    diffs, _ = differential(BLIT_SPRITE, {"a1": 0, "_pokes": {}},
                            lambda lib, buf: lib.blit_sprite(buf, 0))
    assert not diffs, report(diffs)


def test_blit_sprite():
    data = bytes(range(0x10, 0x40))
    for outer in (1, 2, 3, 5):
        for inner in (1, 3):
            for flags in (0, 4, 8, 12, 3, 15, 1, 2, 5, 7, 0x10, 0xff):
                for stride in (0, 2, 6, 8):
                    desc = _desc(outer, inner, DEST, flags, stride, data)
                    diffs, _ = differential(BLIT_SPRITE, {"a1": DESC, "_pokes": {DESC: desc}},
                                            lambda lib, buf: lib.blit_sprite(buf, DESC))
                    assert not diffs, (
                        f"outer={outer} inner={inner} flags={flags} stride={stride}\n{report(diffs)}")


def _screen_pokes(cols, rows, a0=SRC_SCREEN):
    pokes = {}
    for i in range(0xA0 * (rows + 1) + 0x20):
        pokes[a0 + i] = bytes([(0x11 * i + 0x37) & 0xff])
    return pokes


def test_save_rect():
    """0x1258a empaqueta el rect en el descriptor 0xd8f0 (inverso del blit)."""
    for cols in (1, 2, 3, 5):
        for rows in (1, 2, 3):
            for d7 in (0, 4, 8, 12):
                pokes = _screen_pokes(cols, rows)
                diffs, info = differential(
                    SAVE_RECT, {"d2": cols, "d3": rows, "a0": SRC_SCREEN, "d7": d7,
                                "_pokes": pokes},
                    lambda lib, buf, c=cols, r=rows, p=d7:
                    lib.save_rect(buf, c, r, SRC_SCREEN, p))
                assert not diffs, f"cols={cols} rows={rows} d7={d7}\n{report(diffs)}"
                assert info["ret"] == info["regs"]["a1"], f"cols={cols} rows={rows} d7={d7}"


def test_restore_rect():
    """0x124f2 reblitea el descriptor 0xd8f0 (ida y vuelta con el save)."""
    pokes = {SAVE_DESC: _desc(2, 3, DEST, 4, 4, bytes(range(0x40, 0x60)))}
    pokes.update({DEST + i: bytes([0x0F if i % 2 else 0xF0]) for i in range(0x300)})
    diffs, _ = differential(RESTORE_RECT, {"_pokes": pokes},
                            lambda lib, buf: lib.restore_rect(buf))
    assert not diffs, report(diffs)


def test_blit_rect():
    SRC = 0x70000
    for outer in (1, 2, 3):
        for inner in (1, 3):
            for d7 in (0, 4, 8, 12):
                pokes = {SRC + i: bytes([(i * 7 + 3) & 0xFF]) for i in range(0x40)}
                pokes.update({DEST + i: bytes([(i * 11 + 5) & 0xFF]) for i in range(0x200)})
                diffs, _ = differential(
                    BLIT_RECT, {"a0": DEST, "a1": SRC, "d2": outer, "d3": inner, "d7": d7,
                                "a3": 2, "_pokes": pokes},
                    lambda lib, buf, o=outer, i=inner, p=d7:
                    lib.blit_rect(buf, DEST, SRC, o, i, p, 2))
                assert not diffs, f"outer={outer} inner={inner} d7={d7}\n{report(diffs)}"


def test_blit_rect_vbl():
    SRC = 0x70000
    pokes = {0x1a534: (2).to_bytes(4, "big"), VBL_COUNTER: bytes([0x00])}
    pokes.update({SRC + i: bytes([(i * 7 + 3) & 0xFF]) for i in range(0x40)})
    pokes.update({DEST + i: bytes([(i * 11 + 5) & 0xFF]) for i in range(0x200)})
    diffs, _ = differential(
        BLIT_RECT_VBL, {"a0": DEST, "a1": SRC, "d2": 2, "d3": 2, "d7": 0, "_pokes": pokes},
        lambda lib, buf: lib.blit_rect_vbl(buf, DEST, SRC, 2, 2, 0),
        schedule=[{"pc": VBL_SITE, "nth": 1, "addr": VBL_COUNTER, "width": 1, "value": 0x01}],
        wait_sites=[VBL_SITE])
    assert not diffs, report(diffs)


def test_copy_scratch_rows():
    for d4 in (0, 1, 4):
        for d5 in (0, 1, 3):
            pokes = {0xd8f0 + i: bytes([(i * 5 + 1) & 0xFF]) for i in range(0x100)}
            pokes.update({DEST + i: bytes([(i * 11 + 5) & 0xFF]) for i in range(0x400)})
            diffs, _ = differential(
                COPY_SCRATCH_ROWS, {"a0": DEST, "d4": d4, "d5": d5, "_pokes": pokes},
                lambda lib, buf, w=d4, h=d5: lib.copy_scratch_rows(buf, DEST, w, h))
            assert not diffs, f"d4={d4} d5={d5}\n{report(diffs)}"
