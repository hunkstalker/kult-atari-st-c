"""Test diferencial del cuerpo de la caja de texto.

FUN_00010e66 @ 0x10e66: dibuja el marco, coloca el cursor y, línea a línea, imprime
';' + la línea (0x10b24) + '<' hasta que print_word marca el final de la cadena
($1a508 = 0xff); luego pinta el borde inferior.
"""
import ctypes

import harness
from harness import differential, report

FUN_10E66 = 0x10E66
STRBUF = 0x40000
SCREEN = 0x80000

_u8p = ctypes.POINTER(ctypes.c_uint8)
harness._lib.g_fun_10e66.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_fun_10e66.restype = None


def _pokes(bank):
    pokes = {
        0x1a45e: SCREEN.to_bytes(4, "big"),
        0x1a46a: (0x8000).to_bytes(4, "big"),
        0x1a4dc: (STRBUF + len(bank)).to_bytes(4, "big"),
        0x1a512: (8).to_bytes(2, "big"),
        0x1a530: (1).to_bytes(2, "big"),
        0x1a532: (1).to_bytes(2, "big"),
    }
    pokes[STRBUF] = bank
    pokes[SCREEN] = bytes((0x5A + i * 3) & 0xFF for i in range(0x4000))
    return pokes


def test_fun_10e66():
    bank = bytes([0x21, 0x22 | 0x40, 0x23, 0x24 | 0x40, 0x25, 0x26 | 0x40,
                  0x27, 0x28 | 0x40, 0x29, 0x2A])
    diffs, _ = differential(FUN_10E66, {"a5": STRBUF, "_pokes": _pokes(bank)},
                            lambda lib, buf: lib.g_fun_10e66(buf, STRBUF))
    assert not diffs, report(diffs)


FUN_10440 = 0x10440
TABLE_3EC8A = 0x3EC8A
harness._lib.g_fun_10440.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_fun_10440.restype = None


def _cmd_pokes(cmd):
    from harness import console_key
    bank = bytes([3, 0x21, 0x22, 3, 0x23, 0x24, 3, 0x25, 0x26,
                  3, 0x27, 0x28, 3, 0x29, 0x2A, 3, 0x2B, 0x2C])
    pokes = {
        0x1a45e: SCREEN.to_bytes(4, "big"),
        0x1a46a: (0x8000).to_bytes(4, "big"),
        TABLE_3EC8A: bank,
        0x1a450: (0x30).to_bytes(2, "big"),     # cursor x
        0x1a452: (0x40).to_bytes(2, "big"),     # cursor y
        0x1acc6: bytes([0x87]),                  # sin overlay -> prompt_wait por Cnecin
        0x1a44f: bytes([0x00]),
        0x1a456: bytes([0x00]),
        0x1acd4: bytes([0x05]),
        0x1a74e: bytes([0x00]),
        SCREEN: bytes((0x5A + i * 3) & 0xFF for i in range(0x4000)),
    }
    pokes.update(console_key("a"))
    return pokes


def test_fun_10440():
    diffs, _ = differential(FUN_10440, {"d1": 0x9001, "_pokes": _cmd_pokes(0x9001)},
                            lambda lib, buf: lib.g_fun_10440(buf, 0x9001))
    assert not diffs, report(diffs)


FUN_1042C = 0x1042C
harness._lib.g_fun_1042c.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_fun_1042c.restype = ctypes.c_uint32


def test_fun_1042c():
    for idx in (6, 4, 5):
        pokes = _cmd_pokes(0)
        pokes[0x50000] = bytes([0x00, idx])
        diffs, info = differential(
            FUN_1042C, {"a6": 0x50000, "_pokes": pokes},
            lambda lib, buf, n=idx: lib.g_fun_1042c(buf, 0x50000))
        assert not diffs, f"idx={idx}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], f"idx={idx} a6"


FUN_10DFA = 0x10DFA
harness._lib.g_fun_10dfa.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_fun_10dfa.restype = ctypes.c_uint32


def test_fun_10dfa():
    """0x10dfa: caja de texto con [idx][x][y][ancho] inline + entrada de icono (0x120a8)."""
    base = _cmd_pokes(0)
    for d4 in (4, 5, 6):
        pokes = dict(base)
        pokes[0x50000] = bytes([92, d4, 3, 2, 8])
        diffs, info = differential(
            FUN_10DFA, {"a6": 0x50000, "_pokes": pokes},
            lambda lib, buf: lib.g_fun_10dfa(buf, 0x50000))
        assert not diffs, f"d4={d4}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], "a6"


FUN_10B52 = 0x10B52
harness._lib.g_fun_10b52.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_fun_10b52.restype = ctypes.c_uint32


def test_fun_10b52():
    """0x10b52/0x10b6c: ventana de texto (marco + cadena + icono)."""
    bank = bytes([3, 0x21, 0x22, 3, 0x23, 0x24, 3, 0x25, 0x26,
                  3, 0x27, 0x28, 3, 0x29, 0x2A, 3, 0x2B, 0x2C])
    base = _cmd_pokes(0)
    for i, b in enumerate(bank):
        base[0x3C534 + i] = bytes([b])
    for d4 in (0, 4, 5, 6):
        for flags in (0x00, 0x20, 0x80, 0xA0, 0xC0, 0xE0):
            pokes = dict(base)
            # a6 apunta a 0x50002 (d4<4, 10fb4 consume un byte) o 0x50001 (d4>=4):
            # se fija y = 0x10 (>= 7; con y < 7 el borde hace (y-7) y genera una
            # direccion fuera de la imagen que el oraculo acota a 0 pero el candidato
            # leeria del heap) y flags controla el borde 0xe0 en ambos layouts.
            if d4 < 4:
                pokes[0x50000] = bytes([0, d4, 0x00, 2, 0x10, flags])
            else:
                pokes[0x50000] = bytes([0, d4, 3, 0x10, flags])
            diffs, info = differential(
                FUN_10B52, {"a6": 0x50000, "_pokes": pokes},
                lambda lib, buf: lib.g_fun_10b52(buf, 0x50000))
            assert not diffs, f"d4={d4} flags={flags:#x}\n{report(diffs)}"
            assert info["ret"] == info["regs"]["a6"], "a6"


FUN_13F86 = 0x13F86
harness._lib.g_fun_13f86.argtypes = [_u8p]
harness._lib.g_fun_13f86.restype = None


def test_fun_13f86():
    for chain, blocks in ((7, ((0, 0x10, 3), (0, 0x11, 5))),
                          (10, ((0, 0x10, 3), (0, 0x11, 5), (0, 0x12, 7))),
                          # bit7 en x/y: D6=A6[1]&0x7f y D5=(A6[2]<<1)&0xff
                          # (si no, la caja se ensancha y y0 no coge el minimo)
                          (7, ((0, 0x10, 0x3C), (0, 0x98, 0x96)))):
        pokes = {
            0x1a45e: (0x90000).to_bytes(4, "big"),
            0x1a462: (0x88000).to_bytes(4, "big"),
            0x1a573: bytes([0]),
            0x1acf1: bytes([5]),
            0x1acc7: bytes([1]),
            0x1acc9: bytes([0]),
            0x1aced: bytes([0]),
            0x1acc6: bytes([0x87]),
            0x24982: bytes([chain]),
            0x41fb0: (0x12).to_bytes(2, "big"),
            0x41fb2: bytes([2, 4]),
        }
        pos = 0x24983
        for b in blocks:
            pokes[pos] = bytes(b)
            pos += 3
        for i in range(0x20):
            pokes[0x41fb4 + i] = bytes([(0x10 + i) & 0xFF])
        for i in range(0x4000):
            pokes[0x88000 + i] = bytes([0xCC])
            pokes[0x90000 + i] = bytes([0x55])
        diffs, _ = differential(FUN_13F86, {"_pokes": pokes},
                                lambda lib, buf: lib.g_fun_13f86(buf))
        assert not diffs, f"chain={chain}\n{report(diffs)}"


FUN_17930 = 0x17930
harness._lib.g_fun_17930.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_fun_17930.restype = None


def test_fun_17930():
    """Entrada 0x19978 + 0x10dfa + relleno 0x11020 (a1 por 0x1a75e o por objeto)."""
    for sel in (0x00, 0x01):
        pokes = _cmd_pokes(0)
        pokes[0x50000] = bytes([0, sel])
        pokes[0x19979] = bytes([4])                 # indice leido por 0x10dfa
        pokes[0x1A858] = (0x10).to_bytes(2, "big")
        pokes[0x1A85A] = (0x20).to_bytes(2, "big")
        pokes[0x1A75E] = (0x1A8FC).to_bytes(4, "big")
        pokes[0x1A812] = (0x50008).to_bytes(4, "big")
        pokes[0x5000C] = bytes([3])                 # obj[4] -> d3 = (3-1)*6 = 12
        pokes[0x1A8FC + 2] = bytes([0])             # d4 del relleno 0x11020
        pokes[0x1A8FC + 3] = bytes([5])             # ancho + 0x12
        pokes[0x28AD2] = bytes([0x00, 0x04, 0x01, 0x01])
        for i in range(0x40):
            pokes[0x28AD6 + i] = bytes([(i * 3 + 1) & 0xFF])
        diffs, _ = differential(FUN_17930, {"a6": 0x50000, "_pokes": pokes},
                                lambda lib, buf: lib.g_fun_17930(buf, 0x50000))
        assert not diffs, f"sel={sel}\n{report(diffs)}"
