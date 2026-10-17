"""Tests de los iconos de pista (icons.c): 0x16556, 0x16514/0x1652e, 0x164d4, 0x164f6."""
import ctypes

import harness
from harness import differential, report

TABLE = 0x1a776
DESC_BLIT = 0x38000
DESC_REST = 0x3A000
SCREEN = 0x90000
BACK = 0x88000

_u8p = ctypes.POINTER(ctypes.c_uint8)
harness._lib.g_fun_16556.argtypes = [_u8p]
harness._lib.g_fun_16556.restype = None
harness._lib.g_fun_16514.argtypes = [_u8p]
harness._lib.g_fun_16514.restype = None
harness._lib.g_fun_164d4.argtypes = [_u8p]
harness._lib.g_fun_164d4.restype = None
harness._lib.g_fun_164f6.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_164f6.restype = None


def _blit_desc(dest):
    e = bytearray([2, 3])                       # outer=2, inner=3
    e += dest.to_bytes(4, "big")                # dest
    e += (4).to_bytes(2, "big")                 # flags
    e += (4).to_bytes(4, "big")                 # stride
    e += bytes(range(0x20, 0x20 + 24))
    return bytes(e)


def _rest_desc():
    e = bytearray([2, 3])                       # cols, rows
    e += (SCREEN - 0x8000).to_bytes(4, "big")   # dest (se le suma 0x8000)
    e += (8).to_bytes(2, "big")                 # phase
    e += (4).to_bytes(4, "big")                 # stride
    return bytes(e)


def _table_common():
    pokes = {
        0x1a45e: SCREEN.to_bytes(4, "big"),
        0x1a462: BACK.to_bytes(4, "big"),
        0x0d8ea: bytes([0x41]),
        0x1d402: bytes([0x5A]),
        DESC_BLIT: _blit_desc(SCREEN),
        DESC_REST: _rest_desc(),
    }
    for i in range(16):
        pokes[TABLE + i * 4] = DESC_BLIT.to_bytes(4, "big")
    for i in range(0x10000):
        pokes[SCREEN + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[BACK + i] = bytes([(i * 11 + 7) & 0xFF])
    return pokes


def test_fun_16556():
    diffs, _ = differential(0x16556, {"_pokes": _table_common()},
                            lambda lib, buf: lib.g_fun_16556(buf))
    assert not diffs, report(diffs)


def test_fun_16514():
    pokes = _table_common()
    for i in range(16):
        pokes[TABLE + i * 4] = DESC_REST.to_bytes(4, "big")
    diffs, _ = differential(0x16514, {"_pokes": pokes},
                            lambda lib, buf: lib.g_fun_16514(buf))
    assert not diffs, report(diffs)


def test_fun_164d4():
    for mode in (0x29, 0x16, 0x18, 0x15, 0x19, 0x00):
        pokes = _table_common()
        pokes[0x1acc7] = bytes([mode])
        diffs, _ = differential(0x164D4, {"_pokes": pokes},
                                lambda lib, buf: lib.g_fun_164d4(buf))
        assert not diffs, f"mode={mode:#x}\n{report(diffs)}"


def test_fun_164f6():
    """Al llamar a 0x13f56 hay que montar ARPLA + la tabla de texto como en test_vm."""
    TEXT_TABLE = 0x41FB0
    for mode in (0x15, 0x16, 0x18, 0x19):
        pokes = _table_common()
        pokes[0x1acc7] = bytes([mode])
        for i in range(0x20):                 # 32 entradas de 4 bytes [4,0,1,0]
            pokes[0x24982 + i * 4] = bytes([4, 0, 1, 0])
        pokes[TEXT_TABLE] = (2).to_bytes(2, "big")
        pokes[TEXT_TABLE + 2] = bytes([2])
        pokes[TEXT_TABLE + 3] = bytes([2])
        for i in range(0x40):
            pokes[TEXT_TABLE + 4 + i] = bytes([(0x10 + i) & 0xFF])
        diffs, _ = differential(0x164F6, {"a6": 0x30000, "d0": 1, "_pokes": pokes},
                                lambda lib, buf: lib.g_fun_164f6(buf, 0x30000, 1))
        assert not diffs, f"mode={mode:#x}\n{report(diffs)}"


FUN_18CE0 = 0x18CE0
FUN_18C8E = 0x18C8E
FUN_18CB2 = 0x18CB2
FUN_18316 = 0x18316
FUN_182F0 = 0x182F0
harness._lib.g_fun_18ce0.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_18ce0.restype = None
for _n in ("g_fun_18c8e", "g_fun_18cb2"):
    getattr(harness._lib, _n).argtypes = [_u8p, ctypes.POINTER(ctypes.c_uint32)]
    getattr(harness._lib, _n).restype = None
harness._lib.g_fun_18316.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_fun_18316.restype = None
harness._lib.g_fun_182f0.argtypes = [_u8p]
harness._lib.g_fun_182f0.restype = None


def test_fun_18ce0():
    for d0 in (0, 1, 5, 0xd):
        pokes = {0x1acd2: bytes([0x33])}
        for i in range(0x100):
            pokes[0x1a95c + i] = bytes([0x80 if i % 6 == 0 else (i * 3) & 0xFF])
        diffs, _ = differential(FUN_18CE0, {"a0": 0x1a95c, "d0": d0, "_pokes": pokes},
                                lambda lib, buf, n=d0: lib.g_fun_18ce0(buf, 0x1a95c, n))
        assert not diffs, f"d0={d0}\n{report(diffs)}"


def test_fun_18c8e_18cb2():
    for entry, fn in ((FUN_18C8E, harness._lib.g_fun_18c8e),
                      (FUN_18CB2, harness._lib.g_fun_18cb2)):
        pokes = {0x1acd2: bytes([0x44]), 0x1acf7: bytes([0x55])}
        for base in (0x1a8f0, 0x1a95c, 0x1a9d4, 0x1aa3a):
            for i in range(0x100):
                pokes[base + i] = bytes([0x80 if i % 6 == 0 else (i * 5) & 0xFF])
        a6 = ctypes.c_uint32(0x30000)
        diffs, info = differential(entry, {"a6": 0x30000, "_pokes": pokes},
                                   lambda lib, buf, p=a6: fn(buf, ctypes.byref(p)))
        assert not diffs, f"{entry:#x}\n{report(diffs)}"
        assert a6.value == info["regs"]["a6"], f"{entry:#x} a6"


def test_fun_182f0_18316():
    pokes = {0x1a579: bytes([0x10])}
    for i in range(300):
        pokes[0x2741e + i] = bytes([(i * 5 + 7) & 0xFF])
    diffs, _ = differential(FUN_182F0, {"_pokes": pokes},
                            lambda lib, buf: lib.g_fun_182f0(buf))
    assert not diffs, report(diffs)


FUN_18D28 = 0x18D28
harness._lib.g_fun_18d28.argtypes = [_u8p]
harness._lib.g_fun_18d28.restype = None


def test_fun_18d28():
    """Marco del objeto seleccionado: convierte el destino a coords y dibuja 2 segmentos H
    y 2 V, y cae en 0x1652e."""
    CELL = 0x30000
    DESC = 0x30010
    for w, h in ((4, 4), (2, 3), (8, 2)):
        for off in (0x160, 0x20, 0x1a0):
            for phase in (0, 8):
                dest = BACK + off
                desc = (bytes([w, h]) + dest.to_bytes(4, "big") +
                        phase.to_bytes(2, "big") + (4).to_bytes(4, "big"))
                pokes = {
                    0x1a45e: SCREEN.to_bytes(4, "big"),
                    0x1a462: BACK.to_bytes(4, "big"),
                    0x1a81a: (0x12345678).to_bytes(4, "big"),
                    0x1a81e: CELL.to_bytes(4, "big"),
                    0x1acc9: bytes([0x40]),
                    CELL: DESC.to_bytes(4, "big"),
                    DESC: desc,
                }
                for i in range(0x10000):
                    pokes[SCREEN + i] = bytes([(i * 3 + 1) & 0xFF])
                    pokes[BACK + i] = bytes([(i * 11 + 7) & 0xFF])
                diffs, _ = differential(FUN_18D28, {"_pokes": pokes},
                                        lambda lib, buf: lib.g_fun_18d28(buf))
                assert not diffs, (f"w={w} h={h} off={off:#x} phase={phase}\n"
                                   f"{report(diffs)}")


FUN_164A4 = 0x164A4
FUN_16460 = 0x16460
harness._lib.g_fun_164a4.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32]
harness._lib.g_fun_164a4.restype = ctypes.c_uint32
harness._lib.g_fun_16460.argtypes = [_u8p]
harness._lib.g_fun_16460.restype = None


def _obj(addr, x0, x1, y0, y1, flags):
    return bytes([x0, x1, y0, y1, flags, 0, 0, 0])


def test_fun_164a4():
    """El save desde A1 con descriptor en 0x3a556; devuelve el nuevo A1."""
    obj = 0x40000
    pokes = {0x1a45e: SCREEN.to_bytes(4, "big"), obj: _obj(obj, 10, 13, 8, 10, 0x40)}
    for i in range(0x1000):
        pokes[BACK + i] = bytes([(i * 5 + 3) & 0xFF])
    diffs, info = differential(
        FUN_164A4, {"a1": 0x3A556, "a2": TABLE, "a5": obj, "_pokes": pokes},
        lambda lib, buf: lib.g_fun_164a4(buf, 0x3A556, TABLE, obj))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a1"]


def test_fun_16460():
    """Reconstruye los 16 slots: solo los objetos con bit6 pasan por 0x164a4."""
    begin = 0x40000
    pokes = {
        0x1a45e: SCREEN.to_bytes(4, "big"),
        0x1a7ca: begin.to_bytes(4, "big"),
        0x1a7ce: (begin + 0x18).to_bytes(4, "big"),
        begin: _obj(begin, 10, 13, 8, 10, 0x40),
        begin + 8: _obj(begin + 8, 20, 23, 12, 14, 0x00),
        begin + 16: _obj(begin + 16, 30, 33, 16, 18, 0x40),
    }
    for i in range(16):
        pokes[TABLE + i * 4] = (0xAABBCCDD).to_bytes(4, "big")
    for i in range(0x2000):
        pokes[BACK + i] = bytes([(i * 7 + 1) & 0xFF])
    diffs, _ = differential(FUN_16460, {"_pokes": pokes},
                            lambda lib, buf: lib.g_fun_16460(buf))
    assert not diffs, report(diffs)
