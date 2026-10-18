"""Pruebas diferenciales de la capa de texto (text.c).

print_char 0x10838, print_word 0x108a2, position_text 0x10fc4, advance_text_entry 0x10f8e,
measure_word 0x10946, measure_words 0x10998, wrap_line 0x109bc, print_words_fit 0x1090e,
print_line 0x10b24, print_spaces 0x10b46, text_index_arg 0x10fb4, print_line_center 0x10aee,
overlay_a_draw/frame 0x109fe/0x10a54, overlay_b_draw/frame 0x10a88/0x10acc.
"""
import ctypes
import random

import harness
from harness import differential, report

_u8p = ctypes.POINTER(ctypes.c_uint8)
_u32p = ctypes.POINTER(ctypes.c_uint32)

harness._lib.print_char.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.print_char.restype = None
harness._lib.print_word.argtypes = [_u8p, ctypes.c_uint32, _u32p]
harness._lib.print_word.restype = ctypes.c_uint32
harness._lib.position_text.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.position_text.restype = ctypes.c_uint32
harness._lib.advance_text_entry.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.advance_text_entry.restype = ctypes.c_uint32
harness._lib.measure_word.argtypes = [_u8p, ctypes.c_uint32, _u32p]
harness._lib.measure_word.restype = ctypes.c_uint32
for _n in ("measure_words", "wrap_line"):
    getattr(harness._lib, _n).argtypes = [_u8p, ctypes.c_uint32, _u32p, _u32p]
    getattr(harness._lib, _n).restype = None
harness._lib.print_words_fit.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32, _u32p, _u32p]
harness._lib.print_words_fit.restype = ctypes.c_uint32
harness._lib.print_line.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.print_line.restype = ctypes.c_uint32
harness._lib.print_spaces.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.print_spaces.restype = None
harness._lib.text_index_arg.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32, _u32p]
harness._lib.text_index_arg.restype = ctypes.c_uint32
harness._lib.print_line_center.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.print_line_center.restype = ctypes.c_uint32
for _n in ("overlay_a_draw", "overlay_b_draw"):
    getattr(harness._lib, _n).argtypes = [_u8p, ctypes.c_uint32]
    getattr(harness._lib, _n).restype = None
for _n in ("overlay_a_frame", "overlay_b_frame"):
    getattr(harness._lib, _n).argtypes = [_u8p]
    getattr(harness._lib, _n).restype = None

ADVANCE_TEXT_ENTRY = 0x10f8e
PRINT_CHAR = 0x10838
PRINT_WORD = 0x108a2
POSITION_TEXT = 0x10fc4
MEASURE_WORD = 0x10946
MEASURE_WORDS = 0x10998
WRAP_LINE = 0x109bc
PRINT_WORDS_FIT = 0x1090e
PRINT_LINE = 0x10b24
PRINT_SPACES = 0x10b46
TEXT_INDEX_ARG = 0x10fb4
PRINT_LINE_CENTER = 0x10aee
OVERLAY_A_DRAW = 0x109fe
OVERLAY_B_DRAW = 0x10a88
OVERLAY_A_FRAME = 0x10a54
OVERLAY_B_FRAME = 0x10acc

TABLE = 0x30000
STRBUF = 0x30000
SCREEN = 0x80000
DRAW_BASE = 0x8000
COLOR_TABLE = 0x1d382
CUR_STR_END = 0x1a4dc
TEXT_BOX_W = 0x1a512


def test_advance_text_entry():
    def pokes():
        return {
            TABLE: (0x10).to_bytes(2, "big"),
            TABLE + 0x10: (0x20).to_bytes(2, "big"),
            TABLE + 0x30: (0x30).to_bytes(2, "big"),
        }
    for d4 in (0, 1, 2, 3):
        diffs, info = differential(
            ADVANCE_TEXT_ENTRY, {"a1": TABLE, "d4": d4, "_pokes": pokes()},
            lambda lib, buf, n=d4: lib.advance_text_entry(buf, TABLE, n))
        assert not diffs, report(diffs)
        assert info["ret"] == info["regs"]["a1"], f"d4={d4}"


def _char_pokes(x, y):
    return {
        0x1a45e: SCREEN.to_bytes(4, "big"),
        0x1a46a: DRAW_BASE.to_bytes(4, "big"),
        0x1a50a: x.to_bytes(2, "big"),
        0x1a50c: y.to_bytes(2, "big"),
        0x1a4d8: COLOR_TABLE.to_bytes(4, "big"),
        SCREEN: bytes((0x5A + i * 3) & 0xFF for i in range(0x4000)),
    }


def test_print_char():
    for x in (0, 1, 2, 3, 15, 16, 17, 159, 319):
        for y in (0, 1, 50, 199):
            for ch in (0, 1, 0x20, 0x21, 0x3a, 0x40, 0x7f, 0xff):
                diffs, _ = differential(
                    PRINT_CHAR, {"d0": ch, "_pokes": _char_pokes(x, y)},
                    lambda lib, buf, c=ch: lib.print_char(buf, c))
                assert not diffs, f"x={x} y={y} ch={ch:#x}\n{report(diffs)}"


_CHAIN = {0: 4, 4: 6, 10: 3, 13: 5, 18: 2, 20: 7, 27: 1, 28: 9}


def _chain_pokes():
    return {STRBUF + p: bytes([v]) for p, v in _CHAIN.items()}


def test_position_text():
    for d4 in (0, 1, 4, 5, 6, 7, 8, 9, 0xffff):
        diffs, info = differential(
            POSITION_TEXT, {"a5": STRBUF, "d4": d4, "_pokes": _chain_pokes()},
            lambda lib, buf, d=d4: lib.position_text(buf, STRBUF, d))
        assert not diffs, f"d4={d4}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a5"], f"d4={d4}"


_WORD_CASES = [
    (bytes([0x21, 0x22, 0x23 | 0x40]), 3),
    (bytes([0x21, 0x22]), 2),
    (bytes([0x21 | 0x80, 0x40]), 2),
    (bytes([0x21 | 0xc0, 0x40]), 2),
    (bytes([0x25 | 0x80]), 1),
    (bytes([0x00, 0x40]), 2),
    (bytes([0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x40]), 7),
]


def test_print_word():
    for s, endoff in _WORD_CASES:
        for x in (0, 1, 15, 16):
            pokes = dict(_char_pokes(x, 0))
            pokes[STRBUF] = s
            pokes[CUR_STR_END] = (STRBUF + endoff).to_bytes(4, "big")
            a5 = ctypes.c_uint32(0)
            diffs, info = differential(
                PRINT_WORD, {"a5": STRBUF, "_pokes": pokes},
                lambda lib, buf, p=a5: lib.print_word(buf, STRBUF, ctypes.byref(p)))
            assert not diffs, f"{s.hex()} x={x}\n{report(diffs)}"
            assert a5.value == info["regs"]["a5"], f"{s.hex()} x={x}"
            assert info["ret"] == info["regs"]["d0"], f"{s.hex()} x={x}"


def _random_bank(rng, n, base=STRBUF):
    data = bytes(rng.randrange(256) for _ in range(n))
    pokes = {base + i: bytes([b]) for i, b in enumerate(data)}
    pokes[CUR_STR_END] = (base + n).to_bytes(4, "big")
    return data, pokes


def test_measure_word():
    rng = random.Random(7)
    for trial in range(300):
        data, pokes = _random_bank(rng, rng.randrange(1, 48))
        a4 = ctypes.c_uint32(0)
        diffs, info = differential(
            MEASURE_WORD, {"a5": STRBUF, "_pokes": pokes},
            lambda lib, buf, p=a4: lib.measure_word(buf, STRBUF, ctypes.byref(p)))
        assert not diffs, f"trial={trial} data={data.hex()}\n{report(diffs)}"
        assert a4.value == info["regs"]["a4"], f"trial={trial} data={data.hex()}"
        assert info["ret"] == info["regs"]["d0"], f"trial={trial} data={data.hex()}"


def test_measure_words():
    rng = random.Random(11)
    for trial in range(200):
        data, pokes = _random_bank(rng, rng.randrange(1, 48))
        d5, d6 = ctypes.c_uint32(0), ctypes.c_uint32(0)
        diffs, info = differential(
            MEASURE_WORDS, {"a5": STRBUF, "_pokes": pokes},
            lambda lib, buf, p=d5, q=d6:
            lib.measure_words(buf, STRBUF, ctypes.byref(p), ctypes.byref(q)))
        assert not diffs, f"trial={trial} data={data.hex()}\n{report(diffs)}"
        assert d5.value == info["regs"]["d5"], f"trial={trial}"
        assert d6.value == info["regs"]["d6"], f"trial={trial}"
        assert info["regs"]["a5"] == STRBUF, "A5 no restaurado"


def test_wrap_line():
    rng = random.Random(23)
    for trial in range(300):
        data, pokes = _random_bank(rng, rng.randrange(1, 60))
        pokes[TEXT_BOX_W] = rng.randrange(1, 40).to_bytes(2, "big")
        d5, d6 = ctypes.c_uint32(0), ctypes.c_uint32(0)
        diffs, info = differential(
            WRAP_LINE, {"a5": STRBUF, "_pokes": pokes},
            lambda lib, buf, p=d5, q=d6:
            lib.wrap_line(buf, STRBUF, ctypes.byref(p), ctypes.byref(q)))
        assert not diffs, f"trial={trial} data={data.hex()}\n{report(diffs)}"
        assert d5.value == info["regs"]["d5"], f"trial={trial}"
        assert d6.value == info["regs"]["d6"], f"trial={trial}"
        assert info["regs"]["a5"] == STRBUF, "A5 no restaurado"


def _draw_pokes(bank, width):
    pokes = {
        0x1a45e: SCREEN.to_bytes(4, "big"),
        0x1a46a: DRAW_BASE.to_bytes(4, "big"),
        0x1a50a: (0).to_bytes(2, "big"),
        0x1a50c: (0).to_bytes(2, "big"),
        0x1a4d8: COLOR_TABLE.to_bytes(4, "big"),
        CUR_STR_END: (STRBUF + len(bank)).to_bytes(4, "big"),
        TEXT_BOX_W: width.to_bytes(2, "big"),
    }
    for i, b in enumerate(bank):
        pokes[STRBUF + i] = bytes([b])
    return pokes


def test_print_words_fit():
    bank = bytes([0x21, 0x22 | 0x40, 0x23, 0x24 | 0x40, 0x25 | 0x40, 0x26, 0x27])
    for width in range(0, 12):
        a5, d6o = ctypes.c_uint32(0), ctypes.c_uint32(0)
        diffs, info = differential(
            PRINT_WORDS_FIT, {"a5": STRBUF, "d0": 0x1234, "_pokes": _draw_pokes(bank, width)},
            lambda lib, buf, p=a5, q=d6o:
            lib.print_words_fit(buf, STRBUF, 0x1234, ctypes.byref(p), ctypes.byref(q)))
        assert not diffs, f"width={width}\n{report(diffs)}"
        assert a5.value == info["regs"]["a5"], f"width={width}"
        assert d6o.value == info["regs"]["d6"], f"width={width}"
        assert info["ret"] == (info["regs"]["d0"] & 0xffff), f"width={width}"


def test_print_line():
    bank = bytes([0x21, 0x22 | 0x40, 0x23, 0x24 | 0x40, 0x25 | 0x40, 0x26, 0x27])
    for init in (0, 5):
        diffs, info = differential(
            PRINT_LINE, {"a5": STRBUF, "_pokes": _draw_pokes(bank, init)},
            lambda lib, buf: lib.print_line(buf, STRBUF))
        assert not diffs, f"init={init}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a5"], f"init={init}"


def test_print_spaces():
    for d6 in (0, 1, 4, 9):
        diffs, _ = differential(PRINT_SPACES, {"d6": d6, "_pokes": _char_pokes(0, 0)},
                                lambda lib, buf, n=d6: lib.print_spaces(buf, n))
        assert not diffs, f"d6={d6}\n{report(diffs)}"


SCRIPT = 0x42100


def test_text_index_arg():
    for d4 in (4, 5, 6, 0):
        pokes = _chain_pokes()
        pokes[SCRIPT] = bytes([0, 0])
        a6 = ctypes.c_uint32(SCRIPT)
        diffs, info = differential(
            TEXT_INDEX_ARG, {"a5": STRBUF, "d4": d4, "a6": SCRIPT, "_pokes": pokes},
            lambda lib, buf, d=d4, p=a6:
            lib.text_index_arg(buf, STRBUF, d, ctypes.byref(p)))
        assert not diffs, f"d4={d4}\n{report(diffs)}"
        assert a6.value == info["regs"]["a6"], f"d4={d4}"


def _line_pokes(bank, width):
    pokes = _char_pokes(0, 0)
    pokes[CUR_STR_END] = (STRBUF + len(bank)).to_bytes(4, "big")
    pokes[TEXT_BOX_W] = width.to_bytes(2, "big")
    pokes[STRBUF] = bank
    return pokes


def test_print_line_center():
    bank = bytes([0x21, 0x22 | 0x40, 0x23, 0x24 | 0x40, 0x25, 0x26 | 0x40,
                  0x27, 0x28 | 0x40, 0x29, 0x2A])
    for width in (10, 20, 40):
        diffs, info = differential(
            PRINT_LINE_CENTER, {"a5": STRBUF, "_pokes": _line_pokes(bank, width)},
            lambda lib, buf: lib.print_line_center(buf, STRBUF))
        assert not diffs, f"width={width}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a5"], f"width={width}"


def _overlay_pokes(sel, x, y):
    pokes = _char_pokes(0, 0)
    pokes[0x1acc6] = bytes([0x00])
    pokes[0x1aa6a] = x.to_bytes(2, "big")
    pokes[0x1aa6c] = y.to_bytes(2, "big")
    pokes[0x1ab45] = bytes([sel])
    pokes[0x1ab46] = bytes([sel])
    pokes[0x1ab49] = bytes([4])
    pokes[TEXT_BOX_W] = (8).to_bytes(2, "big")
    entry = bytes([3, 0x21, 0x22, 3, 0x23, 0x24, 3, 0x25, 0x26])
    pokes[0x41b76] = entry
    pokes[0x41586] = entry
    return pokes


def test_overlay_a_draw():
    for sel in (4, 5, 6):
        diffs, _ = differential(
            OVERLAY_A_DRAW, {"a6": SCRIPT, "_pokes": _overlay_pokes(sel, 0x10, 0x18)},
            lambda lib, buf: lib.overlay_a_draw(buf, SCRIPT))
        assert not diffs, f"sel={sel}\n{report(diffs)}"


def test_overlay_a_draw_mode87():
    pokes = _overlay_pokes(4, 0x10, 0x18)
    pokes[0x1acc6] = bytes([0x87])
    diffs, _ = differential(OVERLAY_A_DRAW, {"a6": SCRIPT, "_pokes": pokes},
                            lambda lib, buf: lib.overlay_a_draw(buf, SCRIPT))
    assert not diffs, report(diffs)


def test_overlay_b_draw():
    for sel in (4, 5, 6):
        diffs, _ = differential(
            OVERLAY_B_DRAW, {"a6": SCRIPT, "_pokes": _overlay_pokes(sel, 0x10, 0x18)},
            lambda lib, buf: lib.overlay_b_draw(buf, SCRIPT))
        assert not diffs, f"sel={sel}\n{report(diffs)}"


DEST = 0x90000
SRC = DEST - 0x8000


def _frame_pokes(x, y, width, size=0x8000):
    pokes = {
        0x1a45e: DEST.to_bytes(4, "big"),
        0x1a46a: (0x8000).to_bytes(4, "big"),
        0x1aa6a: x.to_bytes(2, "big"),
        0x1aa6c: y.to_bytes(2, "big"),
        0x1ab49: bytes([width]),
        0x1acc6: bytes([0x00]),
        0x0d8ea: bytes([0x41]),
        0x1d402: bytes([0x5A]),
    }
    for i in range(size):
        pokes[SRC + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[DEST + i] = bytes([(i * 11 + 7) & 0xFF])
    return pokes


def test_overlay_a_frame():
    for width in (1, 8, 16, 40):
        diffs, _ = differential(OVERLAY_A_FRAME, {"_pokes": _frame_pokes(0x11, 0x20, width)},
                                lambda lib, buf: lib.overlay_a_frame(buf))
        assert not diffs, f"width={width}\n{report(diffs)}"


def test_overlay_b_frame():
    diffs, _ = differential(OVERLAY_B_FRAME, {"_pokes": _frame_pokes(0x11, 0x20, 4)},
                            lambda lib, buf: lib.overlay_b_frame(buf))
    assert not diffs, report(diffs)
