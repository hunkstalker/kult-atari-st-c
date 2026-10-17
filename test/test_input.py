"""Pruebas diferenciales del input: ikbd_packet_copy @ 0x13686, wait_frame @ 0x13698.

wait_frame espera el paquete del IKBD; la llegada se modela con una escritura
programada (`schedule`) en el sitio del compare 0x136b0 (TRAP_MODEL.md Fase 8).
"""
import ctypes

import harness
from harness import differential, report

_u8p = ctypes.POINTER(ctypes.c_uint8)
harness._lib.ikbd_packet_copy.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.ikbd_packet_copy.restype = None
harness._lib.wait_frame.argtypes = [_u8p]
harness._lib.wait_frame.restype = None

IKBD_PACKET_COPY = 0x13686
WAIT_FRAME = 0x13698
PKT = 0x20000
PKT_ADDR = 0x1a44f
BUTTONS = 0x1a456
SKIP = 0x1a570
SITE = 0x136b0


def test_ikbd_packet_copy():
    pokes = {PKT: bytes([0x05, 0x12, 0x34, 0x56, 0x78])}
    diffs, _ = differential(IKBD_PACKET_COPY, {"a0": PKT, "_pokes": pokes},
                            lambda lib, buf: lib.ikbd_packet_copy(buf, PKT))
    assert not diffs, report(diffs)


def _run_wf(start, value):
    pokes = {PKT_ADDR: bytes([start]), BUTTONS: bytes([0x00]), SKIP: bytes([0x00])}
    return differential(
        WAIT_FRAME, {"_pokes": pokes},
        lambda lib, buf: lib.wait_frame(buf),
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
    """$1a44f ya distinto de 0xff: el compare no gira, se procesa tal cual."""
    diffs, _ = _run_wf(0x02, 0x03)
    assert not diffs, report(diffs)
