"""Tests de los cargadores de pantalla y de la pila de llamadas de script.

FUN_0001350c @ 0x1350c: a6 = TEMPL + (a6[2]<<8 | a6[1]).
FUN_00013522 @ 0x13522: idem, apilando antes (a6+3, $1a4e0) en 0x1a7d6.
FUN_00013552 @ 0x13552: desapila (restaura $1a4e0 y A6).
load_fond @ 0x13568 / load_named_screen @ 0x13572: cargan 0x7d22 bytes en screen_ptr_b-0x22.
"""
import ctypes

import harness
from harness import differential, report, stage_files

FUN_1350C = 0x1350C
FUN_13522 = 0x13522
FUN_13552 = 0x13552
LOAD_FOND = 0x13568
LOAD_NAMED = 0x13572
TEMPL = 0x1d402
CALL_STACK = 0x1a7d6
SCRIPT_END = 0x1a4e0
SCRIPTP = 0x40000

_u8p = ctypes.POINTER(ctypes.c_uint8)
_u32p = ctypes.POINTER(ctypes.c_uint32)
harness._lib.script_push.argtypes = [_u8p, _u32p]
harness._lib.script_push.restype = None
harness._lib.script_pop.argtypes = [_u8p, _u32p]
harness._lib.script_pop.restype = None
harness._lib.script_call.argtypes = [_u8p, _u32p]
harness._lib.script_call.restype = None
harness._lib.load_fond.argtypes = [_u8p]
harness._lib.load_fond.restype = None
harness._lib.load_named_screen.argtypes = [_u8p]
harness._lib.load_named_screen.restype = None


def _script_pokes():
    return {
        0x1a55a: (0x1a41e).to_bytes(4, "big"),
        0x1a55e: (0x30000).to_bytes(4, "big"),
        0x1a562: (4).to_bytes(4, "big"),
        0x1a47e: (0).to_bytes(2, "big"),
        0x1a420: bytes([0]),   # nombre vacio (nunca se abre en estos casos)
        SCRIPTP + 1: bytes([0x10]),
        SCRIPTP + 2: bytes([0x00]),
    }


def test_fun_1350c():
    a6 = ctypes.c_uint32(SCRIPTP)
    diffs, info = differential(FUN_1350C, {"a6": SCRIPTP, "_pokes": _script_pokes()},
                               lambda lib, buf, p=a6:
                               lib.script_push(buf, ctypes.byref(p)))
    assert not diffs, report(diffs)
    assert a6.value == info["regs"]["a6"], "A6 nuevo"


def test_fun_13522():
    a6 = ctypes.c_uint32(SCRIPTP)
    pokes = _script_pokes()
    pokes[CALL_STACK] = (0x30000).to_bytes(4, "big")
    pokes[SCRIPT_END] = (0x12345678).to_bytes(4, "big")
    diffs, info = differential(FUN_13522, {"a6": SCRIPTP, "_pokes": pokes},
                               lambda lib, buf, p=a6:
                               lib.script_pop(buf, ctypes.byref(p)))
    assert not diffs, report(diffs)
    assert a6.value == info["regs"]["a6"], "A6 nuevo"


def test_fun_13552():
    pokes = {
        CALL_STACK: (0x2fff0).to_bytes(4, "big"),
        0x2fff0: (0xdeadbeef).to_bytes(4, "big"),   # $1a4e0 a restaurar
        0x2fff4: (0x000abcde).to_bytes(4, "big"),   # a6 a restaurar
    }
    a6 = ctypes.c_uint32(0)
    diffs, info = differential(FUN_13552, {"a6": 0, "_pokes": pokes},
                               lambda lib, buf, p=a6:
                               lib.script_call(buf, ctypes.byref(p)))
    assert not diffs, report(diffs)
    assert a6.value == 0x000abcde
    assert a6.value == info["regs"]["a6"], "A6 restaurado"


def test_load_named_screen():
    data = bytes((i * 7) & 0xFF for i in range(0x7d22))
    pokes, _h = stage_files([("SCREEN.BIN", data, 0x7d22)])
    pokes[0x1a41e] = b"SCREEN.BIN\0"
    pokes[0x1a55a] = (0x1a41e).to_bytes(4, "big")   # cur_filename
    pokes[0x1a462] = (0x90000).to_bytes(4, "big")   # screen_ptr_b
    diffs, _ = differential(LOAD_NAMED, {"_pokes": pokes},
                            lambda lib, buf: lib.load_named_screen(buf))
    assert not diffs, report(diffs)


def test_load_fond():
    data = bytes((i * 5 + 1) & 0xFF for i in range(0x7d22))
    pokes, _h = stage_files([("FOND.BIN", data, 0x7d22)])
    pokes[0x1a41e] = b"FOND.BIN\0"                  # nombre en 0x1a41e
    pokes[0x1a55a] = (0x1a41e).to_bytes(4, "big")   # cur_filename
    pokes[0x1a462] = (0x90000).to_bytes(4, "big")
    diffs, _ = differential(LOAD_FOND, {"_pokes": pokes},
                            lambda lib, buf: lib.load_fond(buf))
    assert not diffs, report(diffs)


FUN_10682 = 0x10682
FUN_1066E = 0x1066E
FUN_10668 = 0x10668
harness._lib.load_eche_bank.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.load_eche_bank.restype = None
harness._lib.load_eche_bank_wrap.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.load_eche_bank_wrap.restype = None
harness._lib.load_eche_bank_cmd.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.load_eche_bank_cmd.restype = ctypes.c_uint32


def _10682_pokes(last=0x00, table_head=0x24):
    return {0x1a57c: bytes([last]), 0x1a404: bytes([table_head]),
            0x1a4e0: (0x12345678).to_bytes(4, "big"),
            SCRIPTP: bytes([0x00, 0x35]), SCRIPTP + 2: bytes([0x00])}


def test_fun_10682():
    # 0xff: sin escribir; digito igual: sin escribir; distinto: escribe y restaura $1a4e0.
    for last, d0 in ((0xFF, 5), (0x35, 5), (0x00, 5)):
        pokes = _10682_pokes(last)
        diffs, _ = differential(
            FUN_10682, {"d0": d0, "d1": 0x1234, "_pokes": pokes},
            lambda lib, buf, x=d0: lib.load_eche_bank(buf, x, 0x1234))
        assert not diffs, f"last={last:#x} d0={d0}\n{report(diffs)}"


def test_fun_1066e():
    pokes = _10682_pokes(0x00)
    diffs, _ = differential(FUN_1066E, {"d0": 5, "_pokes": pokes},
                            lambda lib, buf: lib.load_eche_bank_wrap(buf, 5))
    assert not diffs, report(diffs)


def test_fun_10668():
    pokes = _10682_pokes(0x00)
    diffs, info = differential(FUN_10668, {"a6": SCRIPTP, "_pokes": pokes},
                               lambda lib, buf: lib.load_eche_bank_cmd(buf, SCRIPTP))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"], "a6"
