"""Tests diferenciales de la medida/justificado del texto.

FUN_00010946 @ 0x10946: mide una "palabra" del banco de texto desde A5 hasta D0 unidades
(+1 por byte imprimible, +1 extra si lleva flag 0x80/0xc0, un 0x40 la cierra contándola,
un byte sin bits bajos la cierra sin contarla). Devuelve A4 final.
FUN_00010998 @ 0x10998: cuenta palabras (D5) y la más ancha (D6) hasta cur_str_end.
FUN_000109bc @ 0x109bc: justifica a líneas de ancho *(0x1a512) -> D5 (líneas) y D6 (resto).

Las cadenas son aleatorias para cubrir todos los flags; el avance de A4 lo verifica el
diferencial por bytes y los contadores se comparan con los registros del oráculo.
"""
import ctypes
import random

import harness
from harness import differential, report

FUN_10946 = 0x10946
FUN_10998 = 0x10998
FUN_109BC = 0x109BC
STRBUF = 0x40000
CUR_STR_END = 0x1a4dc
TEXT_BOX_W = 0x1a512

_u8p = ctypes.POINTER(ctypes.c_uint8)
_u32p = ctypes.POINTER(ctypes.c_uint32)
harness._lib.measure_word.argtypes = [_u8p, ctypes.c_uint32, _u32p]
harness._lib.measure_word.restype = ctypes.c_uint32
for _name in ("measure_words", "wrap_line"):
    getattr(harness._lib, _name).argtypes = [_u8p, ctypes.c_uint32, _u32p, _u32p]
    getattr(harness._lib, _name).restype = None


def _random_bank(rng, n):
    data = bytes(rng.randrange(256) for _ in range(n))
    pokes = {STRBUF + i: bytes([b]) for i, b in enumerate(data)}
    pokes[CUR_STR_END] = (STRBUF + n).to_bytes(4, "big")
    return data, pokes


def test_fun_10946():
    rng = random.Random(7)
    for trial in range(300):
        data, pokes = _random_bank(rng, rng.randrange(1, 48))
        a4 = ctypes.c_uint32(0)
        diffs, info = differential(FUN_10946, {"a5": STRBUF, "_pokes": pokes},
                                   lambda lib, buf, p=a4:
                                   lib.measure_word(buf, STRBUF, ctypes.byref(p)))
        assert not diffs, f"trial={trial} data={data.hex()}\n{report(diffs)}"
        assert a4.value == info["regs"]["a4"], f"trial={trial} data={data.hex()}"
        assert info["ret"] == info["regs"]["d0"], f"trial={trial} data={data.hex()}"


def test_fun_10998():
    rng = random.Random(11)
    for trial in range(200):
        data, pokes = _random_bank(rng, rng.randrange(1, 48))
        d5, d6 = ctypes.c_uint32(0), ctypes.c_uint32(0)
        diffs, info = differential(FUN_10998, {"a5": STRBUF, "_pokes": pokes},
                                   lambda lib, buf, p=d5, q=d6:
                                   lib.measure_words(buf, STRBUF, ctypes.byref(p), ctypes.byref(q)))
        assert not diffs, f"trial={trial} data={data.hex()}\n{report(diffs)}"
        assert d5.value == info["regs"]["d5"], f"trial={trial} data={data.hex()}"
        assert d6.value == info["regs"]["d6"], f"trial={trial} data={data.hex()}"
        assert info["regs"]["a5"] == STRBUF, "A5 no restaurado"


def test_fun_109bc():
    rng = random.Random(23)
    for trial in range(300):
        data, pokes = _random_bank(rng, rng.randrange(1, 60))
        pokes[TEXT_BOX_W] = rng.randrange(1, 40).to_bytes(2, "big")
        d5, d6 = ctypes.c_uint32(0), ctypes.c_uint32(0)
        diffs, info = differential(FUN_109BC, {"a5": STRBUF, "_pokes": pokes},
                                   lambda lib, buf, p=d5, q=d6:
                                   lib.wrap_line(buf, STRBUF, ctypes.byref(p), ctypes.byref(q)))
        assert not diffs, (f"trial={trial} w={int.from_bytes(pokes[TEXT_BOX_W], 'big')} "
                           f"data={data.hex()}\n{report(diffs)}")
        assert d5.value == info["regs"]["d5"], f"trial={trial} data={data.hex()}"
        assert d6.value == info["regs"]["d6"], f"trial={trial} data={data.hex()}"
        assert info["regs"]["a5"] == STRBUF, "A5 no restaurado"


FUN_1090E = 0x1090E
SCREEN_T = 0x80000
DRAW_BASE_T = 0x8000
COLOR_T = 0x1d382

harness._lib.print_words_fit.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32, _u32p, _u32p]
harness._lib.print_words_fit.restype = ctypes.c_uint32


def _draw_pokes(bank, width):
    pokes = {
        0x1a45e: SCREEN_T.to_bytes(4, "big"),
        0x1a46a: DRAW_BASE_T.to_bytes(4, "big"),
        0x1a50a: (0).to_bytes(2, "big"),
        0x1a50c: (0).to_bytes(2, "big"),
        0x1a4d8: COLOR_T.to_bytes(4, "big"),
        CUR_STR_END: (STRBUF + len(bank)).to_bytes(4, "big"),
        TEXT_BOX_W: width.to_bytes(2, "big"),
    }
    for i, b in enumerate(bank):
        pokes[STRBUF + i] = bytes([b])
    return pokes


def test_fun_1090e():
    """Palabras [0x21 0x62][0x23 0x64][0x65][0x26 0x27]; imprime las que caben en *(0x1a512)."""
    bank = bytes([0x21, 0x22 | 0x40, 0x23, 0x24 | 0x40, 0x25 | 0x40, 0x26, 0x27])
    for width in range(0, 12):
        pokes = _draw_pokes(bank, width)
        a5, d6o = ctypes.c_uint32(0), ctypes.c_uint32(0)
        diffs, info = differential(
            FUN_1090E, {"a5": STRBUF, "d0": 0x1234, "_pokes": pokes},
            lambda lib, buf, p=a5, q=d6o:
            lib.print_words_fit(buf, STRBUF, 0x1234, ctypes.byref(p), ctypes.byref(q)))
        assert not diffs, f"width={width}\n{report(diffs)}"
        assert a5.value == info["regs"]["a5"], f"width={width}"
        assert d6o.value == info["regs"]["d6"], f"width={width}"
        assert info["ret"] == (info["regs"]["d0"] & 0xffff), f"width={width}"


FUN_10B24 = 0x10B24
harness._lib.print_line.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.print_line.restype = ctypes.c_uint32


def test_fun_10b24():
    """0x10b24 mide la línea, ajusta *(0x1a512) y la imprime rellenando con espacios."""
    bank = bytes([0x21, 0x22 | 0x40, 0x23, 0x24 | 0x40, 0x25 | 0x40, 0x26, 0x27])
    for init in (0, 5):
        pokes = _draw_pokes(bank, init)
        diffs, info = differential(FUN_10B24, {"a5": STRBUF, "_pokes": pokes},
                                   lambda lib, buf: lib.print_line(buf, STRBUF))
        assert not diffs, f"init={init}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a5"], f"init={init}"
