"""Differential test for the cursor save-buffer restore: FUN_0001060a @ 0x1060a.

Copies 16 rows of 16 bytes from 0x99ce to *(0x9ace) with an 0xa0 dest stride. Pure: the
whole effect is the image.
"""
import ctypes

import harness
from harness import differential, report

RESTORE = 0x1060a
DRAW = 0x10526
DEST = 0x78000
SRC = 0x199ce

_u8p = ctypes.POINTER(ctypes.c_uint8)
harness._lib.g_restore_cursor.argtypes = [_u8p]
harness._lib.g_restore_cursor.restype = None
harness._lib.g_draw_cursor.argtypes = [_u8p]
harness._lib.g_draw_cursor.restype = None


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


def test_draw_cursor():
    for hs in (0, 1):
        diffs, _ = differential(DRAW, {"_pokes": _draw_pokes(hs)},
                                lambda lib, buf: lib.g_draw_cursor(buf))
        assert not diffs, f"hotspot={hs}\n{report(diffs)}"


def test_restore_cursor():
    src = bytes((i * 7 + 1) & 0xff for i in range(16 * 16))
    pokes = {0x19ace: DEST.to_bytes(4, "big"), SRC: src}
    diffs, _ = differential(RESTORE, {"_pokes": pokes},
                            lambda lib, buf: lib.g_restore_cursor(buf))
    assert not diffs, report(diffs)
