"""Tests diferenciales de la seleccion de samples (0x15492/0x15510).

0x15492 recorre la tabla de 0x3f entradas de 6 B en 0x1a8f0, selecciona las que
cumplen `(word & D2) == D3`, copia el registro a 0x1a6ae+4/6/8 y lo dibuja con
0x15510; la primera coincidencia abre la ventana y arranca el sample.
"""
import ctypes

import harness
from harness import differential, report

_u8p = ctypes.POINTER(ctypes.c_uint8)
_u32p = ctypes.POINTER(ctypes.c_uint32)

FUN_15492 = 0x15492
FUN_15510 = 0x15510
TABLE = 0x1A8F0
RECORD = 0x1A6AE
VBL_SITE = 0x1359A
VBL_COUNTER = 0x1ACD4

harness._lib.g_fun_15492.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_15492.restype = ctypes.c_uint32
harness._lib.g_fun_15510.argtypes = [_u8p, _u32p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_15510.restype = ctypes.c_uint32


def _table_pokes(matches):
    pokes = {
        0x1A45E: (0x80000).to_bytes(4, "big"),
        0x1A488: (0x0001).to_bytes(2, "big"),
        0x1A510: (0).to_bytes(2, "big"),
        0x1AB4F: bytes([0x00]),
        RECORD: bytes([0x10, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]),
    }
    for i in range(0x3F):
        base = TABLE + i * 6
        word = 0x0002 if i in matches else 0x0001
        pokes[base] = word.to_bytes(2, "big")
        pokes[base + 2] = bytes([0x20 + i])
        pokes[base + 3] = bytes([0x30 + i])
        pokes[base + 4] = (0x100 + i).to_bytes(2, "big")
    return pokes


def test_fun_15510_dibuja():
    a2 = ctypes.c_uint32(RECORD)
    diffs, info = differential(
        FUN_15510, {"a2": RECORD, "d4": 0x20, "d6": 7, "_pokes": _table_pokes(())},
        lambda lib, buf: lib.g_fun_15510(buf, ctypes.byref(a2), 0x20, 7))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["d6"] == 8
    assert a2.value == info["regs"]["a2"] == RECORD + 0xA


def test_fun_15492_selecciona():
    for matches in ((4,), (4, 10), (0, 0x3E)):
        diffs, info = differential(
            FUN_15492, {"d2": 0x000F, "d3": 0x0002, "_pokes": _table_pokes(matches)},
            lambda lib, buf: lib.g_fun_15492(buf, 0x000F, 0x0002),
            schedule=[{"pc": VBL_SITE, "nth": 1, "addr": VBL_COUNTER,
                       "width": 1, "value": 1}],
            wait_sites=[VBL_SITE])
        assert not diffs, f"matches={matches}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["d6"] == len(matches)


# ================= menu de samples (0x14db2/0x14d76/0x14d90/0x149da) =================

FUN_14DB2 = 0x14DB2
FUN_14D76 = 0x14D76
FUN_14D90 = 0x14D90
FUN_149DA = 0x149DA
IKBD_SITE = 0x136B0
IKBD_PACKET = 0x1A44F

harness._lib.g_fun_14db2.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32]
harness._lib.g_fun_14db2.restype = None
harness._lib.g_fun_14d76.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_fun_14d76.restype = None
harness._lib.g_fun_14d90.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_fun_14d90.restype = None
harness._lib.g_fun_149da.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_fun_149da.restype = ctypes.c_uint32

SCRIPT = 0x51000


def _menu_pokes(matches, cursor=(0, 0)):
    pokes = _table_pokes(matches)
    pokes.update({
        0x1A488: (0x00FF).to_bytes(2, "big"),
        0x1A50E: (0).to_bytes(2, "big"),
        0x1A510: (0).to_bytes(2, "big"),
        0x1A75E: (RECORD).to_bytes(4, "big"),
        0x1AB46: bytes([0x00]),
        0x1AB47: bytes([0x00]),
        0x1AB4F: bytes([0x00]),
        0x1ACE9: bytes([0x10]),
        0x1ACEC: bytes([0x10]),
        0x1ACD2: bytes([0x05]),
        0x1ACD4: bytes([0x00]),
        0x1ACD7: bytes([0x10]),
        0x1AD29: bytes([0x00]),
        0x1A44F: bytes([0xFF]),
        0x1A456: bytes([0x00]),
        0x1A474: cursor[1].to_bytes(2, "big"),
        0x1A472: cursor[0].to_bytes(2, "big"),
        SCRIPT: bytes([2, 0x00]),
    })
    for i in range(0x70):                # bancos de overlay A/B [len=3][c1][c2]
        pokes[0x41586 + i * 3] = bytes([3, 0x21 + (i % 0x40), 0x22])
        pokes[0x41B76 + i * 3] = bytes([3, 0x21 + (i % 0x40), 0x22])
    return pokes


def _click_schedule():
    return [
        {"pc": VBL_SITE, "nth": 1, "addr": VBL_COUNTER, "width": 1, "value": 1},
        {"pc": VBL_SITE, "nth": 2, "addr": VBL_COUNTER, "width": 1, "value": 2},
        {"pc": IKBD_SITE, "nth": 1, "addr": IKBD_PACKET, "width": 1, "value": 0x00},
        {"pc": IKBD_SITE, "nth": 2, "addr": IKBD_PACKET, "width": 1, "value": 0x01},
    ]


def test_fun_14db2_sin_matches():
    """Sin muestras: no hay bucle; restaura marco y para/arranca el Timer A."""
    diffs, _ = differential(
        FUN_14DB2, {"a6": SCRIPT, "d2": 0x8000, "d3": 0x8000,
                    "_pokes": _menu_pokes(())},
        lambda lib, buf: lib.g_fun_14db2(buf, SCRIPT, 0x8000, 0x8000))
    assert not diffs, report(diffs)


def test_fun_14db2_con_clic():
    """Con muestra y clic: ventana + frame + IKBD + hit-test + frame + salida."""
    diffs, _ = differential(
        FUN_14DB2, {"a6": SCRIPT, "d2": 0x000F, "d3": 0x0002,
                    "_pokes": _menu_pokes((4,))},
        lambda lib, buf: lib.g_fun_14db2(buf, SCRIPT, 0x000F, 0x0002),
        schedule=_click_schedule(), wait_sites=[VBL_SITE, IKBD_SITE])
    assert not diffs, report(diffs)


def test_fun_14d76_entrada():
    diffs, _ = differential(
        FUN_14D76, {"a6": SCRIPT, "_pokes": _menu_pokes(())},
        lambda lib, buf: lib.g_fun_14d76(buf, SCRIPT))
    assert not diffs, report(diffs)


def test_fun_14d90_entrada():
    diffs, _ = differential(
        FUN_14D90, {"a6": SCRIPT, "_pokes": _menu_pokes(())},
        lambda lib, buf: lib.g_fun_14d90(buf, SCRIPT))
    assert not diffs, report(diffs)


def test_fun_149da_corta():
    """$1acec >= 0x3f: sale por 0x10768 sin tocar nada."""
    pokes = _menu_pokes(())
    pokes[0x1ACEC] = bytes([0x3F])
    diffs, info = differential(
        FUN_149DA, {"a6": SCRIPT, "_pokes": pokes},
        lambda lib, buf: lib.g_fun_149da(buf, SCRIPT))
    assert not diffs, report(diffs)
    assert info["ret"] == SCRIPT


def test_fun_149da_sin_muestra():
    """Sin muestras: $1ad0e = 1 y restaura $1a4e0."""
    diffs, info = differential(
        FUN_149DA, {"a6": SCRIPT, "_pokes": _menu_pokes(())},
        lambda lib, buf: lib.g_fun_149da(buf, SCRIPT))
    assert not diffs, report(diffs)
    assert info["ret"] == SCRIPT + 1
