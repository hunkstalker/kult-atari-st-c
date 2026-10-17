"""Differential test for the IKBD packet handler: FUN_00013686 @ 0x13686.

Copies 5 bytes from A0 (the IKBD packet) to 0x1a44f..0x1a453, where wait_frame reads
input_buttons and cursor_x/y. Pure: the whole effect is the image.
"""
import ctypes

import harness
from harness import differential, report

IKBD_HANDLER = 0x13686
PKT = 0x20000

_u8p = ctypes.POINTER(ctypes.c_uint8)
harness._lib.g_ikbd_handler.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_ikbd_handler.restype = None


def test_ikbd_handler():
    pokes = {PKT: bytes([0x05, 0x12, 0x34, 0x56, 0x78])}
    diffs, _ = differential(IKBD_HANDLER, {"a0": PKT, "_pokes": pokes},
                            lambda lib, buf: lib.g_ikbd_handler(buf, PKT))
    assert not diffs, report(diffs)


WAIT_FRAME = 0x13698
PKT_ADDR = 0x1a44f
BUTTONS = 0x1a456
SKIP = 0x1a570
SITE = 0x136b0

harness._lib.g_fun_13698.argtypes = [_u8p]
harness._lib.g_fun_13698.restype = None


def _wf_pokes(start):
    return {
        PKT_ADDR: bytes([start]),
        BUTTONS: bytes([0x00]),
        SKIP: bytes([0x00]),
    }


def _run_wf(start, value):
    return differential(
        WAIT_FRAME, {"_pokes": _wf_pokes(start)},
        lambda lib, buf: lib.g_fun_13698(buf),
        schedule=[{"pc": SITE, "nth": 1, "addr": PKT_ADDR, "width": 1, "value": value}],
        wait_sites=[SITE])


def test_wait_frame_release_no_click():
    """El paquete llega sin botón 0: no se marca clic, d0 = 0."""
    diffs, _ = _run_wf(0xff, 0x00)
    assert not diffs, report(diffs)


def test_wait_frame_button0_click():
    """El paquete pide botón 0 (bit 0): se marca clic (bit 2) y d0 = 0xff."""
    diffs, _ = _run_wf(0xff, 0x01)
    assert not diffs, report(diffs)


def test_wait_frame_stale_packet():
    """$1a44f ya distinto de 0xff: el compare no gira, se procesa el paquete tal cual."""
    diffs, _ = _run_wf(0x02, 0x03)
    assert not diffs, report(diffs)
