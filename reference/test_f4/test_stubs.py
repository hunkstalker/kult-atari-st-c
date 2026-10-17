"""Tests de los `rts` pelados (0x135e0, 0x160c0, 0x16dba, 0x19312) y 0x162d8."""
import ctypes
import harness
from harness import differential, report

_u8p = ctypes.POINTER(ctypes.c_uint8)
for _n in ("g_fun_135e0", "g_fun_160c0", "g_fun_16dba", "g_fun_19312", "g_fun_162d8"):
    getattr(harness._lib, _n).argtypes = [_u8p]
    getattr(harness._lib, _n).restype = None


def test_bare_rts_stubs():
    for entry, fn in ((0x135E0, harness._lib.g_fun_135e0),
                      (0x160C0, harness._lib.g_fun_160c0),
                      (0x16DBA, harness._lib.g_fun_16dba),
                      (0x19312, harness._lib.g_fun_19312)):
        diffs, _ = differential(entry, {"_pokes": {}}, lambda lib, buf, f=fn: f(buf))
        assert not diffs, f"{entry:#x}\n{report(diffs)}"


def test_fun_162d8():
    pokes = {0x1a466: (0x30000).to_bytes(4, "big"), 0x1acf4: bytes([3])}
    for i in range(100):
        pokes[0x1a10a + i] = bytes([(0x40 + i) & 0xFF])
    for i in range(32):
        pokes[0x30000 + i] = bytes([0xAA])
    diffs, info = differential(0x162D8, {"_pokes": pokes},
                               lambda lib, buf: lib.g_fun_162d8(buf))
    assert not diffs, report(diffs)
    assert info["regs"]["events"] == [(harness.OS_EVENT_SETPALETTE, 0x30000)]
