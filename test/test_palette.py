"""Pruebas diferenciales de la paleta (0x135da, 0x162ce..0x16300).

Cada prueba ejecuta el original en el oráculo y el candidato (`libkult.so`) sobre
la misma imagen y compara memoria + registros + el ledger de eventos.

- XBIOS Setpalette (0x16300) no escribe imagen: su efecto está en el shifter, así
  que se comprueba por el ledger (`OS_EVENT_SETPALETTE`), igual que el oráculo.
- 0x162e2 sí escribe los 5 words de la entrada en *(A_PALETTE_PTR)+2 y además cae
  en 0x16300 (no tiene `rts`).
- La cadena 0x162d8 -> 0x162da -> 0x162e2 -> 0x16300 se prueba entrada por entrada.
- Para las dos entradas que avanzan el guion (A6), el candidato devuelve el A6
  resultante y la prueba lo compara con `info["regs"]["a6"]` del oráculo.
"""
import ctypes

import harness
from harness import differential, report

_u8p = ctypes.POINTER(ctypes.c_uint8)

harness._lib.xbios_setpalette.argtypes = [_u8p]
harness._lib.xbios_setpalette.restype = None
harness._lib.set_palette.argtypes = [_u8p]
harness._lib.set_palette.restype = None
harness._lib.set_palette_entry.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.set_palette_entry.restype = None
harness._lib.set_palette_current.argtypes = [_u8p]
harness._lib.set_palette_current.restype = None
harness._lib.set_palette_from_entry.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.set_palette_from_entry.restype = ctypes.c_uint32
harness._lib.set_palette_advance.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.set_palette_advance.restype = ctypes.c_uint32

# direcciones originales (entradas del decomp)
XBIOS_SETPALETTE = 0x16300
SET_PALETTE = 0x135da
SET_PALETTE_ENTRY = 0x162e2
SET_PALETTE_CURRENT = 0x162da
SET_PALETTE_FROM_ENTRY = 0x162ce
SET_PALETTE_ADVANCE = 0x162d8

# globales de addrs.h (copia local: Python no lee el .h)
PALETTE_PTR = 0x1a466
PAL_TABLE = 0x1a10a
PAL_INDEX_CUR = 0x1acf4

PAL_BUF = 0x30000       # destino de los 5 words (dentro de la imagen)
PAL_ENTRIES = 10        # entradas de color rellenadas (>= los índices usados)


def _palette_pokes():
    """Rellena A_PAL_TABLE con bytes distinguibles y el buffer destino con 0xaa."""
    pokes = {PALETTE_PTR: PAL_BUF.to_bytes(4, "big"), PAL_INDEX_CUR: bytes([3])}
    for i in range(PAL_ENTRIES * 0xa):
        pokes[PAL_TABLE + i] = bytes([(0x40 + i) & 0xff])
    for i in range(0x20):
        pokes[PAL_BUF + i] = bytes([0xaa])
    return pokes


def _events(info):
    return info["regs"]["events"]


def test_xbios_setpalette():
    """0x16300: XBIOS Setpalette(*(A_PALETTE_PTR)) — solo el evento."""
    pal = 0x18ad2
    diffs, info = differential(
        XBIOS_SETPALETTE, {"_pokes": {PALETTE_PTR: pal.to_bytes(4, "big")}},
        lambda lib, buf: lib.xbios_setpalette(buf))
    assert not diffs, report(diffs)
    assert _events(info) == [(harness.OS_EVENT_SETPALETTE, pal)], (
        f"el oráculo no emitió Setpalette({pal:#x}): {_events(info)}")


def test_set_palette():
    """0x135da: alias `bsr 0x16300` — mismo evento que la entrada XBIOS."""
    pal = 0x18af2
    diffs, info = differential(
        SET_PALETTE, {"_pokes": {PALETTE_PTR: pal.to_bytes(4, "big")}},
        lambda lib, buf: lib.set_palette(buf))
    assert not diffs, report(diffs)
    assert _events(info) == [(harness.OS_EVENT_SETPALETTE, pal)], (
        f"el oráculo no emitió Setpalette({pal:#x}): {_events(info)}")


def test_set_palette_entry():
    """0x162e2: copia la entrada índice d0 a *(A_PALETTE_PTR)+2 y aplica."""
    for idx in (0, 1, 3, 9):
        diffs, info = differential(
            SET_PALETTE_ENTRY, {"d0": idx, "_pokes": _palette_pokes()},
            lambda lib, buf, i=idx: lib.set_palette_entry(buf, i))
        assert not diffs, f"idx={idx}\n{report(diffs)}"
        assert _events(info) == [(harness.OS_EVENT_SETPALETTE, PAL_BUF)], (
            f"idx={idx}: {_events(info)}")


def test_set_palette_current():
    """0x162da: índice desde A_PAL_INDEX_CUR (0x1acf4 = 3) y aplica."""
    diffs, info = differential(
        SET_PALETTE_CURRENT, {"_pokes": _palette_pokes()},
        lambda lib, buf: lib.set_palette_current(buf))
    assert not diffs, report(diffs)
    assert _events(info) == [(harness.OS_EVENT_SETPALETTE, PAL_BUF)]


def test_set_palette_from_entry():
    """0x162ce: índice desde A6[1], avanza A6 +2 y aplica."""
    a6 = 0x40000
    pokes = _palette_pokes()
    pokes[a6 + 1] = bytes([5])
    diffs, info = differential(
        SET_PALETTE_FROM_ENTRY, {"a6": a6, "_pokes": pokes},
        lambda lib, buf: lib.set_palette_from_entry(buf, a6))
    assert not diffs, report(diffs)
    assert _events(info) == [(harness.OS_EVENT_SETPALETTE, PAL_BUF)]
    assert info["ret"] == info["regs"]["a6"] == a6 + 2, (
        f"A6: candidato={info['ret']:#x} oráculo={info['regs']['a6']:#x}")


def test_set_palette_advance():
    """0x162d8: `addq.l #1,a6` y cae en 0x162da (paleta actual)."""
    a6 = 0x40000
    diffs, info = differential(
        SET_PALETTE_ADVANCE, {"a6": a6, "_pokes": _palette_pokes()},
        lambda lib, buf: lib.set_palette_advance(buf, a6))
    assert not diffs, report(diffs)
    assert _events(info) == [(harness.OS_EVENT_SETPALETTE, PAL_BUF)]
    assert info["ret"] == info["regs"]["a6"] == a6 + 1, (
        f"A6: candidato={info['ret']:#x} oráculo={info['regs']['a6']:#x}")
