"""Pruebas diferenciales de los helpers de estado (state.c).

clear_flag 0x172ba, clear_object_flags 0x15192, set_cursor_clip 0x14a98,
fix_sample_flag 0x14bdc. Todo el efecto es la imagen.
"""
import ctypes

import harness
from harness import differential, report

_u8p = ctypes.POINTER(ctypes.c_uint8)
harness._lib.clear_flag.argtypes = [_u8p]
harness._lib.clear_flag.restype = None
harness._lib.clear_object_flags.argtypes = [_u8p]
harness._lib.clear_object_flags.restype = None
harness._lib.set_cursor_clip.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.set_cursor_clip.restype = None
harness._lib.fix_sample_flag.argtypes = [_u8p]
harness._lib.fix_sample_flag.restype = None

CLEAR_FLAG = 0x172ba
CLEAR_OBJECT_FLAGS = 0x15192
SET_CURSOR_CLIP = 0x14a98
FIX_SAMPLE_FLAG = 0x14bdc


def test_clear_flag():
    pokes = {
        0x1a806: (0x30000).to_bytes(4, "big"),
        0x30000: b"\xff",
        0x1acaa: b"\xff\xff",
    }
    diffs, _ = differential(CLEAR_FLAG, {"_pokes": pokes},
                            lambda lib, buf: lib.clear_flag(buf))
    assert not diffs, report(diffs)


def test_clear_object_flags():
    pokes = {0x1acd3: bytes([0x55])}
    for i in range(0x29 * 5 + 5):
        pokes[0x1aa72 + i] = bytes([(0x80 + i * 7) & 0xFF])
    diffs, _ = differential(CLEAR_OBJECT_FLAGS, {"_pokes": pokes},
                            lambda lib, buf: lib.clear_object_flags(buf))
    assert not diffs, report(diffs)


def test_set_cursor_clip():
    for d0 in (0, 1, 5, 9):
        pokes = {}
        for i in range(0x196dc, 0x196dc + 0x40 * 4):
            pokes[i] = bytes([(i * 3 + 1) & 0xFF])
        for i in range(0x1949c, 0x1949c + 0x40 * 64):
            pokes[i] = bytes([(i * 5 + 2) & 0xFF])
        diffs, _ = differential(
            SET_CURSOR_CLIP, {"a0": 0x12345678, "d0": d0, "_pokes": pokes},
            lambda lib, buf, v=d0: lib.set_cursor_clip(buf, 0, v))
        assert not diffs, f"d0={d0}\n{report(diffs)}"


def test_fix_sample_flag():
    pokes = {}
    for i in range(0x3f * 6 + 8):
        pokes[0x1a8f0 + i] = bytes([0x81 if i % 6 == 0 else (0x80 + i) & 0xFF])
    diffs, _ = differential(FIX_SAMPLE_FLAG, {"_pokes": pokes},
                            lambda lib, buf: lib.fix_sample_flag(buf))
    assert not diffs, report(diffs)
