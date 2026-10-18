"""Tests de los envoltorios/entradas pequenas (misc.c) y 0x134da (handlers.c)."""
import ctypes

import harness
from harness import differential, report, console_key

_u8p = ctypes.POINTER(ctypes.c_uint8)
SCREEN = 0x90000
BACK = 0x88000
VBL_COUNTER = 0x1acd4
VBL_SITE = 0x1359a

for _n, _args in (("func_1104a", [_u8p] + [ctypes.c_uint32] * 5),
                  ("func_127f0", [_u8p] + [ctypes.c_uint32] * 5),
                  ("func_12324", [_u8p]),
                  ("func_12fea", [_u8p, ctypes.c_uint32]),
                  ("func_16512", [_u8p, ctypes.c_uint32]),
                  ("func_16554", [_u8p, ctypes.c_uint32]),
                  ("func_16576", [_u8p, ctypes.c_uint32]),
                  ("func_18d26", [_u8p, ctypes.c_uint32]),
                  ("func_17472", [_u8p])):
    getattr(harness._lib, _n).argtypes = _args
    getattr(harness._lib, _n).restype = ctypes.c_uint32


def test_fun_17472():
    pokes = {0x1aa8b: bytes([0x55]), 0x1acae: (0x1234).to_bytes(2, "big")}
    diffs, _ = differential(0x17472, {"_pokes": pokes},
                            lambda lib, buf: lib.func_17472(buf))
    assert not diffs, report(diffs)


def test_fun_1104a():
    for d2, d3 in ((4, 3), (8, 2)):
        pokes = {0x1a534: (0x28).to_bytes(4, "big")}
        for i in range(0x4000):
            pokes[SCREEN + i] = bytes([(i * 3 + 1) & 0xFF])
            pokes[0x40000 + i] = bytes([(i * 7 + 3) & 0xFF])
        diffs, _ = differential(
            0x1104A, {"a0": SCREEN, "a1": 0x40000, "d2": d2, "d3": d3, "d7": 0,
                      "_pokes": pokes},
            lambda lib, buf, a=d2, b=d3: lib.func_1104a(buf, SCREEN, 0x40000, a, b, 0))
        assert not diffs, f"d2={d2} d3={d3}\n{report(diffs)}"


def test_fun_127f0():
    pokes = {0x1a534: (0x28).to_bytes(4, "big"), VBL_COUNTER: bytes([0x00])}
    for i in range(0x4000):
        pokes[SCREEN + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x40000 + i] = bytes([(i * 7 + 3) & 0xFF])
    diffs, _ = differential(
        0x127F0, {"a0": SCREEN, "a1": 0x40000, "d2": 0x10, "d3": 3, "d7": 0,
                  "_pokes": pokes},
        lambda lib, buf: lib.func_127f0(buf, SCREEN, 0x40000, 0x10, 3, 0))
    assert not diffs, report(diffs)


def test_fun_12324():
    pokes = {0x1a44f: bytes([0x00]), 0x1a456: bytes([0x00])}
    pokes.update(console_key("a"))
    diffs, info = differential(0x12324, {"_pokes": pokes},
                               lambda lib, buf: lib.func_12324(buf))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["d0"]


def _hint_table():
    pokes = {0x1a851: bytes([0x11]), 0x1ad0a: bytes([0x00]),
             0x1a45e: SCREEN.to_bytes(4, "big")}
    for i in range(10):
        base = 0x1a850 + i * 0x10
        pokes[base] = ((i + 1) & 0xffff).to_bytes(2, "big")
        pokes[base + 2] = (SCREEN + i * 0x10).to_bytes(4, "big")
        for k, v in enumerate((1, 2, 3, 4, 5)):
            pokes[base + 6 + k * 2] = v.to_bytes(2, "big")
    return pokes


def test_fun_12fea():
    pokes = _hint_table()
    pokes[0x30000] = bytes([0, 3])
    for i in range(0x8000):
        pokes[SCREEN + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[BACK + i] = bytes([(i * 11 + 7) & 0xFF])
    diffs, info = differential(0x12FEA, {"a6": 0x30000, "_pokes": pokes},
                               lambda lib, buf: lib.func_12fea(buf, 0x30000))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


def _icon_zones(desc):
    pokes = {
        0x1a45e: SCREEN.to_bytes(4, "big"),
        0x1a462: BACK.to_bytes(4, "big"),
        0x0d8ea: bytes([0x41]),
        0x1d402: bytes([0x5A]),
    }
    for i in range(16):
        pokes[0x1a776 + i * 4] = desc.to_bytes(4, "big")
    for i in range(0x10000):
        pokes[SCREEN + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[BACK + i] = bytes([(i * 11 + 7) & 0xFF])
    return pokes


def test_fun_16512_16554():
    for entry, fn in ((0x16512, harness._lib.func_16512),
                      (0x16554, harness._lib.func_16554)):
        desc = 0x3A000
        e = bytearray([2, 3]) + (SCREEN - 0x8000).to_bytes(4, "big") + \
            (8).to_bytes(2, "big") + (4).to_bytes(4, "big")
        pokes = _icon_zones(desc)
        pokes[desc] = bytes(e)
        diffs, info = differential(entry, {"a6": 0x30000, "_pokes": pokes},
                                   lambda lib, buf, f=fn: f(buf, 0x30000))
        assert not diffs, f"{entry:#x}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], f"{entry:#x} a6"


def test_fun_16576():
    pokes = {
        0x1a7ca: (0x40000).to_bytes(4, "big"),
        0x1a7ce: (0x40010).to_bytes(4, "big"),
        0x1acd2: bytes([0x00]),
    }
    for i in range(0x29):
        base = 0x1AA72 + i * 5
        pokes[base] = bytes([0x00])
        pokes[base + 1] = bytes([0x00])
        pokes[base + 2] = bytes([0x11])
        pokes[base + 3] = bytes([0x00])
        pokes[base + 4] = bytes([0x00])
    pokes[0x40000] = bytes([8, 0x20, 4, 0x20, 0x10, 0, 0, 0])
    pokes[0x40008] = bytes([0x30, 0x40, 0x30, 0x40, 0x10, 0, 0, 0])
    diffs, info = differential(0x16576, {"a6": 0x30000, "_pokes": pokes},
                               lambda lib, buf: lib.func_16576(buf, 0x30000))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


def test_fun_18d26():
    CELL = 0x30000
    DESC = 0x30010
    pokes = {
        0x1a45e: SCREEN.to_bytes(4, "big"),
        0x1a462: BACK.to_bytes(4, "big"),
        0x1a81e: CELL.to_bytes(4, "big"),
        CELL: DESC.to_bytes(4, "big"),
    }
    dest = BACK + 0x160
    pokes[DESC] = (bytes([4, 4]) + dest.to_bytes(4, "big") +
                   (0).to_bytes(2, "big") + (4).to_bytes(4, "big"))
    for i in range(0x10000):
        pokes[SCREEN + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[BACK + i] = bytes([(i * 11 + 7) & 0xFF])
    diffs, info = differential(0x18D26, {"a6": 0x30000, "_pokes": pokes},
                               lambda lib, buf: lib.func_18d26(buf, 0x30000))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]
