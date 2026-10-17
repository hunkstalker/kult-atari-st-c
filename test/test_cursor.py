"""Pruebas diferenciales del cursor: cursor_draw @ 0x10526, cursor_restore @ 0x1060a.

Todo el efecto es la imagen (guardado/restauración del área 16×16 bajo el cursor).
"""
import ctypes

import harness
from harness import differential, report

_u8p = ctypes.POINTER(ctypes.c_uint8)
harness._lib.cursor_draw.argtypes = [_u8p]
harness._lib.cursor_draw.restype = None
harness._lib.cursor_restore.argtypes = [_u8p]
harness._lib.cursor_restore.restype = None

CURSOR_DRAW = 0x10526
CURSOR_RESTORE = 0x1060a
DEST = 0x78000
SAVE_BUF = 0x199ce


def _draw_pokes(hotspot):
    return {
        0x1a450: (100).to_bytes(2, "big"),         # cursor x
        0x1a452: (50).to_bytes(2, "big"),          # cursor y
        0x19700: (0).to_bytes(2, "big"),           # clip x
        0x19702: (0).to_bytes(2, "big"),           # clip y
        0x1a48a: bytes([hotspot]),                 # hotspot flag
        0x199ca: (0x30000).to_bytes(4, "big"),     # sprite ptr
        0x1a45e: (0x78000).to_bytes(4, "big"),     # screen_ptr_a
        0x30000: bytes((i * 7 + 3) & 0xff for i in range(64)),
        0x79e00: bytes((i * 5 + 1) & 0xff for i in range(0xd00)),
    }


def test_cursor_draw():
    for hs in (0, 1):
        diffs, _ = differential(CURSOR_DRAW, {"_pokes": _draw_pokes(hs)},
                                lambda lib, buf: lib.cursor_draw(buf))
        assert not diffs, f"hotspot={hs}\n{report(diffs)}"


def test_cursor_restore():
    src = bytes((i * 7 + 1) & 0xff for i in range(16 * 16))
    pokes = {0x19ace: DEST.to_bytes(4, "big"), SAVE_BUF: src}
    diffs, _ = differential(CURSOR_RESTORE, {"_pokes": pokes},
                            lambda lib, buf: lib.cursor_restore(buf))
    assert not diffs, report(diffs)
