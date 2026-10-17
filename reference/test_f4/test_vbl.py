"""Test diferencial de la espera de VBL FUN_00013594 @ 0x13594.

El contador $1acd4 lo cambia la VBL (fuera de la imagen), así que la liberación se
declara con el modelo de escrituras programadas del kit (`schedule`/`wait_sites`,
TRAP_MODEL.md Fase 8): el oráculo cuenta llegadas al sitio de espera 0x1359a y el
candidato sondea con `sched_poll8` una vez por iteración.
"""
import ctypes

import harness
from harness import differential, report

VBL_WAIT = 0x13594
COUNTER = 0x1acd4
SITE = 0x1359a

_u8p = ctypes.POINTER(ctypes.c_uint8)
harness._lib.g_fun_13594.argtypes = [_u8p]
harness._lib.g_fun_13594.restype = None


def test_fun_13594_vbl_wait():
    for start, nxt in ((0x10, 0x11), (0x00, 0x01), (0xff, 0x00), (0x80, 0x81), (0x7f, 0x7e)):
        diffs, _ = differential(
            VBL_WAIT, {"_pokes": {COUNTER: bytes([start])}},
            lambda lib, buf: lib.g_fun_13594(buf),
            schedule=[{"pc": SITE, "nth": 1, "addr": COUNTER, "width": 1, "value": nxt}],
            wait_sites=[SITE])
        assert not diffs, f"start={start:#x} next={nxt:#x}\n{report(diffs)}"


FUN_16D1E = 0x16D1E
harness._lib.g_fun_16d1e.argtypes = [_u8p]
harness._lib.g_fun_16d1e.restype = None


def test_fun_16d1e_vbl_wait():
    """Espera a que cambie el contador y a que el nuevo valor sea par."""
    for start, nxt in ((0x10, 0x12), (0x11, 0x14), (0x00, 0x02)):
        diffs, _ = differential(
            FUN_16D1E, {"_pokes": {COUNTER: bytes([start])}},
            lambda lib, buf: lib.g_fun_16d1e(buf),
            schedule=[{"pc": 0x16D24, "nth": 1, "addr": COUNTER, "width": 1, "value": nxt}],
            wait_sites=[0x16D24])
        assert not diffs, f"start={start:#x} next={nxt:#x}\n{report(diffs)}"
