"""Tests diferenciales del reveal pixelado.

FUN_00012ef6 @ 0x12ef6: mezcla 4 palabras de A0-0x8000 en A0 bajo la máscara D4
rotada por la fase D7.  FUN_00012e2e @ 0x12e2e: recorre la región en pasos dando el
dot por pixel (con retardo; el retardo no cambia memoria).
"""
import ctypes

import harness
from harness import differential, report

DOT = 0x12ef6
REVEAL = 0x12e2e
X0 = 0x1a532
Y0 = 0x1a530
W = 0x1a52e
TOTAL = 0x1a49c
STEP = 0x1a577
DELAY = 0x1a56a
DEST = 0x90000
SRC = DEST - 0x8000

_u8p = ctypes.POINTER(ctypes.c_uint8)
harness._lib.reveal_dot.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.reveal_dot.restype = None
harness._lib.reveal_region.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32, ctypes.c_uint32]
harness._lib.reveal_region.restype = None


def _screen_pokes(n=0x2000):
    pokes = {}
    for i in range(n):
        pokes[SRC + i] = bytes([(i * 3 + 1) & 0xff])
        pokes[DEST + i] = bytes([(i * 11 + 7) & 0xff])
    return pokes


def test_fun_12ef6_dot():
    for a0 in (DEST, DEST + 0x40):
        for d4 in (0x8000, 0xf000, 0x00f0, 0xffff, 0xaaaa, 0x0000):
            for d7 in (0, 1, 2, 4, 8, 15):
                pokes = _screen_pokes()
                regs = {"a0": a0, "d4": d4, "d7": d7, "_pokes": pokes}
                diffs, _ = differential(
                    DOT, regs,
                    lambda lib, buf, a=a0, m=d4, ph=d7: lib.reveal_dot(buf, a, m, ph))
                assert not diffs, f"a0={a0:#x} d4={d4:#x} d7={d7}\n{report(diffs)}"


def test_fun_12e2e_reveal():
    for d0, d1, d2, d3, step in ((2, 1, 1, 1, 1), (0, 0, 2, 2, 1),
                                 (4, 3, 1, 3, 2), (1, 1, 1, 2, 1),
                                 # step que desborda el total: cubre `acc > total`
                                 (0, 0, 1, 2, 3), (0, 0, 2, 3, 5)):
        pokes = _screen_pokes()
        pokes[0x1a45e] = DEST.to_bytes(4, "big")
        pokes[STEP] = bytes([step])
        pokes[DELAY] = (0).to_bytes(2, "big")
        pokes[0x0d8ea] = bytes([0x41])
        regs = {"d0": d0, "d1": d1, "d2": d2, "d3": d3, "_pokes": pokes}
        diffs, _ = differential(
            REVEAL, regs,
            lambda lib, buf, a=d0, b=d1, c=d2, e=d3: lib.reveal_region(buf, a, b, c, e))
        assert not diffs, f"d0={d0} d1={d1} d2={d2} d3={d3} step={step}\n{report(diffs)}"


FUN_12FB6 = 0x12FB6
harness._lib.masked_word4.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.masked_word4.restype = None


def test_fun_12fb6():
    for d0 in (0, 0x1234, 0xFFFF, 0x00FF):
        for d7 in (0, 1, 4, 8, 12, 15):
            pokes = {0x40000 + i: bytes([(i * 7 + 3) & 0xFF]) for i in range(0x10)}
            diffs, _ = differential(
                FUN_12FB6, {"a0": 0x40000, "d0": d0, "d7": d7, "_pokes": pokes},
                lambda lib, buf, v=d0, p=d7: lib.masked_word4(buf, 0x40000, v, p))
            assert not diffs, f"d0={d0:#x} d7={d7}\n{report(diffs)}"


FUN_12F28 = 0x12F28
harness._lib.reveal_pixel.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.reveal_pixel.restype = ctypes.c_uint32


def _elem_pokes(w, h):
    pokes = {
        0x2751e: bytes([5, 0, 0, 0x00, 0x00]),
        0x1a252: bytes([w]), 0x1a253: bytes([h]),
        0x1a254: (0x1111).to_bytes(2, "big"),
        0x1a256: (0x2222).to_bytes(2, "big"),
        0x1a258: (0x3333).to_bytes(2, "big"),
        0x1a25a: (0x4444).to_bytes(2, "big"),
        0x31072: (0x10).to_bytes(2, "big"), 0x31074: bytes([2, 2]),
        0x1a45e: DEST.to_bytes(4, "big"),
        0x1a851: bytes([0x22]), 0x1ad0a: bytes([0]),
    }
    for i in range(0x100):
        pokes[0x31076 + i] = bytes([(i * 7 + 3) & 0xFF])
    for i in range(0x2000):
        pokes[0x3b1ae + i] = bytes([(i * 5 + 1) & 0xFF])
        pokes[0xD8F0 + i] = bytes([(i * 11 + 5) & 0xFF])
    for i in range(10 * 0x10):
        pokes[0x1a850 + i] = bytes([0])
    pokes.update(_screen_pokes(0x10000))
    return pokes


def test_fun_12f28():
    for skip in (0, 1):
        for w, h, x, y in ((16, 17, 8, 4), (8, 34, 4, 2)):
            pokes = _elem_pokes(w, h)
            pokes[0x1a570] = bytes([skip])
            pokes[0x50000] = bytes([0x00, 1, x, y])
            sch = ([{"pc": 0x1359a, "nth": 1, "addr": 0x1acd4, "width": 1, "value": 1}]
                   if skip else [])
            pokes[0x1acd4] = bytes([0])
            diffs, info = differential(
                FUN_12F28, {"a6": 0x50000, "_pokes": pokes},
                lambda lib, buf: lib.reveal_pixel(buf, 0x50000),
                schedule=sch, wait_sites=[0x1359a])
            assert not diffs, f"skip={skip} w={w} h={h}\n{report(diffs)}"
            assert info["ret"] == info["regs"]["a6"], "a6"
