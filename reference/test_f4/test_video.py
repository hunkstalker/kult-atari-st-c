"""Differential tests for the palette setter: set_palette @ 0x135da -> FUN_00016300 @ 0x16300.

XBIOS Setpalette touches no image byte, so the byte diff sees nothing: the ordered OS-event
ledger (the kit's os_setpalette() door) is what verifies the reconstruction.
"""
import ctypes

import harness
from harness import differential, report

SET_PALETTE = 0x16300
FUN_162E2 = 0x162e2
FUN_162DA = 0x162da
FUN_162CE = 0x162ce
FUN_16210 = 0x16210
FUN_16230 = 0x16230
FUN_1627A = 0x1627a
PAL = 0x18ad2   # any mapped address; the value is the argument the trap carries
PAL_BUF = 0x30000
PAL_TABLE = 0x1a10a

IDX_1A579 = 0x1a579
TBL_2741E = 0x2741e
DAT_1ACD7 = 0x1acd7
DAT_1AB60 = 0x1ab60
DAT_1A526 = 0x1a526
LIST_1A8F0 = 0x1a8f0
DAT_1ACD2 = 0x1acd2

_u8p = ctypes.POINTER(ctypes.c_uint8)
harness._lib.g_set_palette.argtypes = [_u8p]
harness._lib.g_set_palette.restype = None
for _name in ("g_fun_162e2", "g_fun_162da", "g_fun_162ce"):
    getattr(harness._lib, _name).argtypes = (
        [_u8p, ctypes.c_uint32] if _name != "g_fun_162da" else [_u8p])
    getattr(harness._lib, _name).restype = None
harness._lib.g_fun_16210.argtypes = [_u8p]
harness._lib.g_fun_16210.restype = None
harness._lib.g_fun_16230.argtypes = [_u8p]
harness._lib.g_fun_16230.restype = ctypes.c_uint32
harness._lib.g_fun_1627a.argtypes = [_u8p]
harness._lib.g_fun_1627a.restype = None


def test_set_palette():
    pokes = {0x1a466: PAL.to_bytes(4, "big")}
    diffs, info = differential(SET_PALETTE, {"_pokes": pokes},
                               lambda lib, buf: lib.g_set_palette(buf))
    assert not diffs, report(diffs)
    assert info["regs"]["events"] == [(harness.OS_EVENT_SETPALETTE, PAL)], (
        f"the oracle did not issue Setpalette({PAL:#x}): {info['regs']['events']}")


def test_set_palette_wrapper():
    """The named wrapper set_palette @ 0x135da just calls 0x16300; same ledger entry."""
    pokes = {0x1a466: (PAL + 0x20).to_bytes(4, "big")}
    diffs, info = differential(0x135da, {"_pokes": pokes},
                               lambda lib, buf: lib.g_set_palette(buf))
    assert not diffs, report(diffs)
    assert info["regs"]["events"] == [(harness.OS_EVENT_SETPALETTE, PAL + 0x20)]


def _pal_pokes():
    pokes = {0x1a466: PAL_BUF.to_bytes(4, "big"), 0x1acf4: bytes([3])}
    for i in range(100):
        pokes[PAL_TABLE + i] = bytes([(0x40 + i) & 0xff])
    for i in range(32):
        pokes[PAL_BUF + i] = bytes([0xaa])
    return pokes


def test_fun_162e2():
    """FUN_000162e2 copies the indexed 5-word palette entry to *(0x1a466)+2, Setpalette."""
    for idx in (0, 1, 3, 9):
        diffs, info = differential(FUN_162E2, {"d0": idx, "_pokes": _pal_pokes()},
                                   lambda lib, buf, i=idx: lib.g_fun_162e2(buf, i))
        assert not diffs, report(diffs)
        assert info["regs"]["events"] == [(harness.OS_EVENT_SETPALETTE, PAL_BUF)]


def test_fun_162da():
    """FUN_000162da reads the current palette index from 0x1acf4."""
    diffs, info = differential(FUN_162DA, {"_pokes": _pal_pokes()},
                               lambda lib, buf: lib.g_fun_162da(buf))
    assert not diffs, report(diffs)
    assert info["regs"]["events"] == [(harness.OS_EVENT_SETPALETTE, PAL_BUF)]


def test_fun_162ce():
    """FUN_000162ce seeds the index from A6[1] and branches into FUN_000162e2."""
    pokes = _pal_pokes()
    pokes[0x40000 + 1] = bytes([5])
    diffs, info = differential(FUN_162CE, {"a6": 0x40000, "_pokes": pokes},
                               lambda lib, buf: lib.g_fun_162ce(buf, 0x40000))
    assert not diffs, report(diffs)
    assert info["regs"]["events"] == [(harness.OS_EVENT_SETPALETTE, PAL_BUF)]


def _idx_pokes():
    pokes = {IDX_1A579: bytes([0x10]), DAT_1ACD7: bytes([0x00])}
    for i in range(300):
        pokes[TBL_2741E + i] = bytes([(i * 5 + 7) & 0xff])
    return pokes


def test_fun_16210():
    """FUN_00016210 bumps 0x1a579 and copies 0x2741e[new] into 0x1acd7."""
    for start in (0, 1, 0x7f, 0x80, 0xfe, 0xff):
        pokes = _idx_pokes()
        pokes[IDX_1A579] = bytes([start])
        diffs, _ = differential(FUN_16210, {"_pokes": pokes},
                                lambda lib, buf: lib.g_fun_16210(buf))
        assert not diffs, f"start={start:#x}\n{report(diffs)}"


def test_fun_16230():
    """FUN_00016230 bumps 0x1a579 and returns the big-endian word 0x2741e[new]."""
    for start in (0, 1, 0x7f, 0x80, 0xfe, 0xff):
        pokes = _idx_pokes()
        pokes[IDX_1A579] = bytes([start])
        diffs, info = differential(FUN_16230, {"_pokes": pokes},
                                   lambda lib, buf: lib.g_fun_16230(buf))
        assert not diffs, f"start={start:#x}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["d0"], f"start={start:#x}"


def _list_pokes():
    pokes = {
        DAT_1AB60: (0x00b4).to_bytes(2, "big"),
        DAT_1A526: (0x0000).to_bytes(2, "big"),
        DAT_1ACD2: bytes([0x10]),
    }
    for i in range(0x3f):
        pokes[LIST_1A8F0 + 6 * i] = bytes([((i * 7) & 0xc0) | 0x01])
        pokes[LIST_1A8F0 + 6 * i + 1] = bytes([i])
        for j in range(2, 6):
            pokes[LIST_1A8F0 + 6 * i + j] = bytes([(0xA0 + i + j) & 0xff])
    return pokes


def test_fun_1627a():
    """FUN_0001627a short-circuits below the 0xb4 byte delta and otherwise clears flags."""
    for hi, lo in ((0x0000, 0x0000),        # delta 0 -> below threshold, no writes
                   (0x00b3, 0x0000),        # 0xb3 -> below
                   (0x00b4, 0x0000),        # 0xb4 -> fires
                   (0x0010, 0x0050),        # wraps to 0xffc0 -> low byte 0xc0, fires
                   (0xffff, 0x0000)):
        pokes = _list_pokes()
        pokes[DAT_1AB60] = hi.to_bytes(2, "big")
        pokes[DAT_1A526] = lo.to_bytes(2, "big")
        diffs, _ = differential(FUN_1627A, {"_pokes": pokes},
                                lambda lib, buf: lib.g_fun_1627a(buf))
        assert not diffs, f"ab60={hi:#x} a526={lo:#x}\n{report(diffs)}"


FUN_135DA = 0x135DA


def test_fun_135da():
    diffs, info = differential(FUN_135DA, {"_pokes": _pal_pokes()},
                               lambda lib, buf: lib.g_fun_135da(buf))
    assert not diffs, report(diffs)
    assert info["regs"]["events"] == [(harness.OS_EVENT_SETPALETTE, PAL_BUF)]


FUN_135E2 = 0x135E2


def test_fun_135e2():
    pokes = {0x456: (0x40000).to_bytes(4, "big"), 0x4BD: bytes([0x42])}
    diffs, _ = differential(FUN_135E2, {"_pokes": pokes},
                            lambda lib, buf: lib.g_fun_135e2(buf))
    assert not diffs, report(diffs)
