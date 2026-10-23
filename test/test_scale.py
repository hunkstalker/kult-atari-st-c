"""Tests diferenciales de los escaladores de elemento.

FUN_00013a18 @ 0x13a18: escala el bitmap 0x1a528 en el descriptor 0xd8f0 (filas
empaquetadas) con el acumulador 16.16 y lo blitea a A2 con 0x123a0.
FUN_00013bc2 @ 0x13bc2: el mismo bucle escribiendo en A1 con filas a 0xa0.
"""
import ctypes

import harness
from harness import differential, report

FUN_13A18 = 0x13A18
FUN_13BC2 = 0x13BC2
SRC = 0x40000
DEST = 0x90000

_u8p = ctypes.POINTER(ctypes.c_uint8)
harness._lib.scale_16_16.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32, ctypes.c_uint32]
harness._lib.scale_16_16.restype = None
harness._lib.scale_blit.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32, ctypes.c_uint32]
harness._lib.scale_blit.restype = None


def _scale_pokes():
    pokes = {
        0x1a528: SRC.to_bytes(4, "big"),
        0x1a52e: (4).to_bytes(2, "big"),
        0x1a542: (39).to_bytes(2, "big"),
        0x1a544: (15).to_bytes(2, "big"),
        0x1a486: (8).to_bytes(2, "big"),
        0x1a45e: DEST.to_bytes(4, "big"),
    }
    for i in range(0x200):
        pokes[SRC + i] = bytes([(i * 7 + 3) & 0xFF])
    for i in range(0x2000):
        pokes[DEST + i] = bytes([(i * 11 + 7) & 0xFF])
    return pokes


def test_fun_13a18():
    for d2, d3, d5 in ((3, 2, 8), (2, 3, 4), (1, 1, 0), (3, 1, 12)):
        pokes = _scale_pokes()
        pokes[0x1a486] = d5.to_bytes(2, "big")
        diffs, _ = differential(
            FUN_13A18, {"d2": d2, "d3": d3, "d7": 0, "a2": DEST, "_pokes": pokes},
            lambda lib, buf, a=d2, b=d3: lib.scale_16_16(buf, a, b, 0, DEST))
        assert not diffs, f"d2={d2} d3={d3} d5={d5}\n{report(diffs)}"


def test_fun_13bc2():
    for d2, d3, d5 in ((3, 2, 8), (2, 3, 4), (1, 1, 0), (3, 1, 12)):
        pokes = _scale_pokes()
        pokes[0x1a486] = d5.to_bytes(2, "big")
        for i in range(0x400):
            pokes[DEST + 0x1000 + i] = bytes([0xCC])
        diffs, _ = differential(
            FUN_13BC2, {"d2": d2, "d3": d3, "d7": 0, "a1": DEST + 0x1000,
                        "_pokes": pokes},
            lambda lib, buf, a=d2, b=d3: lib.scale_blit(buf, a, b, 0, DEST + 0x1000))
        assert not diffs, f"d2={d2} d3={d3} d5={d5}\n{report(diffs)}"


FUN_13702 = 0x13702
harness._lib.scale_prep.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.scale_prep.restype = ctypes.c_uint32


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
        0x1a570: bytes([0]),
    }
    for i in range(0x100):
        pokes[0x31076 + i] = bytes([(i * 7 + 3) & 0xFF])
    for i in range(0x2000):
        pokes[0x3b1ae + i] = bytes([(i * 5 + 1) & 0xFF])
        pokes[0xD8F0 + i] = bytes([(i * 11 + 5) & 0xFF])
    for i in range(10 * 0x10):
        pokes[0x1a850 + i] = bytes([0])
    pokes.update(_scale_screen())
    return pokes


def _scale_screen():
    p = {}
    for i in range(0x8000):
        p[DEST + i] = bytes([(i * 11 + 7) & 0xFF])
    return p


def test_fun_13702():
    for r, c in ((6, 3), (4, 2), (8, 6)):
        pokes = _elem_pokes(16, 3)
        pokes[0x50000] = bytes([0x00, 1, 8, 4, r, c])
        diffs, info = differential(
            FUN_13702, {"a6": 0x50000, "_pokes": pokes},
            lambda lib, buf: lib.scale_prep(buf, 0x50000))
        assert not diffs, f"r={r} c={c}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], "a6"


def _abort_pokes(kind):
    pokes = _elem_pokes(16, 3)
    pokes[0x50000] = bytes([0x00, 0xFF])
    pokes[0x1ad0a] = bytes([1 if kind == "flag" else 0])
    if kind == "clase":
        pokes[0x1a81e] = (0x60000).to_bytes(4, "big")
        pokes[0x60000] = (0).to_bytes(4, "big")
        pokes[0x1a772] = (0x40000).to_bytes(4, "big")
        pokes[0x40003] = bytes([0x37])
        for i in range(24):
            base = 0x19e19 + i * 6
            pokes[base] = bytes([0x80])
            for k in range(1, 6):
                pokes[base + k] = bytes([(i + k) & 0xFF])
    return pokes


def _run_abort(entry, fn):
    for kind in ("flag", "clase"):
        pokes = _abort_pokes(kind)
        diffs, info = differential(
            entry, {"a6": 0x50000, "_pokes": pokes},
            lambda lib, buf, f=fn: f(buf, 0x50000),
            max_insns=20_000_000)
        assert not diffs, f"{entry:#x} {kind}\n{report(diffs)}"
        assert info["ret"] == 0x50002, f"{entry:#x} {kind}: {info['ret']:#x}"
        assert info["regs"]["a6"] == 0x50002, f"{entry:#x} {kind} a6"


def test_fun_13702_abort():
    _run_abort(FUN_13702, harness._lib.scale_prep)


FUN_1398E = 0x1398E
harness._lib.scale_list_build.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32, ctypes.c_uint32]
harness._lib.scale_list_build.restype = None


def test_fun_1398e():
    for d2, d3, d7, a0 in ((5, 2, 0, DEST), (4, 1, 4, DEST + 8),
                           (6, 3, 8, DEST + 3), (3, 2, 12, DEST + 0x100)):
        pokes = _scale_screen()
        pokes[0x1a45e] = DEST.to_bytes(4, "big")
        for i in range(0x40):
            pokes[0x1a1f2 + i] = bytes([0 if i % 5 == 0 else (i * 3 + 1) & 0xFF])
        for i in range(0x1000):
            pokes[0xD8F0 + i] = bytes([0xEE])
        diffs, _ = differential(
            FUN_1398E, {"d2": d2, "d3": d3, "d7": d7, "a0": a0, "_pokes": pokes},
            lambda lib, buf, a=d2, b=d3, c=d7, d=a0:
            lib.scale_list_build(buf, a, b, c, d))
        assert not diffs, f"d2={d2} d3={d3} d7={d7} a0={a0:#x}\n{report(diffs)}"


FUN_138A6 = 0x138A6
harness._lib.scale_anim.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.scale_anim.restype = ctypes.c_uint32


def test_fun_138a6():
    for skip in (0, 1):
        pokes = _elem_pokes(16, 3)
        pokes[0x1a570] = bytes([skip])
        pokes[0x50000] = bytes([0x00, 1])
        entry = (bytes([0x00, 0x02]) + (0x7700).to_bytes(4, "big")
                 + (0).to_bytes(2, "big") + (8).to_bytes(2, "big")
                 + (4).to_bytes(2, "big") + (6).to_bytes(2, "big")
                 + (4).to_bytes(2, "big"))
        for i, b in enumerate(entry):
            pokes[0x1a850 + i] = bytes([b])
        for i in range(0x40):
            pokes[0x1a1f2 + i] = bytes([4])
        sched = ([] if skip else
                 [{"pc": 0x1359a, "nth": k, "addr": 0x1acd4, "width": 1,
                   "value": k} for k in (1, 2)])
        pokes[0x1acd4] = bytes([0])
        diffs, info = differential(
            FUN_138A6, {"a6": 0x50000, "_pokes": pokes},
            lambda lib, buf: lib.scale_anim(buf, 0x50000),
            schedule=sched, wait_sites=[0x1359a])
        assert not diffs, f"skip={skip}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], "a6"


FUN_13740 = 0x13740
harness._lib.scale_loop.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.scale_loop.restype = ctypes.c_uint32


def test_fun_13740():
    for skip in (0, 1):
        for w, h in ((4, 3), (4, 6), (8, 3)):
            pokes = _elem_pokes(w, h)
            pokes[0x1a570] = bytes([skip])
            pokes[0x50000] = bytes([0x00, 1, 8, 4])
            sched = [{"pc": 0x1359a, "nth": 1, "addr": 0x1acd4, "width": 1, "value": 1}]
            pokes[0x1acd4] = bytes([0])
            diffs, info = differential(
                FUN_13740, {"a6": 0x50000, "_pokes": pokes},
                lambda lib, buf: lib.scale_loop(buf, 0x50000),
                schedule=sched, wait_sites=[0x1359a])
            assert not diffs, f"skip={skip} w={w} h={h}\n{report(diffs)}"
            assert info["ret"] == info["regs"]["a6"], "a6"


def test_fun_13740_abort():
    _run_abort(FUN_13740, harness._lib.scale_loop)
