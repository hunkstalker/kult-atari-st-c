"""Tests del blitter de planos de overlay (overlay.c): 0x111b6 y wrapper 0x111ac."""
import ctypes

import harness
from harness import differential, report

_u8p = ctypes.POINTER(ctypes.c_uint8)
VBL_COUNTER = 0x1acd4
VBL_SITE = 0x1359a

harness._lib.g_fun_111b6.argtypes = [_u8p] + [ctypes.c_uint32] * 6
harness._lib.g_fun_111b6.restype = None
harness._lib.g_fun_111ac.argtypes = [_u8p] + [ctypes.c_uint32] * 6
harness._lib.g_fun_111ac.restype = None

SRC = 0x40000
DEST = 0x90000


def _pokes(stride=0x40):
    p = {0x1a534: stride.to_bytes(4, "big"), VBL_COUNTER: bytes([0x00])}
    for i in range(0x10000):
        p[SRC + i] = bytes([(i * 7 + 3) & 0xFF])
        p[DEST + i] = bytes([(i * 3 + 1) & 0xFF])
    for i in range(0x1000):
        p[0xD8F0 + i] = bytes([(i * 11 + 5) & 0xFF])
    return p


def _vbl_k(n):
    return [{"pc": VBL_SITE, "nth": k, "addr": VBL_COUNTER, "width": 1, "value": k}
            for k in range(1, n + 1)]


def test_fun_111b6():
    for d0 in (0, 4, 8, 12, 15):
        for d2 in (5, 17, 33, 49):
            for d1 in (0, 1):
                for d3 in (0, 1):
                    pokes = _pokes()
                    diffs, _ = differential(
                        0x111B6, {"d0": d0, "d1": d1, "d2": d2, "d3": d3,
                                  "a0": DEST, "a1": SRC, "_pokes": pokes},
                        lambda lib, buf, a=d0, b=d1, c=d2, e=d3:
                        lib.g_fun_111b6(buf, a, b, c, e, DEST, SRC),
                        schedule=_vbl_k(d1 + 1), wait_sites=[VBL_SITE])
                    assert not diffs, (f"d0={d0} d1={d1} d2={d2} d3={d3}\n"
                                       f"{report(diffs)}")


def test_fun_111ac():
    for d0 in (0, 8, 15):
        for d1 in (1, 2):
            for d3 in (1, 2):
                pokes = _pokes()
                diffs, _ = differential(
                    0x111AC, {"d0": d0, "d1": d1, "d2": 5, "d3": d3,
                              "a0": DEST, "a1": SRC, "_pokes": pokes},
                    lambda lib, buf, a=d0, b=d1, e=d3:
                    lib.g_fun_111ac(buf, a, b, 5, e, DEST, SRC),
                    schedule=_vbl_k(((d1 << 2) - 1) + 1), wait_sites=[VBL_SITE])
                assert not diffs, f"d0={d0} d1={d1} d3={d3}\n{report(diffs)}"


harness._lib.g_fun_114ba.argtypes = [_u8p] + [ctypes.c_uint32] * 7
harness._lib.g_fun_114ba.restype = None
harness._lib.g_fun_114b0.argtypes = [_u8p] + [ctypes.c_uint32] * 7
harness._lib.g_fun_114b0.restype = None


def test_fun_114ba():
    for d0 in (0, 4, 8, 12, 15):
        for d2 in (5, 17, 33, 49):
            for d1 in (0, 1):
                for d3 in (0, 1):
                    pokes = _pokes()
                    diffs, _ = differential(
                        0x114BA, {"d0": d0, "d1": d1, "d2": d2, "d3": d3, "d6": 0,
                                  "a0": DEST, "a1": SRC, "_pokes": pokes},
                        lambda lib, buf, a=d0, b=d1, c=d2, e=d3:
                        lib.g_fun_114ba(buf, a, b, c, e, 0, DEST, SRC),
                        schedule=_vbl_k(d1 + 1), wait_sites=[VBL_SITE])
                    assert not diffs, (f"d0={d0} d1={d1} d2={d2} d3={d3}\n"
                                       f"{report(diffs)}")


def test_fun_114b0():
    for d0 in (0, 8, 15):
        for d1 in (1, 2):
            for d3 in (1, 2):
                pokes = _pokes()
                diffs, _ = differential(
                    0x114B0, {"d0": d0, "d1": d1, "d2": 5, "d3": d3, "d6": 0,
                              "a0": DEST, "a1": SRC, "_pokes": pokes},
                    lambda lib, buf, a=d0, b=d1, e=d3:
                    lib.g_fun_114b0(buf, a, b, 5, e, 0, DEST, SRC),
                    schedule=_vbl_k((d1 << 2)), wait_sites=[VBL_SITE])
                assert not diffs, f"d0={d0} d1={d1} d3={d3}\n{report(diffs)}"


harness._lib.g_fun_117fe.argtypes = [_u8p] + [ctypes.c_uint32] * 6
harness._lib.g_fun_117fe.restype = None
harness._lib.g_fun_117de.argtypes = [_u8p] + [ctypes.c_uint32] * 5
harness._lib.g_fun_117de.restype = None

SRC2 = 0x88000


def _pokes2(stride=0x40):
    p = {0x1a534: stride.to_bytes(4, "big"), VBL_COUNTER: bytes([0x00]),
         0x1A53E: (0).to_bytes(2, "big")}
    for i in range(0x10000):
        p[SRC + i] = bytes([(i * 7 + 3) & 0xFF])
        p[DEST + i] = bytes([(i * 3 + 1) & 0xFF])
        p[SRC2 + i] = bytes([(i * 5 + 9) & 0xFF])
    for i in range(0x1000):
        p[0xD8F0 + i] = bytes([(i * 11 + 5) & 0xFF])
    return p


def test_fun_117fe():
    for d0 in (0, 4, 8, 12, 15):
        for d2 in (5, 17, 33, 49):
            for d1 in (0, 1):
                for d3 in (0, 1):
                    for flag in (0, 1):
                        pokes = _pokes2()
                        pokes[0x1A53E] = flag.to_bytes(2, "big")
                        diffs, _ = differential(
                            0x117FE, {"d0": d0, "d1": d1, "d2": d2, "d3": d3,
                                      "a0": DEST, "a1": SRC2, "_pokes": pokes},
                            lambda lib, buf, a=d0, b=d1, c=d2, e=d3:
                            lib.g_fun_117fe(buf, a, b, c, e, DEST, SRC2),
                            schedule=_vbl_k(d1 + 1), wait_sites=[VBL_SITE])
                        assert not diffs, (f"d0={d0} d1={d1} d2={d2} d3={d3} "
                                           f"flag={flag}\n{report(diffs)}")


def test_fun_117de():
    for d0 in (0, 8, 15):
        for d1 in (1, 2):
            for d3 in (1, 2):
                pokes = _pokes2()
                diffs, _ = differential(
                    0x117DE, {"d0": d0, "d1": d1, "d2": 5, "d3": d3,
                              "a0": DEST, "_pokes": pokes},
                    lambda lib, buf, a=d0, b=d1, e=d3:
                    lib.g_fun_117de(buf, a, b, 5, e, DEST),
                    schedule=_vbl_k((d1 << 2)), wait_sites=[VBL_SITE])
                assert not diffs, f"d0={d0} d1={d1} d3={d3}\n{report(diffs)}"


harness._lib.g_fun_11afa.argtypes = [_u8p] + [ctypes.c_uint32] * 6
harness._lib.g_fun_11afa.restype = None
harness._lib.g_fun_11ada.argtypes = [_u8p] + [ctypes.c_uint32] * 5
harness._lib.g_fun_11ada.restype = None


def test_fun_11afa():
    for d0 in (0, 4, 8, 12, 15):
        for d2 in (5, 17, 33, 49):
            for d1 in (0, 1):
                for d3 in (0, 1):
                    for flag in (0, 1):
                        pokes = _pokes2()
                        pokes[0x1A53E] = flag.to_bytes(2, "big")
                        diffs, _ = differential(
                            0x11AFA, {"d0": d0, "d1": d1, "d2": d2, "d3": d3,
                                      "a0": DEST, "a1": SRC2, "_pokes": pokes},
                            lambda lib, buf, a=d0, b=d1, c=d2, e=d3, f=flag:
                            lib.g_fun_11afa(buf, a, b, c, e, DEST, SRC2),
                            schedule=_vbl_k(d1 + 1), wait_sites=[VBL_SITE])
                        assert not diffs, (f"d0={d0} d1={d1} d2={d2} d3={d3} "
                                           f"flag={flag}\n{report(diffs)}")


def test_fun_11ada():
    for d0 in (0, 8, 15):
        for d1 in (1, 2):
            for d3 in (1, 2):
                pokes = _pokes2()
                diffs, _ = differential(
                    0x11ADA, {"d0": d0, "d1": d1, "d2": 5, "d3": d3,
                              "a0": DEST, "_pokes": pokes},
                    lambda lib, buf, a=d0, b=d1, e=d3:
                    lib.g_fun_11ada(buf, a, b, 5, e, DEST),
                    schedule=_vbl_k((d1 << 2)), wait_sites=[VBL_SITE])
                assert not diffs, f"d0={d0} d1={d1} d3={d3}\n{report(diffs)}"
