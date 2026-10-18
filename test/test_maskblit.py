"""Pruebas diferenciales del blit de filas enmascarado y la tabla de hints (maskblit.c).

blit_mask_setup 0x12698, blit_mask_row 0x123c0, mask_restore_rows 0x1215e,
blit_mask_rows 0x12466, hint_slot_alloc 0x120a8, hint_select_on/off 0x120dc/0x120e4,
hint_find 0x1213a, hint_blit_entry 0x1222e, hint_blit_stroke 0x1232e,
hint_blit_2/3 0x12368/0x12384.
"""
import ctypes

import harness
from harness import differential, report

_u8p = ctypes.POINTER(ctypes.c_uint8)
_u32p = ctypes.POINTER(ctypes.c_uint32)

harness._lib.blit_mask_setup.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.blit_mask_setup.restype = None
harness._lib.blit_mask_row.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.blit_mask_row.restype = None
harness._lib.mask_restore_rows.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                           ctypes.c_uint32, ctypes.c_uint32]
harness._lib.mask_restore_rows.restype = None
harness._lib.blit_mask_rows.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.blit_mask_rows.restype = None
harness._lib.hint_slot_alloc.argtypes = [_u8p] + [ctypes.c_uint32] * 7
harness._lib.hint_slot_alloc.restype = None
for _n in ("hint_select_on", "hint_select_off", "hint_find"):
    getattr(harness._lib, _n).argtypes = [_u8p, ctypes.c_uint32, _u32p]
    getattr(harness._lib, _n).restype = None
harness._lib.hint_blit_entry.argtypes = [_u8p, ctypes.c_uint32, _u32p]
harness._lib.hint_blit_entry.restype = None
for _n in ("hint_blit_stroke", "hint_blit_2", "hint_blit_3"):
    getattr(harness._lib, _n).argtypes = [_u8p, _u32p]
    getattr(harness._lib, _n).restype = None

BLIT_MASK_SETUP = 0x12698
BLIT_MASK_ROW = 0x123c0
MASK_RESTORE_ROWS = 0x1215e
BLIT_MASK_ROWS = 0x12466
HINT_SLOT_ALLOC = 0x120a8
HINT_SELECT_ON = 0x120dc
HINT_SELECT_OFF = 0x120e4
HINT_FIND = 0x1213a
HINT_BLIT_ENTRY = 0x1222e
HINT_BLIT_STROKE = 0x1232e
HINT_BLIT_2 = 0x12368
HINT_BLIT_3 = 0x12384

MASK_FIRST = 0x1a538
MASK_LAST = 0x1a53a
MASK_GROUPS = 0x1a53c
LOW_D8EA = 0x0d8ea
DAT_1D402 = 0x1d402
DEST = 0x90000
SRC = DEST - 0x8000
VBL_COUNTER = 0x1acd4
VBL_SITE = 0x1359a


def test_blit_mask_setup():
    for d2 in (0, 1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 31, 32, 39, 40, 63, 64, 100, 320):
        for d7 in (0, 1, 2, 3, 4, 7, 8, 11, 12, 15):
            diffs, _ = differential(
                BLIT_MASK_SETUP, {"d2": d2, "d7": d7},
                lambda lib, buf, w=d2, b=d7: lib.blit_mask_setup(buf, w, b))
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


def test_blit_mask_row():
    for groups in (1, 2, 3, 4, 5, 8):
        for left, right in ((0x0000, 0x0000), (0xffff, 0xffff), (0x8000, 0x0001),
                            (0xf000, 0x000f), (0x00f0, 0x0f00), (0xaaaa, 0x5555)):
            diffs, _ = differential(
                BLIT_MASK_ROW, {"a0": DEST, "a1": SRC, "_pokes": _row_pokes(left, right, groups)},
                lambda lib, buf: lib.blit_mask_row(buf, DEST, SRC))
            assert not diffs, f"groups={groups} L={left:#x} R={right:#x}\n{report(diffs)}"


def _rows_pokes(mode_byte):
    pokes = {LOW_D8EA: bytes([mode_byte]), DAT_1D402: bytes([0x5a])}
    for i in range(0x800):
        pokes[SRC + i] = bytes([(i * 3 + 1) & 0xff])
        pokes[DEST + i] = bytes([(i * 11 + 7) & 0xff])
    return pokes


def test_mask_restore_rows():
    for d3 in (1, 2, 3, 5):
        for d2 in (1, 8, 16, 17, 32, 40):
            for d7 in (0, 1, 4, 8, 12, 15):
                for mode_byte in (0x41, 0x00):
                    diffs, _ = differential(
                        MASK_RESTORE_ROWS,
                        {"a0": DEST, "d2": d2, "d3": d3, "d7": d7, "_pokes": _rows_pokes(mode_byte)},
                        lambda lib, buf, w=d2, h=d3, b=d7:
                        lib.mask_restore_rows(buf, DEST, w, h, b))
                    assert not diffs, (
                        f"d2={d2} d3={d3} d7={d7} mode={mode_byte:#x}\n{report(diffs)}")


def _row4_pokes(left, right, groups):
    pokes = {0x1a538: left.to_bytes(2, "big"),
             0x1a53a: right.to_bytes(2, "big"),
             0x1a53c: groups.to_bytes(2, "big"),
             VBL_COUNTER: bytes([0x00])}
    for i in range(0x400):
        pokes[SRC + i] = bytes([(i * 7 + 3) & 0xFF])
        pokes[DEST + i] = bytes([(i * 13 + 5) & 0xFF])
    return pokes


def test_blit_mask_rows():
    for groups in (2, 3, 4, 5):
        for left, right in ((0x0000, 0x0000), (0xffff, 0xffff), (0xf000, 0x000f)):
            for d3 in (1, 2, 4):
                diffs, _ = differential(
                    BLIT_MASK_ROWS, {"a0": DEST, "a1": SRC, "d3": d3,
                                     "_pokes": _row4_pokes(left, right, groups)},
                    lambda lib, buf, h=d3: lib.blit_mask_rows(buf, DEST, SRC, h),
                    schedule=[{"pc": VBL_SITE, "nth": 1, "addr": VBL_COUNTER,
                               "width": 1, "value": 0x01}],
                    wait_sites=[VBL_SITE])
                assert not diffs, (f"groups={groups} L={left:#x} R={right:#x} d3={d3}\n"
                                   f"{report(diffs)}")


def _hint_offset_pokes():
    """Pokea la entrada 0x1a850 (index 0x1001) y la 0x2a840 (index 0) con valores
    distintos, para fijar el truncado `lsl.w #4`."""
    pokes = {0x1a851: bytes([0x11]), 0x1ad0a: bytes([0x00]),
             0x1a45e: (0x90000).to_bytes(4, "big")}
    for base in (0x1a850, 0x2a840):
        pokes[base] = (0x2222).to_bytes(2, "big")
        pokes[base + 2] = (0x90000).to_bytes(4, "big")
        pokes[base + 6] = (0x0007).to_bytes(2, "big")
        pokes[base + 8] = (0x0001).to_bytes(2, "big")
        pokes[base + 10] = (0x0002).to_bytes(2, "big")
        pokes[base + 12] = (0x0003).to_bytes(2, "big")
        pokes[base + 14] = (0x0004).to_bytes(2, "big")
    return pokes


def test_hint_select_offset_truncado():
    """`subq.w #1; lsl.w #4; andi.l #$ffff`: index 0 -> 0x1a850+0xfff0 = 0x2a840
    (no 0x11a840) y index 0x1001 -> 0x1a850 (no 0x2a850)."""
    for d0 in (0, 0x1001):
        o = (ctypes.c_uint32 * 8)()
        diffs, info = differential(
            HINT_SELECT_OFF, {"d0": d0, "_pokes": _hint_offset_pokes()},
            lambda lib, buf, n=d0, p=o: lib.hint_select_off(buf, n, p))
        assert not diffs, f"d0={d0:#x}\n{report(diffs)}"
        for i, reg in enumerate(("a0", "d4", "d7", "d0", "d1", "d2", "d3")):
            assert o[i] == info["regs"][reg], f"d0={d0:#x} out[{i}]({reg})"


def _hint_table_pokes():
    pokes = {0x1a851: bytes([0x11]), 0x1ad0a: bytes([0x00]),
             0x1a45e: (0x90000).to_bytes(4, "big")}
    for i in range(10):
        base = 0x1a850 + i * 0x10
        word = (i + 1) & 0xffff
        pokes[base] = word.to_bytes(2, "big")
        pokes[base + 2] = (0x90000 + i * 0x10).to_bytes(4, "big")
        for k, v in enumerate((1, 2, 3, 4, 5)):
            pokes[base + 6 + k * 2] = v.to_bytes(2, "big")
    return pokes


def test_hint_slot_alloc():
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
            HINT_SLOT_ALLOC,
            {"d4": 0x1234, "a0": 0x0abcde, "d7": 0x5678, "d0": 0x1111,
             "d1": 0x2222, "d2": 0x3333, "d3": 0x4444, "_pokes": pokes},
            lambda lib, buf: lib.hint_slot_alloc(buf, 0x1234, 0x0abcde, 0x5678, 0x1111,
                                                 0x2222, 0x3333, 0x4444))
        assert not diffs, f"first_free={first_free}\n{report(diffs)}"


def _run_hint(entry, fn, d0):
    o = (ctypes.c_uint32 * 8)()
    diffs, info = differential(entry, {"d0": d0, "_pokes": _hint_table_pokes()},
                               lambda lib, buf, n=d0, p=o: fn(buf, n, p))
    assert not diffs, f"{entry:#x} d0={d0}\n{report(diffs)}"
    for i, reg in enumerate(("a0", "d4", "d7", "d0", "d1", "d2", "d3")):
        assert o[i] == info["regs"][reg], f"{entry:#x} d0={d0} out[{i}]({reg})"


def test_hint_select():
    for d0 in (1, 3, 10):
        _run_hint(HINT_SELECT_ON, harness._lib.hint_select_on, d0)
        _run_hint(HINT_SELECT_OFF, harness._lib.hint_select_off, d0)
    for d0 in (1, 5, 11):                     # 11 no está: d5=1
        o = (ctypes.c_uint32 * 8)()
        diffs, info = differential(HINT_FIND, {"d0": d0, "_pokes": _hint_table_pokes()},
                                   lambda lib, buf, n=d0, p=o: lib.hint_find(buf, n, p))
        assert not diffs, f"d0={d0}\n{report(diffs)}"
        assert o[7] == info["regs"]["d5"], f"d0={d0} d5"


def test_hint_blit_entry():
    pokes = _hint_table_pokes()
    pokes[0x30000] = bytes([0, 3])
    for i in range(0x400):
        pokes[0x88000 + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x90000 + i] = bytes([(i * 11 + 7) & 0xFF])
    a6 = ctypes.c_uint32(0)
    diffs, info = differential(HINT_BLIT_ENTRY, {"a6": 0x30000, "_pokes": pokes},
                               lambda lib, buf, p=a6: lib.hint_blit_entry(buf, 0x30000,
                                                                         ctypes.byref(p)))
    assert not diffs, report(diffs)
    assert a6.value == info["regs"]["a6"]


def test_hint_blit_stroke():
    pokes = _hint_table_pokes()
    pokes[0x1a850] = (1).to_bytes(2, "big")
    pokes[0x1a484] = bytes([0x55])
    for i in range(0x400):
        pokes[0x88000 + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x90000 + i] = bytes([(i * 11 + 7) & 0xFF])
    a6 = ctypes.c_uint32(0x30000)
    diffs, info = differential(HINT_BLIT_STROKE, {"a6": 0x30000, "_pokes": pokes},
                               lambda lib, buf, p=ctypes.byref(a6): lib.hint_blit_stroke(buf, p))
    assert not diffs, report(diffs)
    assert a6.value == info["regs"]["a6"]


def test_hint_blit_2_3():
    for entry, fn, word in ((HINT_BLIT_2, harness._lib.hint_blit_2, 2),
                            (HINT_BLIT_3, harness._lib.hint_blit_3, 3)):
        pokes = _hint_table_pokes()
        pokes[0x1a850] = word.to_bytes(2, "big")
        for i in range(0x400):
            pokes[0x88000 + i] = bytes([(i * 3 + 1) & 0xFF])
            pokes[0x90000 + i] = bytes([(i * 11 + 7) & 0xFF])
        a6 = ctypes.c_uint32(0x30000)
        diffs, info = differential(entry, {"a6": 0x30000, "_pokes": pokes},
                                   lambda lib, buf, p=ctypes.byref(a6), f=fn: f(buf, p))
        assert not diffs, f"{entry:#x}\n{report(diffs)}"
        assert a6.value == info["regs"]["a6"]
