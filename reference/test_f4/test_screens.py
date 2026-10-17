"""Differential tests for the screen-buffer family: copy_screen @ 0x121c6, swap_screens @ 0x121de,
screen_address @ 0x10fec.

The first cases of the reconstruction: pure, hardware-free routines whose whole effect is
the image (or the returned address), so they are verifiable end to end.
"""
import ctypes

import harness
from harness import differential, report

COPY_SCREEN = 0x121c6
COPY_SCREEN_A6 = 0x121c4
SWAP_SCREENS = 0x121de
SCREEN_OFFSET = 0x10fec
SCREEN_OFFSET_RAW = 0x10fee
FUN_159BC = 0x159bc

_u8p = ctypes.POINTER(ctypes.c_uint8)
harness._lib.g_screen_offset.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_screen_offset.restype = ctypes.c_uint32
harness._lib.g_screen_offset_raw.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                             ctypes.POINTER(ctypes.c_uint32)]
harness._lib.g_screen_offset_raw.restype = ctypes.c_uint32
harness._lib.g_fun_121c4.argtypes = [_u8p]
harness._lib.g_fun_121c4.restype = None
harness._lib.g_fun_159bc.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_159bc.restype = None

FRONT = 0x80000   # screen_ptr_a
BACK = 0x78000    # screen_ptr_b


def _pokes():
    """Point the two screen globals at mapped scratch and put sentinels in both buffers.

    Without non-zero sentinels the base image is all zeroes at both ends and the copy would
    be invisible to the byte diff.
    """
    return {
        0x1a45e: FRONT.to_bytes(4, "big"),   # screen_ptr_a (copy dest, swap A)
        0x1a462: BACK.to_bytes(4, "big"),    # screen_ptr_b (copy source, swap B)
        BACK: bytes.fromhex("deadbeefcafebabe"),
        BACK + 31992: bytes.fromhex("0102030405060708"),
        FRONT: bytes.fromhex("1112131415161718"),
        FRONT + 31992: bytes.fromhex("2122232425262728"),
    }


def test_copy_screen():
    diffs, _ = differential(COPY_SCREEN, {"_pokes": _pokes()},
                            lambda lib, buf: lib.g_copy_screen(buf), poison=True)
    assert not diffs, report(diffs)


def test_swap_screens():
    diffs, _ = differential(SWAP_SCREENS, {"_pokes": _pokes()},
                            lambda lib, buf: lib.g_swap_screens(buf), poison=True)
    assert not diffs, report(diffs)


def test_fun_121c4():
    """FUN_000121c4 is `addq.l #1,a6` falling into copy_screen; same image effect."""
    diffs, _ = differential(COPY_SCREEN_A6, {"_pokes": _pokes()},
                            lambda lib, buf: lib.g_fun_121c4(buf))
    assert not diffs, report(diffs)


def test_screen_address():
    """screen_address @ 0x10fec returns the planar byte address in a0; no memory effect."""
    pokes = {0x1a45e: (0x80000).to_bytes(4, "big")}   # screen_ptr_a
    for d0 in (0, 1, 2, 3, 15, 16, 17, 31, 0x1234, 0xffff, 0x12340000):
        for d1 in (0, 1, 100, 199, 0xffff):
            regs = {"d0": d0, "d1": d1, "_pokes": pokes}
            diffs, info = differential(SCREEN_OFFSET, regs,
                                       lambda lib, buf, x=d0, y=d1: lib.g_screen_offset(buf, x, y))
            assert not diffs, report(diffs)
            assert info["ret"] == info["regs"]["a0"], f"x={d0:#x} y={d1:#x}"


def test_screen_address_raw():
    """screen_address_raw @ 0x10fee: idem sin el lsl.w #2; d7 = d0 & 0xf."""
    pokes = {0x1a45e: (0x80000).to_bytes(4, "big")}
    for d0 in (0, 1, 2, 3, 15, 16, 17, 31, 0x1234, 0xffff):
        for d1 in (0, 1, 100, 199):
            d7 = ctypes.c_uint32()
            regs = {"d0": d0, "d1": d1, "_pokes": pokes}
            diffs, info = differential(SCREEN_OFFSET_RAW, regs,
                                       lambda lib, buf, x=d0, y=d1, p=ctypes.byref(d7):
                                       lib.g_screen_offset_raw(buf, x, y, p))
            assert not diffs, report(diffs)
            assert info["ret"] == info["regs"]["a0"], f"x={d0:#x} y={d1:#x}"
            assert d7.value == info["regs"]["d7"], f"x={d0:#x} y={d1:#x}"


SRC_ROWS = 0x40000
DST_ROWS = 0x60000


def test_fun_159bc():
    """FUN_000159bc copies D3+1 rows of 16 bytes from A1 into A2 (stride 0xa0)."""
    for d3 in (0, 1, 2, 7):
        pokes = {}
        for i in range((d3 + 1) * 16):
            pokes[SRC_ROWS + i] = bytes([(i * 3 + 1) & 0xff])
        for i in range(d3 * 0xa0 + 16):
            pokes[DST_ROWS + i] = bytes([(i * 11 + 5) & 0xff])
        diffs, _ = differential(
            FUN_159BC, {"a1": SRC_ROWS, "a2": DST_ROWS, "d3": d3, "_pokes": pokes},
            lambda lib, buf, n=d3: lib.g_fun_159bc(buf, SRC_ROWS, DST_ROWS, n))
        assert not diffs, f"d3={d3}\n{report(diffs)}"


FUN_121B6 = 0x121B6
FUN_12190 = 0x12190
harness._lib.g_fun_121b6.argtypes = [_u8p]
harness._lib.g_fun_121b6.restype = None
harness._lib.g_fun_12190.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_12190.restype = None


def test_fun_121b6():
    pokes = {
        0x1a45e: (0x80000).to_bytes(4, "big"),
        0x1a462: (0x78000).to_bytes(4, "big"),
        0x80000: bytes((i * 3 + 1) & 0xFF for i in range(0x10000)),
        0x78000: bytes((i * 7 + 5) & 0xFF for i in range(0x10000)),
    }
    diffs, _ = differential(FUN_121B6, {"_pokes": pokes},
                            lambda lib, buf: lib.g_fun_121b6(buf))
    assert not diffs, report(diffs)


def test_fun_12190():
    for d2 in (1, 16, 40):
        for d3 in (1, 3, 8):
            for d7 in (0, 4, 8, 12, 15):
                pokes = {
                    0x1a45e: (0x90000).to_bytes(4, "big"),
                    0x1a462: (0x88000).to_bytes(4, "big"),
                    0x0d8ea: bytes([0x41]),
                    0x1d402: bytes([0x5A]),
                }
                for i in range(0x10000):
                    pokes[0x88000 + i] = bytes([(i * 3 + 1) & 0xFF])
                    pokes[0x90000 + i] = bytes([(i * 11 + 7) & 0xFF])
                diffs, _ = differential(
                    FUN_12190, {"a0": 0x90000, "d2": d2, "d3": d3, "d7": d7,
                                "_pokes": pokes},
                    lambda lib, buf, w=d2, h=d3, p=d7:
                    lib.g_fun_12190(buf, 0x90000, w, h, p))
                assert not diffs, f"d2={d2} d3={d3} d7={d7}\n{report(diffs)}"


FUN_16B76 = 0x16B76
harness._lib.g_fun_16b76.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_16b76.restype = None


def test_fun_16b76():
    for d2 in (0, 1, 3):
        for d3 in (0, 1, 4):
            pokes = {
                0x40000: bytes((i * 3 + 1) & 0xFF for i in range(0x4000)),
                0x48000: bytes((i * 7 + 5) & 0xFF for i in range(0x4000)),
            }
            diffs, _ = differential(
                FUN_16B76, {"a3": 0x48000, "d2": d2, "d3": d3, "_pokes": pokes},
                lambda lib, buf, w=d2, h=d3: lib.g_fun_16b76(buf, 0x48000, w, h))
            assert not diffs, f"d2={d2} d3={d3}\n{report(diffs)}"


FUN_10FF4 = 0x10FF4
harness._lib.g_fun_10ff4.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32)]
harness._lib.g_fun_10ff4.restype = ctypes.c_uint32


def test_fun_10ff4():
    """0x10ff4: offset de (d0,d1) sumado a A0; d7 = d0 & 0xf (sin cargar A0)."""
    for a0 in (0, 0x80000, 0x1234):
        for d0 in (0, 3, 0x3c, 0xffff):
            for d1 in (0, 1, 7, 0xffff):
                d7 = ctypes.c_uint32()
                diffs, info = differential(
                    0x10FF4, {"a0": a0, "d0": d0, "d1": d1},
                    lambda lib, buf, a=a0, x=d0, y=d1, p=d7:
                    lib.g_fun_10ff4(buf, a, x, y, ctypes.byref(p)))
                assert not diffs, f"a0={a0:#x} d0={d0:#x} d1={d1:#x}\n{report(diffs)}"
                assert info["ret"] == info["regs"]["a0"], "a0"
                assert d7.value == info["regs"]["d7"], "d7"


FUN_15944 = 0x15944
harness._lib.g_fun_15944.argtypes = [_u8p]
harness._lib.g_fun_15944.restype = None


def test_fun_15944():
    pokes = {0x1a462: (0x88000).to_bytes(4, "big"), 0x1a573: bytes([0])}
    for i in range(0x9000):
        pokes[0x88000 + i] = bytes([(i * 7 + 1) & 0xFF])
    for i in range(0x4000):
        pokes[0x27000 + i] = bytes([(i * 3 + 5) & 0xFF])
    diffs, _ = differential(FUN_15944, {"_pokes": pokes},
                            lambda lib, buf: lib.g_fun_15944(buf))
    assert not diffs, report(diffs)


FUN_16B2E = 0x16B2E
harness._lib.g_fun_16b2e.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_16b2e.restype = None

DEST_16B2E = 0x90000


def test_fun_16b2e():
    """OR de filas de 4 words desde 0xd8f0+A5 sobre A3 (stride D2/D3)."""
    for d2, d3, a5 in ((1, 0, 0), (2, 1, 0), (1, 1, 8), (7, 0, 0x30)):
        pokes = {}
        span = a5 + (d3 + 1) * 0x40 + 0x40
        for i in range(span):
            pokes[0xD8F0 + i] = bytes([(i * 7 + 1) & 0xFF])
        for i in range(0x400):
            pokes[DEST_16B2E + i] = bytes([(i * 3 + 5) & 0xFF])
        diffs, _ = differential(
            0x16B2E, {"d2": d2, "d3": d3, "a3": DEST_16B2E, "a5": a5, "_pokes": pokes},
            lambda lib, buf: lib.g_fun_16b2e(buf, d2, d3, DEST_16B2E, a5))
        assert not diffs, f"d2={d2} d3={d3} a5={a5:#x}\n{report(diffs)}"
