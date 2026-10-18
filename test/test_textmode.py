"""Tests diferenciales del modo texto: relleno, position_text variante y overlays.

FUN_00010b46 @ 0x10b46: imprime D6+1 espacios (cola compartida).
FUN_00010fb4 @ 0x10fb4: position_text; si D4 < 4 toma el índice de A6[1] y avanza A6.
FUN_00010aee @ 0x10aee: centra la línea y cae en el cuerpo de 0x10b24.
FUN_000109fe @ 0x109fe / FUN_00010a88 @ 0x10a88: overlays A (0x41b76) y B (0x41586).
"""
import ctypes

import harness
from harness import differential, report

FUN_10B46 = 0x10B46
FUN_10FB4 = 0x10FB4
FUN_10AEE = 0x10AEE
FUN_109FE = 0x109FE
FUN_10A88 = 0x10A88

TABLE = 0x42000
SCRIPT = 0x42100
SCREEN = 0x80000
STRBUF = 0x40000

_u8p = ctypes.POINTER(ctypes.c_uint8)
_u32p = ctypes.POINTER(ctypes.c_uint32)
harness._lib.print_spaces.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.print_spaces.restype = None
harness._lib.text_index_arg.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32, _u32p]
harness._lib.text_index_arg.restype = ctypes.c_uint32
harness._lib.print_line_center.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.print_line_center.restype = ctypes.c_uint32
for _name in ("overlay_a_draw", "overlay_b_draw"):
    getattr(harness._lib, _name).argtypes = [_u8p, ctypes.c_uint32]
    getattr(harness._lib, _name).restype = None

# Tabla de entradas byte-length ([u8 len][datos], len incluye el byte de longitud).
_CHAIN = {0: 4, 4: 6, 10: 3, 13: 5, 18: 2, 20: 7, 27: 1, 28: 9}


def _char_pokes(x, y):
    return {
        0x1a45e: SCREEN.to_bytes(4, "big"),
        0x1a46a: (0x8000).to_bytes(4, "big"),
        0x1a50a: x.to_bytes(2, "big"),
        0x1a50c: y.to_bytes(2, "big"),
        0x1a4d8: (0x1d382).to_bytes(4, "big"),
        SCREEN: bytes((0x5A + i * 3) & 0xFF for i in range(0x4000)),
    }


def test_fun_10b46():
    for d6 in (0, 1, 4, 9):
        diffs, _ = differential(FUN_10B46, {"d6": d6, "_pokes": _char_pokes(0, 0)},
                                lambda lib, buf, n=d6: lib.print_spaces(buf, n))
        assert not diffs, f"d6={d6}\n{report(diffs)}"


def _chain_pokes():
    pokes = {TABLE + p: bytes([v]) for p, v in _CHAIN.items()}
    pokes[SCRIPT] = bytes([0, 0])
    return pokes


def test_fun_10fb4():
    for d4 in (4, 5, 6, 0):
        a6 = ctypes.c_uint32(SCRIPT)
        diffs, info = differential(
            FUN_10FB4, {"a5": TABLE, "d4": d4, "a6": SCRIPT, "_pokes": _chain_pokes()},
            lambda lib, buf, d=d4, p=a6: lib.text_index_arg(buf, TABLE, d, ctypes.byref(p)))
        assert not diffs, f"d4={d4}\n{report(diffs)}"
        assert a6.value == info["regs"]["a6"], f"d4={d4}"


def _line_pokes(bank, width):
    pokes = _char_pokes(0, 0)
    pokes[0x1a4dc] = (STRBUF + len(bank)).to_bytes(4, "big")
    pokes[0x1a512] = width.to_bytes(2, "big")
    pokes[STRBUF] = bank
    return pokes


def test_fun_10aee():
    bank = bytes([0x21, 0x22 | 0x40, 0x23, 0x24 | 0x40, 0x25, 0x26 | 0x40,
                  0x27, 0x28 | 0x40, 0x29, 0x2A])
    for width in (10, 20, 40):
        diffs, info = differential(FUN_10AEE, {"a5": STRBUF, "_pokes": _line_pokes(bank, width)},
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
    pokes[0x1a512] = (8).to_bytes(2, "big")
    entry = bytes([3, 0x21, 0x22, 3, 0x23, 0x24, 3, 0x25, 0x26])  # 3 entradas
    pokes[0x41b76] = entry
    pokes[0x41586] = entry
    return pokes


def test_fun_109fe():
    for sel in (4, 5, 6):
        diffs, _ = differential(FUN_109FE, {"a6": SCRIPT, "_pokes": _overlay_pokes(sel, 0x10, 0x18)},
                                lambda lib, buf: lib.overlay_a_draw(buf, SCRIPT))
        assert not diffs, f"sel={sel}\n{report(diffs)}"


def test_fun_109fe_mode87():
    pokes = _overlay_pokes(4, 0x10, 0x18)
    pokes[0x1acc6] = bytes([0x87])
    diffs, _ = differential(FUN_109FE, {"a6": SCRIPT, "_pokes": pokes},
                            lambda lib, buf: lib.overlay_a_draw(buf, SCRIPT))
    assert not diffs, report(diffs)


def test_fun_10a88():
    for sel in (4, 5, 6):
        diffs, _ = differential(FUN_10A88, {"a6": SCRIPT, "_pokes": _overlay_pokes(sel, 0x10, 0x18)},
                                lambda lib, buf: lib.overlay_b_draw(buf, SCRIPT))
        assert not diffs, f"sel={sel}\n{report(diffs)}"


FUN_10A54 = 0x10A54
FUN_10ACC = 0x10ACC
DEST = 0x90000
SRC = DEST - 0x8000

harness._lib.overlay_a_frame.argtypes = [_u8p]
harness._lib.overlay_a_frame.restype = None
harness._lib.overlay_b_frame.argtypes = [_u8p]
harness._lib.overlay_b_frame.restype = None


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


def test_fun_10a54():
    for width in (1, 8, 16, 40):
        diffs, _ = differential(FUN_10A54, {"_pokes": _frame_pokes(0x11, 0x20, width)},
                                lambda lib, buf: lib.overlay_a_frame(buf))
        assert not diffs, f"width={width}\n{report(diffs)}"


def test_fun_10acc():
    diffs, _ = differential(FUN_10ACC, {"_pokes": _frame_pokes(0x11, 0x20, 4)},
                            lambda lib, buf: lib.overlay_b_frame(buf))
    assert not diffs, report(diffs)
