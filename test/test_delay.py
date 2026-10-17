"""Pruebas diferenciales de las esperas de VBL (delay.c).

wait_vbl 0x13594, wait_vbl_even 0x16d1e. El contador 0x1acd4 lo cambia la VBL
(fuera de la imagen): la liberación se declara con `schedule`/`wait_sites` y el
candidato sondea con sched_poll8 una vez por iteración.
"""
import ctypes

import harness
from harness import differential, report

_u8p = ctypes.POINTER(ctypes.c_uint8)
harness._lib.wait_vbl.argtypes = [_u8p]
harness._lib.wait_vbl.restype = None
harness._lib.wait_vbl_even.argtypes = [_u8p]
harness._lib.wait_vbl_even.restype = None

WAIT_VBL = 0x13594
WAIT_VBL_EVEN = 0x16d1e
COUNTER = 0x1acd4
SITE = 0x1359a
SITE_EVEN = 0x16d24


def test_wait_vbl():
    for start, nxt in ((0x10, 0x11), (0x00, 0x01), (0xff, 0x00), (0x80, 0x81), (0x7f, 0x7e)):
        diffs, _ = differential(
            WAIT_VBL, {"_pokes": {COUNTER: bytes([start])}},
            lambda lib, buf: lib.wait_vbl(buf),
            schedule=[{"pc": SITE, "nth": 1, "addr": COUNTER, "width": 1, "value": nxt}],
            wait_sites=[SITE])
        assert not diffs, f"start={start:#x} next={nxt:#x}\n{report(diffs)}"


def test_wait_vbl_even():
    """Espera a que cambie el contador y a que el nuevo valor sea par."""
    for start, nxt in ((0x10, 0x12), (0x11, 0x14), (0x00, 0x02)):
        diffs, _ = differential(
            WAIT_VBL_EVEN, {"_pokes": {COUNTER: bytes([start])}},
            lambda lib, buf: lib.wait_vbl_even(buf),
            schedule=[{"pc": SITE_EVEN, "nth": 1, "addr": COUNTER, "width": 1, "value": nxt}],
            wait_sites=[SITE_EVEN])
        assert not diffs, f"start={start:#x} next={nxt:#x}\n{report(diffs)}"
