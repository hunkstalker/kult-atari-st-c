"""Differential tests for the proportional text-blit subtree.

FUN_00011196 @ 0x11196: source-word permutation (result in D0/D1).
FUN_00011050 @ 0x11050: forward text blitter (A0 dest, A1 src, A3 stride, D2 cols,
                       D3 rows, D7 pixel phase).
FUN_00011102 @ 0x11102: bit-reversed variant (each word goes through FUN_00011196).
FUN_00014138 @ 0x14138: the script-entry driver that computes the screen address and
                       dispatches to one of the two blitters.

The blitters write pixels (plus the 0x55b02 scratch), so the byte diff verifies them;
FUN_00011196 is register-only, so its outputs are asserted against the oracle's D0/D1.
"""
import ctypes
import random

import harness
from harness import differential, report, console_key

BIT_REVERSE = 0x11196
BLIT_FWD = 0x11050
BLIT_REV = 0x11102
FUN_14138 = 0x14138
FUN_141DC = 0x141dc
FUN_141EA = 0x141ea
FUN_11014 = 0x11014

SRC = 0x30000
DEST = 0x80000

A6 = 0x30000
TEXT_TABLE = 0x41fb0
SCREEN = 0x90000

_u8p = ctypes.POINTER(ctypes.c_uint8)
harness._lib.g_bit_reverse_word.argtypes = [ctypes.c_uint32, ctypes.c_uint32,
                                            ctypes.POINTER(ctypes.c_uint32),
                                            ctypes.POINTER(ctypes.c_uint32)]
harness._lib.g_bit_reverse_word.restype = None
harness._lib.g_text_blit_fwd.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                         ctypes.c_uint32, ctypes.c_uint32,
                                         ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_text_blit_fwd.restype = None
harness._lib.g_text_blit_rev.argtypes = list(harness._lib.g_text_blit_fwd.argtypes)
harness._lib.g_text_blit_rev.restype = None
harness._lib.g_fun_14138.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_fun_14138.restype = None
harness._lib.g_fun_141dc.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_fun_141dc.restype = None
harness._lib.g_fun_141ea.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_fun_141ea.restype = None
harness._lib.g_fun_11014.argtypes = [_u8p]
harness._lib.g_fun_11014.restype = None


def test_bit_reverse_word():
    for d0 in (0, 1, 2, 0x00f0, 0x0f00, 0x8000, 0xffff, 0x1234, 0xabcd):
        for d1 in (0, 0xffff, 0x5555):
            o0, o1 = ctypes.c_uint32(), ctypes.c_uint32()
            diffs, info = differential(BIT_REVERSE, {"d0": d0, "d1": d1},
                                       lambda lib, buf, a=d0, b=d1, p=o0, q=o1:
                                       lib.g_bit_reverse_word(a, b, ctypes.byref(p),
                                                              ctypes.byref(q)))
            assert not diffs, report(diffs)
            assert o0.value == info["regs"]["d0"], f"d0={d0:#x} d1={d1:#x}"
            assert o1.value == info["regs"]["d1"], f"d0={d0:#x} d1={d1:#x}"


def _run_blit(entry, fn, rng, trials=80):
    for trial in range(trials):
        cols = rng.randrange(1, 5)
        rows = rng.randrange(1, 5)
        d7 = rng.choice([0, 4, 8, 12])
        stride = (cols * 2) if rng.random() < 0.7 else rng.randrange(2, 30)
        pokes = {SRC + i: bytes([rng.randrange(256)]) for i in range(cols * 2 * rows + 4)}
        pokes.update({DEST + i: bytes([0x0f if i % 2 else 0xf0])
                      for i in range(0xa0 * (rows + 1))})
        regs = {"a0": DEST, "a1": SRC, "a3": stride, "d2": cols, "d3": rows, "d7": d7,
                "_pokes": pokes}
        diffs, _ = differential(entry, regs,
                                lambda lib, buf, a0=DEST, a1=SRC, a3=stride, c=cols,
                                r=rows, p=d7, f=fn:
                                f(buf, a0, a1, a3, c, r, p))
        assert not diffs, (f"trial={trial} cols={cols} rows={rows} d7={d7} "
                           f"stride={stride}\n{report(diffs)}")


def test_text_blit_fwd():
    _run_blit(BLIT_FWD, harness._lib.g_text_blit_fwd, random.Random(3))


def test_text_blit_rev():
    _run_blit(BLIT_REV, harness._lib.g_text_blit_rev, random.Random(3))


def _entry_pokes(s0, char, flags, rows, cols, glyph, table_word=2, base=A6):
    pokes = {
        base: bytes([s0]),
        base + 1: bytes([char]),
        base + 2: bytes([flags]),
        TEXT_TABLE: table_word.to_bytes(2, "big"),
        TEXT_TABLE + 2: bytes([rows]),
        TEXT_TABLE + 3: bytes([cols]),
        0x1a45e: SCREEN.to_bytes(4, "big"),
    }
    for i, b in enumerate(glyph):
        pokes[TEXT_TABLE + 4 + i] = bytes([b])
    dest = SCREEN - 0x8000
    pokes.update({dest + i: bytes([0x0f if i % 2 else 0xf0]) for i in range(0x300)})
    return pokes


def _run_entry(trial, char, flags, rows, cols, s0, glyph):
    pokes = _entry_pokes(s0, char, flags, rows, cols, glyph)
    diffs, _ = differential(FUN_14138, {"a6": A6, "_pokes": pokes},
                            lambda lib, buf: lib.g_fun_14138(buf, A6))
    assert not diffs, (f"trial={trial} char={char:#x} flags={flags:#x} rows={rows} "
                       f"cols={cols} s0={s0:#x}\n{report(diffs)}")


def test_fun_14138():
    """Forward/reverse blits, both D7 phases, small glyphs."""
    rng = random.Random(11)
    for trial in range(48):
        char = rng.choice([0x01, 0x02, 0x03, 0x11, 0x41, 0x81, 0x91])
        flags = rng.choice([0x00, 0x01, 0x80, 0xc0])
        rows = rng.randrange(1, 4)
        cols = rng.randrange(1, 4)
        glyph = bytes(rng.randrange(256) for _ in range(2 * 2 * rows * cols + 8))
        _run_entry(trial, char, flags, rows, cols, 0, glyph)


def test_fun_14138_special_char():
    """a6[0] == 'S': the 0x1a49e LSB is consumed and D1 may drop by 8.

    The walk skips a6[0] = 0x53 word-length entries, so the table gets 83 two-byte
    entries of length 2 and the dimensions sit just past them."""
    TBL = TEXT_TABLE
    pokes = {
        A6: bytes([0x53]), A6 + 1: bytes([0x01]), A6 + 2: bytes([0x10]),
        0x1a45e: SCREEN.to_bytes(4, "big"),
        0x1a49e: (1).to_bytes(2, "big"),
    }
    for i in range(83):
        pokes[TBL + 2 * i] = (2).to_bytes(2, "big")
    pokes[TBL + 166] = (2).to_bytes(2, "big")
    pokes[TBL + 168] = bytes([2])       # rows
    pokes[TBL + 169] = bytes([2])       # cols
    for i in range(0x60):
        pokes[TBL + 170 + i] = bytes([(0x10 + i) & 0xff])
    dest = SCREEN - 0x8000
    pokes.update({dest + i: bytes([0x0f if i % 2 else 0xf0]) for i in range(0x300)})
    diffs, _ = differential(FUN_14138, {"a6": A6, "_pokes": pokes},
                            lambda lib, buf: lib.g_fun_14138(buf, A6))
    assert not diffs, report(diffs)


def test_fun_141dc():
    """FUN_000141dc: text entry at A6+1 is blitted, then the masked row copy
    (FUN_0001215e) runs with the A0/D2/D3/D7 FUN_00014138 left behind.

    Flags are kept small so the screen offsets the entry produces stay inside the
    poked bands; the whole point is that the second blit reads what the first wrote.
    """
    rng = random.Random(29)
    for trial in range(48):
        char = rng.choice([0x01, 0x02, 0x11, 0x41, 0x81, 0x91])
        flags = rng.choice([0x00, 0x01, 0x02, 0x80])
        rows = rng.randrange(1, 4)
        cols = rng.randrange(1, 4)
        glyph = bytes(rng.randrange(256) for _ in range(2 * 2 * rows * cols + 8))
        pokes = _entry_pokes(0, char, flags, rows, cols, glyph, base=A6 + 1)
        pokes.update({SCREEN - 0x8000 + i: bytes([0x0f if i % 2 else 0xf0])
                      for i in range(0x400)})
        pokes.update({SCREEN + i: bytes([0xcc if i % 2 else 0x33])
                      for i in range(0x400)})
        diffs, _ = differential(FUN_141DC, {"a6": A6, "_pokes": pokes},
                                lambda lib, buf: lib.g_fun_141dc(buf, A6))
        assert not diffs, (f"trial={trial} char={char:#x} flags={flags:#x} rows={rows} "
                           f"cols={cols}\n{report(diffs)}")


def test_fun_141ea():
    """FUN_000141ea runs FUN_00014138 on A6+1 (then A6 += 3, unused)."""
    pokes = _entry_pokes(0, 0x01, 0x00, 2, 2, bytes(range(0x10, 0x40)))
    # FUN_000141ea's effective entry is A6+1, so shift the three entry bytes.
    pokes[A6 + 1] = bytes([0])
    pokes[A6 + 2] = bytes([0x01])
    pokes[A6 + 3] = bytes([0x00])
    diffs, _ = differential(FUN_141EA, {"a6": A6, "_pokes": pokes},
                            lambda lib, buf: lib.g_fun_141ea(buf, A6))
    assert not diffs, report(diffs)


def test_fun_11014():
    """FUN_00011014 is a bare rts; nothing may move."""
    pokes = {0x30000 + i: bytes([(0x30 + i) & 0xff]) for i in range(0x40)}
    diffs, _ = differential(FUN_11014, {"_pokes": pokes},
                            lambda lib, buf: lib.g_fun_11014(buf))
    assert not diffs, report(diffs)


FUN_11016 = 0x11016
FUN_11020 = 0x11020
harness._lib.g_fun_11016.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_11016.restype = None
harness._lib.g_fun_11020.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_11020.restype = None

OVERLAY_SCREEN = 0x90000


def _overlay_pokes():
    pokes = {0x1a45e: OVERLAY_SCREEN.to_bytes(4, "big"),
             0x1a46a: (0x8000).to_bytes(4, "big")}
    for i in range(0x10000):
        pokes[OVERLAY_SCREEN + i] = bytes([(i * 5 + 0x21) & 0xff])
    return pokes


def test_fun_11016_11020():
    for entry, fn in ((FUN_11016, harness._lib.g_fun_11016),
                      (FUN_11020, harness._lib.g_fun_11020)):
        for d4 in (0, 1, 0x16, 0x17):
            diffs, _ = differential(
                entry, {"d0": 0x4B, "d1": 0x9B, "d4": d4, "_pokes": _overlay_pokes()},
                lambda lib, buf, f=fn, n=d4: f(buf, 0x4B, 0x9B, n))
            assert not diffs, f"entry={entry:#x} d4={d4:#x}\n{report(diffs)}"


FUN_10768 = 0x10768
FUN_122EE = 0x122EE
VBL_COUNTER = 0x1acd4
VBL_SITE = 0x1359a
harness._lib.g_fun_10768.argtypes = [_u8p]
harness._lib.g_fun_10768.restype = None
harness._lib.g_fun_122ee.argtypes = [_u8p]
harness._lib.g_fun_122ee.restype = None


def test_fun_10768():
    diffs, _ = differential(FUN_10768, {"_pokes": {}}, lambda lib, buf: lib.g_fun_10768(buf))
    assert not diffs, report(diffs)


def test_fun_122ee():
    for flag in (0, 1):
        pokes = _overlay_pokes()
        pokes[VBL_COUNTER] = bytes([0x00])
        pokes[0x1a74e] = bytes([flag])
        pokes[0x1acc6] = bytes([0x00])
        diffs, _ = differential(
            FUN_122EE, {"_pokes": pokes},
            lambda lib, buf: lib.g_fun_122ee(buf),
            schedule=[{"pc": VBL_SITE, "nth": 1, "addr": VBL_COUNTER,
                       "width": 1, "value": 0x01}],
            wait_sites=[VBL_SITE])
        assert not diffs, f"flag={flag}\n{report(diffs)}"


def test_fun_122ee_mode87():
    pokes = _overlay_pokes()
    pokes[0x1a74e] = bytes([0x00])
    pokes[0x1acc6] = bytes([0x87])
    diffs, _ = differential(FUN_122EE, {"_pokes": pokes},
                            lambda lib, buf: lib.g_fun_122ee(buf))
    assert not diffs, report(diffs)


PROMPT_WAIT = 0x12292
WF_SITE = 0x136b0
PKT_ADDR = 0x1a44f
BUTTONS = 0x1a456
VBL_COUNTER = 0x1acd4
harness._lib.g_fun_12292.argtypes = [_u8p]
harness._lib.g_fun_12292.restype = None


def test_prompt_wait_mode87():
    """$1acc6 == 0x87: no espera; sale por GEMDOS Cnecin (trap #1/$8) con una tecla staged."""
    pokes = _overlay_pokes() if False else {
        0x1a45e: OVERLAY_SCREEN.to_bytes(4, "big"),
        0x1a46a: (0x8000).to_bytes(4, "big"),
        0x1acc6: bytes([0x87]),
    }
    pokes[0x1a44f] = bytes([0x00])
    pokes[BUTTONS] = bytes([0x00])
    pokes[VBL_COUNTER] = bytes([0x00])
    pokes.update(console_key("a"))
    diffs, _ = differential(PROMPT_WAIT, {"_pokes": pokes},
                            lambda lib, buf: lib.g_fun_12292(buf))
    assert not diffs, report(diffs)


def test_prompt_wait_click_after_release():
    """Primer frame sin clic (suelta) y segundo con clic: termina sin parpadear."""
    pokes = {
        0x1a45e: OVERLAY_SCREEN.to_bytes(4, "big"),
        0x1a46a: (0x8000).to_bytes(4, "big"),
        0x1acc6: bytes([0x00]),
        0x1a44f: bytes([0x00]),
        BUTTONS: bytes([0x00]),
        VBL_COUNTER: bytes([0x05]),        # &0xf != 0 -> no parpadea
        0x1a74e: bytes([0x00]),
    }
    for i in range(0x8000):
        pokes[OVERLAY_SCREEN + i] = bytes([(i * 5 + 0x21) & 0xFF])
    diffs, _ = differential(
        PROMPT_WAIT, {"_pokes": pokes},
        lambda lib, buf: lib.g_fun_12292(buf),
        schedule=[{"pc": WF_SITE, "nth": 1, "addr": PKT_ADDR, "width": 1, "value": 0x00},
                  {"pc": WF_SITE, "nth": 2, "addr": PKT_ADDR, "width": 1, "value": 0x01}],
        wait_sites=[WF_SITE])
    assert not diffs, report(diffs)


FUN_11030 = 0x11030
harness._lib.g_fun_11030.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_11030.restype = None


def test_fun_11030():
    """0x11030: cuerpo de 0x11026 sin avance de entrada (A1 -> [rows][cols])."""
    pokes = _overlay_pokes()
    a1 = 0x40000
    pokes[a1] = bytes([2, 0x10])
    for i in range(0x40):
        pokes[a1 + 2 + i] = bytes([(0x40 + i * 3) & 0xFF])
    for a4off in (0, 0x8000):
        pokes2 = dict(pokes)
        diffs, _ = differential(
            0x11030, {"a1": a1, "d0": 0x4B, "d1": 0x9B, "a4": a4off, "_pokes": pokes2},
            lambda lib, buf, o=a4off: lib.g_fun_11030(buf, a1, 0x4B, 0x9B, o))
        assert not diffs, f"a4={a4off:#x}\n{report(diffs)}"


FUN_16DBC = 0x16DBC
harness._lib.g_fun_16dbc.argtypes = [_u8p, ctypes.POINTER(ctypes.c_uint32),
                                     ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32,
                                     ctypes.POINTER(ctypes.c_uint32)]
harness._lib.g_fun_16dbc.restype = ctypes.c_uint32


def test_fun_16dbc():
    """Setup de elemento de texto: 0x10fec + 0x10f8e + stride + 0x126e0; A0/A5/A6."""
    for d0, d1, d4 in ((0x23, 0x12, 0), (0x10, 0x20, 1), (0x30, 0x30, 5)):
        pokes = {0x1A45E: (0x90000).to_bytes(4, "big")}
        # banco 0x41fb0: d4 entradas vacias [0,2] y la elegida [0,6][rows=1][cols=1][2 B]
        off = 0
        for _ in range(d4):
            pokes[0x41FB0 + off] = bytes([0, 2]); off += 2
        pokes[0x41FB0 + off] = bytes([0, 6, 1, 1, 0xAA, 0x55])
        a6 = ctypes.c_uint32(0x50000)
        a5 = ctypes.c_uint32(0xDEADBEEF)
        diffs, info = differential(
            0x16DBC, {"a6": 0x50000, "d0": d0, "d1": d1, "d4": d4, "_pokes": pokes},
            lambda lib, buf: lib.g_fun_16dbc(buf, ctypes.byref(a6), d0, d1, d4,
                                             ctypes.byref(a5)))
        assert not diffs, f"d0={d0:#x} d1={d1:#x} d4={d4}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a0"], f"d4={d4} a0"
        assert a5.value == info["regs"]["a5"], f"d4={d4} a5"
        assert a6.value == info["regs"]["a6"], f"d4={d4} a6"


FUN_16C00 = 0x16C00
FUN_16C48 = 0x16C48
FUN_16C6C = 0x16C6C
harness._lib.g_fun_16c00.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_fun_16c00.restype = ctypes.c_uint32
harness._lib.g_fun_16c48.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_fun_16c48.restype = ctypes.c_uint32
harness._lib.g_fun_16c6c.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32]
harness._lib.g_fun_16c6c.restype = ctypes.c_uint32

SCREEN_16C = 0x90000
BACK_16C = 0x88000
VBL_SITE_16C = 0x16D24
A6_16C = 0x50000


def _wipe_common():
    pokes = {0x1A45E: SCREEN_16C.to_bytes(4, "big")}
    for i in range(0x3000):
        pokes[SCREEN_16C + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[BACK_16C + i] = bytes([(i * 5 + 7) & 0xFF])
    for i in range(0x200):
        pokes[0xD8F0 + i] = bytes([(i * 7 + 3) & 0xFF])
    return pokes


def _vbl_schedule(n, start=0x10):
    return [{"pc": VBL_SITE_16C, "nth": k, "addr": 0x1ACD4, "width": 1,
             "value": (start + 2 * k) & 0xFF} for k in range(1, n + 1)]


def _anim_table(d4):
    pokes = {}
    off = 0
    for _ in range(d4):
        pokes[0x41FB0 + off] = bytes([0, 2]); off += 2
    pokes[0x41FB0 + off] = bytes([0, 6, 1, 1, 0xAA, 0x55])
    return pokes


def test_fun_16c00():
    """Transicion de texto d4=0x77 + revelado 0x12e2e."""
    pokes = _wipe_common()
    pokes.update(_anim_table(0x77))
    pokes[0x1ACD4] = bytes([0x10])
    diffs, _ = differential(FUN_16C00, {"a6": A6_16C, "_pokes": pokes},
                            lambda lib, buf: lib.g_fun_16c00(buf, A6_16C))
    assert not diffs, report(diffs)


def test_fun_16c6c():
    """Cuerpo del wipe: D7+1 (4 o 3) esperas de VBL."""
    for a6v in (0, 1):
        waits = 3 if a6v else 4
        pokes = _wipe_common()
        pokes[A6_16C] = bytes([a6v])
        pokes[0x1ACD4] = bytes([0x10])
        diffs, info = differential(
            FUN_16C6C, {"a6": A6_16C, "a0": SCREEN_16C, "d2": 2, "d3": 8, "a5": 0,
                        "_pokes": pokes},
            lambda lib, buf: lib.g_fun_16c6c(buf, A6_16C, SCREEN_16C, 2, 8, 0),
            schedule=_vbl_schedule(waits), wait_sites=[VBL_SITE_16C])
        assert not diffs, f"a6[0]={a6v}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], f"a6[0]={a6v}"


def test_fun_16c48():
    """Wipe con setup (0x16dbc) y A6[1] variando d1 y el numero de esperas."""
    for a61 in (0, 1):
        waits = 3 if a61 else 4
        pokes = _wipe_common()
        pokes.update(_anim_table(3))
        pokes[A6_16C] = bytes([0, a61])
        pokes[0x1ACD4] = bytes([0x10])
        diffs, _ = differential(
            FUN_16C48, {"a6": A6_16C, "_pokes": pokes},
            lambda lib, buf: lib.g_fun_16c48(buf, A6_16C),
            schedule=_vbl_schedule(waits), wait_sites=[VBL_SITE_16C])
        assert not diffs, f"A6[1]={a61}\n{report(diffs)}"


FUN_16CA6 = 0x16CA6
FUN_16CE2 = 0x16CE2
FUN_16D3C = 0x16D3C
FUN_16D78 = 0x16D78
for _name in ("g_fun_16ca6", "g_fun_16ce2", "g_fun_16d3c", "g_fun_16d78"):
    getattr(harness._lib, _name).argtypes = [_u8p, ctypes.c_uint32]
    getattr(harness._lib, _name).restype = ctypes.c_uint32


def _wipe_big_pokes():
    pokes = {0x1A45E: SCREEN_16C.to_bytes(4, "big")}
    for i in range(0x12000):
        pokes[0x88000 + i] = bytes([(i * 3 + 1) & 0xFF])
    for i in range(0x200):
        pokes[0xD8F0 + i] = bytes([(i * 7 + 3) & 0xFF])
    return pokes


def _wipe_run(fn, d4, waits, name):
    pokes = _wipe_big_pokes()
    pokes.update(_anim_table(d4))
    pokes[0x1ACD4] = bytes([0x10])
    # 0x16ce2 acumula ~68 pasadas de copia+OR (~>200k instrucciones): sube el tope.
    diffs, _ = differential(fn, {"a6": A6_16C, "_pokes": pokes},
                            lambda lib, buf: getattr(lib, name)(buf, A6_16C),
                            schedule=_vbl_schedule(waits), wait_sites=[VBL_SITE_16C],
                            max_insns=3_000_000)
    assert not diffs, f"{name}\n{report(diffs)}"


def test_fun_16ca6():
    _wipe_run(FUN_16CA6, 0x76, 34, "g_fun_16ca6")


def test_fun_16ce2():
    _wipe_run(FUN_16CE2, 0x76, 68, "g_fun_16ce2")


def test_fun_16d3c():
    _wipe_run(FUN_16D3C, 0x60, 16, "g_fun_16d3c")


def test_fun_16d78():
    _wipe_run(FUN_16D78, 0x60, 15, "g_fun_16d78")


FUN_16B9C = 0x16B9C
harness._lib.g_fun_16b9c.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_fun_16b9c.restype = ctypes.c_uint32


def _anim_table_dos():
    """Tabla 0x41fb0 valida para d4=3 y d4=0x77 (dos 16dbc en la misma pasada).
    Las longitudes son words 0x0002; al poner el par filas/cols en la posicion de
    aterrizaje de d4=3 su word pasa a ser 0x0101 y el walk de 0x77 lo cruza: se
    simula el walk para saber donde aterriza cada uno."""
    pokes = {}
    for i in range(0x200):
        pokes[0x41FB0 + 2 * i] = bytes([0, 2])

    def getb(a):
        v = pokes.get(a)
        if v is not None:
            return v[0]
        v = pokes.get(a - 1)
        if v is not None and len(v) > 1:
            return v[1]
        return 0

    def walk(d4):
        a1 = 0x41FB0
        n = d4 - 1
        while n >= 0:
            a1 += (getb(a1) << 8) | getb(a1 + 1)
            n -= 1
        return a1 + 2

    land3 = walk(3)
    pokes[land3] = bytes([1, 1])
    land77 = walk(0x77)
    pokes[land77] = bytes([1, 1])
    for j in range(0x40):
        pokes[land3 + 2 + j] = bytes([(j * 7 + 3) & 0xFF])
        pokes[land77 + 2 + j] = bytes([(j * 11 + 5) & 0xFF])
    return pokes


def test_fun_16b9c():
    """Wipe grande: dos 16dbc + 72 pasadas de 16d1e/16b76/16b2e y prompt."""
    pokes = _wipe_common()
    pokes.update(_anim_table_dos())
    pokes[0x1ACC6] = bytes([0x87])          # prompt_wait -> Cnecin
    pokes[0x1ACD4] = bytes([0x10])
    pokes[A6_16C] = bytes([0x00, 0x00])
    pokes.update(console_key("a"))
    diffs, _ = differential(
        FUN_16B9C, {"a6": A6_16C, "_pokes": pokes},
        lambda lib, buf: lib.g_fun_16b9c(buf, A6_16C),
        schedule=_vbl_schedule(0x48, start=0x10), wait_sites=[VBL_SITE_16C],
        max_insns=20_000_000)
    assert not diffs, report(diffs)


FUN_VM = 0x10630
harness._lib.g_fun_10630.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_fun_10630.restype = ctypes.c_uint32


def _vm_wipe_cases():
    """(byte, glue, pokes, waits, neto A6, max_insns) por comando de wipe.

    Fixtures identicas a las de los tests directos (0x16dbc hace addq.l #1,a6):
    69 necesita la tabla de dos entradas + prompt, 101 depende de A6[1] = 0."""
    casos = []
    p = _wipe_big_pokes(); p.update(_anim_table(0x60)); p[0x1ACD4] = bytes([0x10])
    casos.append((70, "g_fun_16d78", p, 15, 1, 3_000_000))
    p = _wipe_big_pokes(); p.update(_anim_table(0x76)); p[0x1ACD4] = bytes([0x10])
    casos.append((71, "g_fun_16ce2", p, 68, 1, 3_000_000))
    p = _wipe_big_pokes(); p.update(_anim_table(0x76)); p[0x1ACD4] = bytes([0x10])
    casos.append((72, "g_fun_16ca6", p, 34, 1, 3_000_000))
    p = _wipe_big_pokes(); p.update(_anim_table(0x60)); p[0x1ACD4] = bytes([0x10])
    casos.append((73, "g_fun_16d3c", p, 16, 1, 3_000_000))
    p = _wipe_common(); p.update(_anim_table(3)); p[0x1ACD4] = bytes([0x10])
    casos.append((101, "g_fun_16c48", p, 4, 2, 3_000_000))
    p = _wipe_common(); p.update(_anim_table(0x77))
    casos.append((102, "g_fun_16c00", p, 0, 1, 3_000_000))
    p = _wipe_common(); p.update(_anim_table_dos())
    p[0x1ACC6] = bytes([0x87])
    p.update(console_key("a"))
    casos.append((69, "g_fun_16b9c", p, 0x48, 1, 20_000_000))
    return casos


def test_vm_0x10630_avance_a6_en_wipes():
    """Regresion: el VM (0x10630) debe dejar A6 donde el original tras cada wipe.

    El avance sale de 0x16dbc (addq.l #1,a6) y de 0x16c6c: 69 -> +1 (16dbc x2
    con un subq -1 en medio), 70-73/102 -> +1, 101 -> +2. `byte_handler`
    devolvia -1 para 69 y +1 para 101, desviando el guion."""
    for byte, name, pokes, waits, net, max_insns in _vm_wipe_cases():
        pokes[A6_16C] = bytes([byte, 0, 0])     # comando + byte de stop en A6+1/+2
        sched = _vbl_schedule(waits, start=0x10) if waits else None
        diffs, info = differential(
            FUN_VM, {"a6": A6_16C, "_pokes": pokes},
            lambda lib, buf: lib.g_fun_10630(buf, A6_16C),
            schedule=sched, wait_sites=[VBL_SITE_16C] if sched else None,
            max_insns=max_insns)
        assert not diffs, f"byte {byte} ({name})\n{report(diffs)}"
        assert info["ret"] == A6_16C + net, \
            f"byte {byte} ({name}): C {info['ret']:#x} != esperado {A6_16C + net:#x}"
        assert info["ret"] == info["regs"]["a6"], \
            f"byte {byte} ({name}): C {info['ret']:#x} != oraculo {info['regs']['a6']:#x}"
