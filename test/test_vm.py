"""Differential tests for the script-VM helpers.

walk_entry @ 0x12950: skip D1 byte-length entries from A4; outputs the updated A4 and A5.
"""
import ctypes

import harness
from harness import differential, report

WALK_ENTRY = 0x12950
FUN_13F4C = 0x13f4c
FUN_13F56 = 0x13f56
TABLE = 0x30000

_u8p = ctypes.POINTER(ctypes.c_uint8)
harness._lib.walk_entry.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                      ctypes.POINTER(ctypes.c_uint32),
                                      ctypes.POINTER(ctypes.c_uint32)]
harness._lib.walk_entry.restype = None
harness._lib.text_table_entry.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.text_table_entry.restype = None
harness._lib.run_text_table.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.run_text_table.restype = None

# [u8 len][data], len includes itself: entries at 0(len 4), 4(len 6), 10(len 3), 13(len 5)...
_CHAIN = {0: 4, 4: 6, 10: 3, 13: 5, 18: 2, 20: 7, 27: 1}


def _pokes():
    return {TABLE + p: bytes([v]) for p, v in _CHAIN.items()}


def test_walk_entry():
    for d1 in (0, 1, 2, 3, 4, 5, 8):
        a4o = ctypes.c_uint32()
        a5o = ctypes.c_uint32()
        regs = {"a4": TABLE, "d1": d1, "_pokes": _pokes()}
        diffs, info = differential(WALK_ENTRY, regs,
                                   lambda lib, buf, a4=TABLE, d=d1, p4=a4o, p5=a5o:
                                   lib.walk_entry(buf, a4, d, ctypes.byref(p4), ctypes.byref(p5)))
        assert not diffs, f"d1={d1}\n{report(diffs)}"
        assert a4o.value == info["regs"]["a4"], f"d1={d1} a4"
        assert a5o.value == info["regs"]["a5"], f"d1={d1} a5"


ARPLA = 0x24982
TEXT_TABLE = 0x41fb0
SCREEN = 0x90000


def _text_window_pokes():
    """One 3-byte entry in the ARPLA region plus the text table it points into."""
    pokes = {
        ARPLA: bytes([4]),              # one entry: a4o = ARPLA + 4, a5o = ARPLA + 1
        ARPLA + 1: bytes([0]),          # entry[0]: text-table skip
        ARPLA + 2: bytes([0x01]),       # entry[1]: char
        ARPLA + 3: bytes([0x00]),       # entry[2]: flags
        TEXT_TABLE: (2).to_bytes(2, "big"),
        TEXT_TABLE + 2: bytes([2]),     # rows
        TEXT_TABLE + 3: bytes([2]),     # cols
        0x1a45e: SCREEN.to_bytes(4, "big"),
    }
    for i in range(0x40):
        pokes[TEXT_TABLE + 4 + i] = bytes([(0x10 + i) & 0xff])
    for i in range(0x300):
        pokes[SCREEN - 0x8000 + i] = bytes([0x0f if i % 2 else 0xf0])
    return pokes


def test_fun_13f56():
    """FUN_00013f56 runs FUN_00014138 over the ARPLA entries from the D1th onward."""
    for d0 in (0, 0x1234):
        diffs, _ = differential(FUN_13F56, {"a6": 0x30000, "d0": d0, "d1": 1,
                                            "_pokes": _text_window_pokes()},
                                lambda lib, buf, v=d0: lib.run_text_table(buf, 0x30000, v, 1))
        assert not diffs, report(diffs)


def test_fun_13f4c():
    """FUN_00013f4c seeds D0 = 0 and D1 = A6[1], then falls into FUN_00013f56."""
    pokes = _text_window_pokes()
    pokes[0x30001] = bytes([1])
    diffs, _ = differential(FUN_13F4C, {"a6": 0x30000, "_pokes": pokes},
                            lambda lib, buf: lib.text_table_entry(buf, 0x30000))
    assert not diffs, report(diffs)
