"""Pruebas diferenciales de video.c (0x16210, 0x16230, 0x1627a, 0x16310, 0x135e2).

Cada prueba ejecuta el original en el oráculo y el candidato (`libkult.so`) sobre
la misma imagen y compara memoria + registros + el ledger de eventos off-image.

- `pal_next_byte` (0x16210) / `pal_next_word` (0x16230): avanzan el contador byte
  0x1a579 (wrap a 8 bits) e indexan la tabla 0x2741e; devuelven el valor en D0.
- `pal_threshold` (0x1627a): solo cuando el byte bajo de (0x1ab60-0x1a526) llega a
  0xb4, engancha 0x1a526 y limpia el bit 6 de las entradas de 6 bytes de 0x1a8f0.
- `palette_cycle` (0x16310): ciclo de paleta con dos llamadas de sonido distintas
  (0x18f98 al entrar y 0x18f8e al pasar a la fase 2) y VBLs por pasada.
- `crt0_init` (0x135e2): arranque; XBIOS Getrez/Physbase/Kbdvbase + GEMDOS Super.
"""
import ctypes

import harness
from harness import differential, report

_u8p = ctypes.POINTER(ctypes.c_uint8)

harness._lib.pal_next_byte.argtypes = [_u8p]
harness._lib.pal_next_byte.restype = ctypes.c_uint32
harness._lib.pal_next_word.argtypes = [_u8p]
harness._lib.pal_next_word.restype = ctypes.c_uint32
harness._lib.pal_threshold.argtypes = [_u8p]
harness._lib.pal_threshold.restype = None
harness._lib.palette_cycle.argtypes = [_u8p]
harness._lib.palette_cycle.restype = None
harness._lib.crt0_init.argtypes = [_u8p]
harness._lib.crt0_init.restype = None

# direcciones originales (entradas del decomp)
PAL_NEXT_BYTE = 0x16210
PAL_NEXT_WORD = 0x16230
PAL_THRESHOLD = 0x1627A
PALETTE_CYCLE = 0x16310
CRT0_INIT = 0x135E2

# globales de addrs.h (copia local: Python no lee el .h)
DAT_1A579 = 0x1A579
TBL_2741E = 0x2741E
DAT_1A526 = 0x1A526
DAT_1AB60 = 0x1AB60
DAT_1ACD2 = 0x1ACD2
DAT_1ACD7 = 0x1ACD7
LIST_1A8F0 = 0x1A8F0
PALETTE_PTR = 0x1A466
PAL_TABLE = 0x1A10A
PAL_INDEX_CUR = 0x1ACF4
DAT_1ABFA = 0x1ABFA
FRAME_1AB4F = 0x1AB4F
THE_COMMAND = 0x1A50E
VBL_COUNTER = 0x1ACD4
VBL_SITE = 0x1359A

PAL_BUF = 0x40000


def _vbl_k(n):
    return [{"pc": VBL_SITE, "nth": k, "addr": VBL_COUNTER, "width": 1, "value": k}
            for k in range(1, n + 1)]


def test_pal_next_byte():
    for start in (0, 1, 0x0f, 0x7f, 0xfe, 0xff):
        pokes = {DAT_1A579: bytes([start]),
                 DAT_1ACD7: bytes([0xaa])}
        for i in range(0x100):
            pokes[TBL_2741E + i] = bytes([(0x10 + i * 3) & 0xFF])
        diffs, info = differential(
            PAL_NEXT_BYTE, {"_pokes": pokes},
            lambda lib, buf: lib.pal_next_byte(buf))
        assert not diffs, f"start={start:#x}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["d0"], f"start={start:#x} d0"


def test_pal_next_word():
    for start in (0, 1, 0x0f, 0x7f, 0xfe, 0xff):
        pokes = {DAT_1A579: bytes([start])}
        for i in range(0x100):
            pokes[TBL_2741E + i] = bytes([(0x10 + i * 3) & 0xFF])
        diffs, info = differential(
            PAL_NEXT_WORD, {"_pokes": pokes},
            lambda lib, buf: lib.pal_next_word(buf))
        assert not diffs, f"start={start:#x}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["d0"], f"start={start:#x} d0"


def _threshold_pokes(delta, mode):
    pokes = {DAT_1AB60: (delta & 0xffff).to_bytes(2, "big"),
             DAT_1A526: (0).to_bytes(2, "big"),
             DAT_1ACD2: bytes([mode])}
    # 63 entradas de 6 bytes con combinaciones que cubren la rama de limpieza.
    for i in range(0x3F):
        b0 = [0x40, 0x00, 0x41, 0x40, 0x40][i % 5]
        b1 = [0x00, mode, mode, 0x10, 0x40][i % 5]
        pokes[LIST_1A8F0 + i * 6] = bytes([b0])
        pokes[LIST_1A8F0 + i * 6 + 1] = bytes([b1])
    return pokes


def test_pal_threshold():
    for delta, mode in ((0x00B3, 0x05), (0x00B4, 0x05),
                        (0x01B4, 0x10), (0x00FF, 0x05)):
        pokes = _threshold_pokes(delta, mode)
        diffs, info = differential(
            PAL_THRESHOLD, {"_pokes": pokes},
            lambda lib, buf: lib.pal_threshold(buf))
        assert not diffs, f"delta={delta:#x} mode={mode:#x}\n{report(diffs)}"


def _f16310_pokes(color, target, frame):
    pokes = {
        PALETTE_PTR: PAL_BUF.to_bytes(4, "big"),
        PAL_INDEX_CUR: bytes([0x00]),
        FRAME_1AB4F: bytes([frame]),
        DAT_1ABFA: (0).to_bytes(2, "big"),
        THE_COMMAND: (0).to_bytes(2, "big"),
        VBL_COUNTER: bytes([0x00]),
    }
    for i in range(5):
        pokes[PAL_BUF + 2 + i * 2] = color.to_bytes(2, "big")
        pokes[PAL_TABLE + i * 2] = target.to_bytes(2, "big")
    return pokes


def test_palette_cycle():
    for color, target, waits in ((0x777, 0x777, 0), (0x111, 0x777, 6),
                                 (0x777, 0x111, 6)):
        pokes = _f16310_pokes(color, target, 1)     # sonido enmascarado (0x1ab4f=1)
        sched = _vbl_k(waits) if waits else []
        diffs, _ = differential(
            PALETTE_CYCLE, {"_pokes": pokes},
            lambda lib, buf: lib.palette_cycle(buf),
            schedule=sched, wait_sites=[VBL_SITE] if sched else None,
            max_insns=3_000_000)
        assert not diffs, f"color={color:#x} target={target:#x}\n{report(diffs)}"


def test_palette_cycle_sound():
    """Con 0x1ab4f=0 corre el sonido: el 2º wrapper es 0x18f8e, no 0x18f98.

    Regresión del hallazgo H-1: la fase 2 llama a `sound_play_a` (0x18f8e ->
    0x18fc4, deja el vector $134 = 0x1910e) y no a `sound_play_b` (0x18f98 ->
    0x1906a, lo deja a 0x190f6 y publica 0x192c6).
    """
    pokes = _f16310_pokes(0x777, 0x777, 0)
    diffs, _ = differential(
        PALETTE_CYCLE, {"_pokes": pokes},
        lambda lib, buf: lib.palette_cycle(buf))
    assert not diffs, report(diffs)


def test_crt0_init():
    pokes = {}
    for i in range(4):
        pokes[0x4BD + i] = bytes([(0x20 + i) & 0xFF])
    diffs, _ = differential(
        CRT0_INIT, {"_pokes": pokes},
        lambda lib, buf: lib.crt0_init(buf))
    assert not diffs, report(diffs)
