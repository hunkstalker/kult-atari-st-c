"""Pruebas diferenciales de los `rts` pelados (stubs.c).

0x135e0, 0x160c0, 0x16dba, 0x19312: no tocan la imagen ni registros.
"""
import ctypes

import harness
from harness import differential, report

_u8p = ctypes.POINTER(ctypes.c_uint8)
for _n in ("noop_135e0", "noop_160c0", "noop_16dba", "noop_19312"):
    getattr(harness._lib, _n).argtypes = [_u8p]
    getattr(harness._lib, _n).restype = None


def test_bare_rts_stubs():
    for entry, fn in ((0x135E0, harness._lib.noop_135e0),
                      (0x160C0, harness._lib.noop_160c0),
                      (0x16DBA, harness._lib.noop_16dba),
                      (0x19312, harness._lib.noop_19312)):
        diffs, _ = differential(entry, {"_pokes": {}}, lambda lib, buf, f=fn: f(buf))
        assert not diffs, f"{entry:#x}\n{report(diffs)}"
