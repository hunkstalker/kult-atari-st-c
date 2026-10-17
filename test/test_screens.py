"""Pruebas diferenciales de los buffers de pantalla (0x10fec..0x121de).

- copy_screen / swap_screens: el efecto es la imagen, con centinelas en ambos
  buffers (la imagen base está a cero y la copia sería invisible).
- copy_screen_a6: además avanza A6, que se compara con `info["regs"]["a6"]`.
- screen_address* / screen_offset: no escriben imagen; se comparan A0 (retorno) y
  D7 (out-param) con los registros del oráculo.
"""
import ctypes

import harness
from harness import differential, report

_u8p = ctypes.POINTER(ctypes.c_uint8)
_u32p = ctypes.POINTER(ctypes.c_uint32)

harness._lib.copy_screen.argtypes = [_u8p]
harness._lib.copy_screen.restype = None
harness._lib.copy_screen_a6.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.copy_screen_a6.restype = ctypes.c_uint32
harness._lib.swap_screens.argtypes = [_u8p]
harness._lib.swap_screens.restype = None
harness._lib.screen_address.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32, _u32p]
harness._lib.screen_address.restype = ctypes.c_uint32
harness._lib.screen_address_raw.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32, _u32p]
harness._lib.screen_address_raw.restype = ctypes.c_uint32
harness._lib.screen_offset.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                       ctypes.c_uint32, _u32p]
harness._lib.screen_offset.restype = ctypes.c_uint32

COPY_SCREEN = 0x121c6
COPY_SCREEN_A6 = 0x121c4
SWAP_SCREENS = 0x121de
SCREEN_ADDRESS = 0x10fec
SCREEN_ADDRESS_RAW = 0x10fee
SCREEN_OFFSET = 0x10ff4

FRONT = 0x80000   # screen_ptr_a
BACK = 0x78000    # screen_ptr_b


def _pokes():
    """Apunta los dos buffers a scratch mapeada y pone centinelas en los extremos."""
    return {
        0x1a45e: FRONT.to_bytes(4, "big"),   # screen_ptr_a
        0x1a462: BACK.to_bytes(4, "big"),    # screen_ptr_b
        BACK: bytes.fromhex("deadbeefcafebabe"),
        BACK + 31992: bytes.fromhex("0102030405060708"),
        FRONT: bytes.fromhex("1112131415161718"),
        FRONT + 31992: bytes.fromhex("2122232425262728"),
    }


def test_copy_screen():
    diffs, _ = differential(COPY_SCREEN, {"_pokes": _pokes()},
                            lambda lib, buf: lib.copy_screen(buf), poison=True)
    assert not diffs, report(diffs)


def test_swap_screens():
    diffs, _ = differential(SWAP_SCREENS, {"_pokes": _pokes()},
                            lambda lib, buf: lib.swap_screens(buf), poison=True)
    assert not diffs, report(diffs)


def test_copy_screen_a6():
    a6 = 0x40000
    diffs, info = differential(COPY_SCREEN_A6, {"a6": a6, "_pokes": _pokes()},
                               lambda lib, buf: lib.copy_screen_a6(buf, a6))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"] == a6 + 1, (
        f"A6: candidato={info['ret']:#x} oráculo={info['regs']['a6']:#x}")


def test_screen_address():
    """0x10fec: d0 <<= 2 y dirección planar; A0 al retorno, D7 por out-param."""
    pokes = {0x1a45e: (0x80000).to_bytes(4, "big")}
    for d0 in (0, 1, 2, 3, 15, 16, 17, 31, 0x1234, 0xffff, 0x12340000):
        for d1 in (0, 1, 100, 199, 0xffff):
            d7 = ctypes.c_uint32()
            diffs, info = differential(
                SCREEN_ADDRESS, {"d0": d0, "d1": d1, "_pokes": pokes},
                lambda lib, buf, x=d0, y=d1, p=d7: lib.screen_address(buf, x, y, ctypes.byref(p)))
            assert not diffs, f"x={d0:#x} y={d1:#x}\n{report(diffs)}"
            assert info["ret"] == info["regs"]["a0"], f"a0 x={d0:#x} y={d1:#x}"
            assert d7.value == info["regs"]["d7"], f"d7 x={d0:#x} y={d1:#x}"


def test_screen_address_raw():
    """0x10fee: como screen_address pero sin el `lsl.w #2`."""
    pokes = {0x1a45e: (0x80000).to_bytes(4, "big")}
    for d0 in (0, 1, 2, 3, 15, 16, 17, 31, 0x1234, 0xffff):
        for d1 in (0, 1, 100, 199):
            d7 = ctypes.c_uint32()
            diffs, info = differential(
                SCREEN_ADDRESS_RAW, {"d0": d0, "d1": d1, "_pokes": pokes},
                lambda lib, buf, x=d0, y=d1, p=d7:
                lib.screen_address_raw(buf, x, y, ctypes.byref(p)))
            assert not diffs, f"x={d0:#x} y={d1:#x}\n{report(diffs)}"
            assert info["ret"] == info["regs"]["a0"], f"a0 x={d0:#x} y={d1:#x}"
            assert d7.value == info["regs"]["d7"], f"d7 x={d0:#x} y={d1:#x}"


def test_screen_offset():
    """0x10ff4: cuerpo común con A0 de entrada."""
    for a0 in (0, 0x80000, 0x1234):
        for d0 in (0, 3, 0x3c, 0xffff):
            for d1 in (0, 1, 7, 0xffff):
                d7 = ctypes.c_uint32()
                diffs, info = differential(
                    SCREEN_OFFSET, {"a0": a0, "d0": d0, "d1": d1},
                    lambda lib, buf, a=a0, x=d0, y=d1, p=d7:
                    lib.screen_offset(buf, a, x, y, ctypes.byref(p)))
                assert not diffs, f"a0={a0:#x} d0={d0:#x} d1={d1:#x}\n{report(diffs)}"
                assert info["ret"] == info["regs"]["a0"], "a0"
                assert d7.value == info["regs"]["d7"], "d7"
