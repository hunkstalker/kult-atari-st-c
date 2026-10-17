"""Differential tests for the text layer: advance_text_entry @ 0x10f8e, print_char @ 0x10838.

advance_text_entry walks D4 word-length entries ([word len][data] at A1), then sets
cur_str_end (0x1a4dc) = A1 + word(A1) and returns A1 + 2.

print_char draws the glyph for D0's low byte at the text cursor (0x1a50a/0x1a50c) using
the 6x4 font at image 0x1d202 and the colour table at *(0x1a4d8), then advances the cursor.
Both are pure: their whole effect is the image (and, for the walker, the returned A1).
"""
import ctypes

import harness
from harness import differential, report

ADVANCE_TEXT_ENTRY = 0x10f8e
PRINT_CHAR = 0x10838
TABLE = 0x30000

_u8p = ctypes.POINTER(ctypes.c_uint8)
harness._lib.g_advance_text_entry.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_advance_text_entry.restype = ctypes.c_uint32
harness._lib.g_print_char.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_print_char.restype = None
harness._lib.g_print_word.argtypes = [_u8p, ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32)]
harness._lib.g_print_word.restype = ctypes.c_uint32
harness._lib.g_position_text.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_position_text.restype = ctypes.c_uint32
PRINT_WORD = 0x108a2
POSITION_TEXT = 0x10fc4
STRBUF = 0x30000


def _pokes():
    """Three entries with lengths 0x10, 0x20, 0x30 and arbitrary data between."""
    return {
        TABLE: (0x10).to_bytes(2, "big"),
        TABLE + 0x10: (0x20).to_bytes(2, "big"),
        TABLE + 0x30: (0x30).to_bytes(2, "big"),
    }


def test_advance_text_entry():
    for d4 in (0, 1, 2, 3):
        regs = {"a1": TABLE, "d4": d4, "_pokes": _pokes()}
        diffs, info = differential(ADVANCE_TEXT_ENTRY, regs,
                                   lambda lib, buf, n=d4: lib.g_advance_text_entry(buf, TABLE, n))
        assert not diffs, report(diffs)
        assert info["ret"] == info["regs"]["a1"], f"d4={d4}"


SCREEN = 0x80000
DRAW_BASE = 0x8000   # the real game's logical-vs-draw offset (screen_ptr_a - draw buffer)
COLOR_TABLE = 0x1d382


def _char_pokes(x, y):
    return {
        0x1a45e: SCREEN.to_bytes(4, "big"),        # screen_ptr_a
        0x1a46a: DRAW_BASE.to_bytes(4, "big"),      # draw base subtracted
        0x1a50a: x.to_bytes(2, "big"),              # text cursor x
        0x1a50c: y.to_bytes(2, "big"),              # text cursor y
        0x1a4d8: COLOR_TABLE.to_bytes(4, "big"),    # colour table
    }


def test_print_char():
    """Draw a glyph at every pixel-alignment class of x and a spread of chars/rows."""
    for x in (0, 1, 2, 3, 15, 16, 17, 159, 319):
        for y in (0, 1, 50, 199):
            for ch in (0, 1, 0x20, 0x21, 0x3a, 0x40, 0x7f, 0xff):
                regs = {"d0": ch, "_pokes": _char_pokes(x, y)}
                diffs, _ = differential(PRINT_CHAR, regs,
                                        lambda lib, buf, c=ch: lib.g_print_char(buf, c))
                assert not diffs, f"x={x} y={y} ch={ch:#x}\n{report(diffs)}"


# Strings: glyph|flags. 0x40 ends the word (return 0), 0x80 appends 'E', 0xc0 appends 'A'.
_WORD_CASES = [
    (bytes([0x21, 0x22, 0x23 | 0x40]), 3),   # plain word, separator stops it
    (bytes([0x21, 0x22]), 2),                 # reaches cur_str_end -> 0x1a508 = 0xff
    (bytes([0x21 | 0x80, 0x40]), 2),          # digraph: append 'E'
    (bytes([0x21 | 0xc0, 0x40]), 2),          # digraph: append 'A'
    (bytes([0x25 | 0x80]), 1),                # 'E' appended, then end
    (bytes([0x00, 0x40]), 2),                 # zero glyph takes the L_ee path
    (bytes([0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x40]), 7),   # longer run
]


# Byte-length entry chain: the entry at p has length byte table[p], the next is p + table[p].
_CHAIN = {0: 4, 4: 6, 10: 3, 13: 5, 18: 2, 20: 7, 27: 1, 28: 9}


def _chain_pokes():
    return {STRBUF + p: bytes([v]) for p, v in _CHAIN.items()}


def test_position_text():
    for d4 in (0, 1, 4, 5, 6, 7, 8, 9, 0xffff):
        regs = {"a5": STRBUF, "d4": d4, "_pokes": _chain_pokes()}
        diffs, info = differential(POSITION_TEXT, regs,
                                   lambda lib, buf, d=d4: lib.g_position_text(buf, STRBUF, d))
        assert not diffs, f"d4={d4}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a5"], f"d4={d4}"


def test_print_word():
    for s, endoff in _WORD_CASES:
        for x in (0, 1, 15, 16):
            pokes = dict(_char_pokes(x, 0))
            pokes[STRBUF] = s
            pokes[0x1a4dc] = (STRBUF + endoff).to_bytes(4, "big")
            regs = {"a5": STRBUF, "_pokes": pokes}
            a5 = ctypes.c_uint32(0)
            diffs, info = differential(PRINT_WORD, regs,
                                       lambda lib, buf, p=a5:
                                       lib.g_print_word(buf, STRBUF, ctypes.byref(p)))
            assert not diffs, f"{s.hex()} x={x}\n{report(diffs)}"
            assert a5.value == info["regs"]["a5"], f"{s.hex()} x={x}"
            assert info["ret"] == info["regs"]["d0"], f"{s.hex()} x={x}"
