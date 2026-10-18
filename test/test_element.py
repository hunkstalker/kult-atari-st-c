"""Differential tests for the element blit helpers (element.c).

FUN_000126e0 @ 0x126e0: compone words rotados en el descriptor 0xd8f0.
FUN_0001276c @ 0x1276c: relleno de patron directo en A0 (espera VBL).
FUN_00011ece @ 0x11ece / FUN_00011ee8 @ 0x11ee8: bucles sobre 0x12466.
"""
import ctypes

import harness
from harness import differential, report

_u8p = ctypes.POINTER(ctypes.c_uint8)

DESC = 0xd8f0
SRC = 0x40000
STRIDE = 0x1a534
VBL_COUNTER = 0x1acd4
VBL_SITE = 0x1359a


def test_fun_126e0():
    harness._lib.grid_fill.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                         ctypes.c_uint32, ctypes.c_uint32]
    harness._lib.grid_fill.restype = None
    for d2 in (1, 2, 4, 5, 8):
        for d3 in (1, 2, 3):
            for d7 in (0, 1, 4, 8, 12, 15):
                for stride in (0x40, 0xa0):
                    pokes = {STRIDE: stride.to_bytes(4, "big")}
                    for i in range(0x400):
                        pokes[SRC + i] = bytes([(i * 7 + 3) & 0xFF])
                    for i in range(0x800):
                        pokes[DESC + i] = bytes([(i * 13 + 5) & 0xFF])
                    diffs, _ = differential(
                        ENTRY_126E0, {"d2": d2, "d3": d3, "d7": d7, "a1": SRC,
                                      "_pokes": pokes},
                        lambda lib, buf, w=d2, h=d3, p=d7:
                        lib.grid_fill(buf, w, h, p, SRC))
                    assert not diffs, (
                        f"d2={d2} d3={d3} d7={d7} stride={stride:#x}\n{report(diffs)}")


ENTRY_126E0 = 0x126E0


def test_fun_1276c():
    harness._lib.pattern_fill.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                         ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]
    harness._lib.pattern_fill.restype = None
    DEST = 0x90000
    for d2 in (1, 2, 4, 5):
        for d3 in (1, 2, 3):
            for d7 in (0, 1, 4, 8, 12, 15):
                for d6 in (0x0000, 0x1234, 0xffff, 0x0f0f):
                    pokes = {VBL_COUNTER: bytes([0x00])}
                    for i in range(0x800):
                        pokes[DEST + i] = bytes([(i * 11 + 7) & 0xFF])
                    diffs, _ = differential(
                        ENTRY_1276C, {"d2": d2, "d3": d3, "d7": d7, "d6": d6,
                                      "a0": DEST, "_pokes": pokes},
                        lambda lib, buf, w=d2, h=d3, p=d7, pat=d6:
                        lib.pattern_fill(buf, w, h, p, pat, DEST),
                        schedule=[{"pc": VBL_SITE, "nth": 1, "addr": VBL_COUNTER,
                                   "width": 1, "value": 0x01}],
                        wait_sites=[VBL_SITE])
                    assert not diffs, (
                        f"d2={d2} d3={d3} d7={d7} d6={d6:#x}\n{report(diffs)}")


ENTRY_1276C = 0x1276C


def _loop_pokes(left, right, groups, src_base, dest_base):
    pokes = {0x1a538: left.to_bytes(2, "big"),
             0x1a53a: right.to_bytes(2, "big"),
             0x1a53c: groups.to_bytes(2, "big"),
             VBL_COUNTER: bytes([0x00])}
    for i in range(0x1000):
        pokes[src_base + i] = bytes([(i * 7 + 3) & 0xFF])
        pokes[dest_base + i] = bytes([(i * 13 + 5) & 0xFF])
    return pokes


def _vbl_schedule(n):
    return [{"pc": VBL_SITE, "nth": k, "addr": VBL_COUNTER, "width": 1, "value": k}
            for k in range(1, n + 1)]


def test_fun_11ece():
    harness._lib.row_blit_a.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                         ctypes.c_uint32]
    harness._lib.row_blit_a.restype = None
    DEST = 0x90000
    SRCB = 0xa0000
    for d1 in (0, 1, 2):
        for groups in (2, 3):
            pokes = _loop_pokes(0xf000, 0x000f, groups, SRCB, DEST)
            diffs, _ = differential(
                ENTRY_11ECE, {"d1": d1, "a0": DEST, "a1": SRCB, "_pokes": pokes},
                lambda lib, buf, n=d1: lib.row_blit_a(buf, n, DEST, SRCB),
                schedule=_vbl_schedule(d1 + 1), wait_sites=[VBL_SITE])
            assert not diffs, f"d1={d1} groups={groups}\n{report(diffs)}"


ENTRY_11ECE = 0x11ECE


def test_fun_11ee8():
    harness._lib.row_blit_b.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                         ctypes.c_uint32]
    harness._lib.row_blit_b.restype = None
    DEST = 0x90000
    SRCB = 0xa0000
    for d1 in (0, 1, 2):
        for groups in (2, 3):
            pokes = _loop_pokes(0xf000, 0x000f, groups, SRCB, DEST - 0x20)
            diffs, _ = differential(
                ENTRY_11EE8, {"d1": d1, "a0": DEST, "a1": SRCB, "_pokes": pokes},
                lambda lib, buf, n=d1: lib.row_blit_b(buf, n, DEST, SRCB),
                schedule=_vbl_schedule(d1 + 1), wait_sites=[VBL_SITE])
            assert not diffs, f"d1={d1} groups={groups}\n{report(diffs)}"


ENTRY_11EE8 = 0x11EE8


FUN_1285A = 0x1285A
_u32p = ctypes.POINTER(ctypes.c_uint32)
harness._lib.elem_class_find.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32, ctypes.c_uint32, _u32p]
harness._lib.elem_class_find.restype = ctypes.c_int


def _elem_class_pokes(match):
    pokes = {0x1a772: (0x40000).to_bytes(4, "big"), 0x40003: bytes([match & 0xf8])}
    for i in range(24):
        base = 0x19e19 + i * 6
        pokes[base] = bytes([(0x10 + i) & 0xfc])
        for k in range(1, 6):
            pokes[base + k] = bytes([(i * 7 + k) & 0xff])
    return pokes


def test_fun_1285a():
    for match, entry in ((0x30, 3), (0x10, 0), (0x1c, 23)):
        pokes = _elem_class_pokes(match)
        entry_base = 0x19e19 + entry * 6
        pokes[entry_base] = bytes([match & 0xf8])
        o = (ctypes.c_uint32 * 4)()
        diffs, info = differential(
            FUN_1285A, {"d0": 0x11223344, "d1": 0x55667788, "d2": 0x99AABBCC,
                        "d3": 0xDDEEFF00, "_pokes": pokes},
            lambda lib, buf, p=o: lib.elem_class_find(buf, 0x11223344, 0x55667788,
                                                  0x99AABBCC, 0xDDEEFF00, p))
        assert not diffs, f"match={match:#x} entry={entry}\n{report(diffs)}"
        for i, reg in enumerate(("d0", "d1", "d2", "d3")):
            assert o[i] == info["regs"][reg], f"match={match:#x} out[{i}]({reg})"


def test_fun_1285a_not_found():
    pokes = _elem_class_pokes(0x30)
    for i in range(24):
        pokes[0x19e19 + i * 6] = bytes([0x80])       # ninguna coincide con 0x30
    o = (ctypes.c_uint32 * 4)()
    diffs, info = differential(
        FUN_1285A, {"d0": 0x11223344, "d1": 0x55667788, "d2": 0x99AABBCC,
                    "d3": 0xDDEEFF00, "_pokes": pokes},
        lambda lib, buf, p=o: lib.elem_class_find(buf, 0x11223344, 0x55667788,
                                              0x99AABBCC, 0xDDEEFF00, p))
    assert not diffs, report(diffs)
    for i, reg in enumerate(("d0", "d1", "d2", "d3")):
        assert o[i] == info["regs"][reg], f"out[{i}]({reg})"


FUN_1296C = 0x1296C
harness._lib.elem_bitmap_build.argtypes = [_u8p, _u32p]
harness._lib.elem_bitmap_build.restype = None


def test_fun_1296c():
    for w, h in ((3, 3), (4, 4), (5, 3), (4, 5)):
        for idx in (0, 1, 2):
            base = 0x1a252 + idx * 10
            pokes = {
                0x50000: bytes([idx]),
                base: bytes([w]),
                base + 1: bytes([h]),
                base + 2: (0x1111).to_bytes(2, "big"),
                base + 4: (0x2222).to_bytes(2, "big"),
                base + 6: (0x3333).to_bytes(2, "big"),
                base + 8: (0x4444).to_bytes(2, "big"),
            }
            a5 = ctypes.c_uint32(0x50000)
            diffs, info = differential(
                FUN_1296C, {"a5": 0x50000, "_pokes": pokes},
                lambda lib, buf, p=a5: lib.elem_bitmap_build(buf, ctypes.byref(p)))
            assert not diffs, f"w={w} h={h} idx={idx}\n{report(diffs)}"
            assert a5.value == info["regs"]["a5"], f"w={w} h={h} idx={idx} a5"


FUN_129C8 = 0x129C8
harness._lib.elem_list_build.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.elem_list_build.restype = ctypes.c_uint32


def _elem_list_pokes(w, h, b2):
    pokes = {
        0x2751e: bytes([5, 0, 0, 0x00, b2]),
        0x1a252: bytes([w]),
        0x1a253: bytes([h]),
        0x1a254: (0x1111).to_bytes(2, "big"),
        0x1a256: (0x2222).to_bytes(2, "big"),
        0x1a258: (0x3333).to_bytes(2, "big"),
        0x1a25a: (0x4444).to_bytes(2, "big"),
        0x31072: (0x10).to_bytes(2, "big"),
        0x31074: bytes([2, 2]),
    }
    for i in range(0x100):
        pokes[0x31076 + i] = bytes([(i * 7 + 3) & 0xFF])
    for i in range(0x1000):
        pokes[0x3b1ae + i] = bytes([(i * 5 + 1) & 0xFF])
    return pokes


def test_fun_129c8():
    for w, h in ((3, 3), (4, 3), (3, 4)):
        for b2 in (0x00, 0x40, 0x80, 0xC0):
            pokes = _elem_list_pokes(w, h, b2)
            diffs, info = differential(FUN_129C8, {"d1": 0, "_pokes": pokes},
                                       lambda lib, buf: lib.elem_list_build(buf, 0))
            assert not diffs, f"w={w} h={h} b2={b2:#x}\n{report(diffs)}"
            assert info["ret"] == info["regs"]["a1"], f"w={w} h={h} b2={b2:#x} a1"


FUN_128A4 = 0x128A4
harness._lib.func_128a4.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_128a4.restype = ctypes.c_uint32


def test_fun_128a4_direct():
    """Rama sin 0xff: x=A6[2], y=A6[3], construye bitmap y crea entrada de icono."""
    SCREEN_A = 0x80000
    for x, y in ((8, 4), (16, 3), (4, 10)):
        pokes = _elem_list_pokes(4, 3, 0x00)
        pokes[0x50000] = bytes([0x00, 1, x, y])
        pokes[0x1a45e] = SCREEN_A.to_bytes(4, "big")
        pokes[0x1a851] = bytes([0x22])
        for i in range(10 * 0x10):
            pokes[0x1a850 + i] = bytes([0x00])
        for i in range(0x800):
            pokes[SCREEN_A + i] = bytes([(i * 3 + 1) & 0xFF])
        diffs, info = differential(FUN_128A4, {"a6": 0x50000, "_pokes": pokes},
                                   lambda lib, buf: lib.func_128a4(buf, 0x50000))
        assert not diffs, f"x={x} y={y}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], f"x={x} y={y} a6"


def test_fun_128a4_ff():
    """Rama 0xff: usa 0x1285a para resolver la clase (marco 0x18d28).

    Las salidas tempranas (flag 0x1ad0a puesto, o clase no encontrada) terminan en
    0x1294c, que hace `addq.l #4,a7; rts` (el llamante real deja un long extra en la
    pila): el harness no modela ese pop, asi que se verifica con stop_pc en 0x1294c.
    """
    SCREEN_A = 0x80000
    STOP_EARLY = 0x1294C
    for skip_18d28, found, early in ((True, True, True), (False, False, True),
                                     (False, True, False)):
        pokes = _elem_list_pokes(4, 3, 0x00)
        pokes[0x50000] = bytes([0x00, 0xFF])
        pokes[0x1a45e] = SCREEN_A.to_bytes(4, "big")
        pokes[0x1a851] = bytes([0x22])
        pokes[0x1ad0a] = bytes([0x01 if skip_18d28 else 0x00])
        pokes[0x1a81e] = (0x60000).to_bytes(4, "big")
        pokes[0x60000] = (0).to_bytes(4, "big")
        pokes[0x1a81a] = (0).to_bytes(4, "big")
        pokes[0x1acc9] = bytes([0x11])
        # entrada de clase en 0x19e19 con clase 0x30 y d1=1
        pokes[0x1a772] = (0x40000).to_bytes(4, "big")
        pokes[0x40003] = bytes([0x37])
        for i in range(24):
            base = 0x19e19 + i * 6
            pokes[base] = bytes([0x80])
            for k in range(1, 6):
                pokes[base + k] = bytes([(i + k) & 0xFF])
        if found:
            pokes[0x19e19] = bytes([0x30, 1, 2, 3, 4, 5])
        for i in range(10 * 0x10):
            pokes[0x1a850 + i] = bytes([0x00])
        for i in range(0x800):
            pokes[SCREEN_A + i] = bytes([(i * 3 + 1) & 0xFF])
        if early:
            diffs, info = differential(FUN_128A4, {"a6": 0x50000, "_pokes": pokes},
                                       lambda lib, buf: lib.func_128a4(buf, 0x50000),
                                       stop_pc=STOP_EARLY)
        else:
            diffs, info = differential(FUN_128A4, {"a6": 0x50000, "_pokes": pokes},
                                       lambda lib, buf: lib.func_128a4(buf, 0x50000))
        assert not diffs, (f"skip={skip_18d28} found={found}\n{report(diffs)}")
        assert info["ret"] == info["regs"]["a6"], f"skip={skip_18d28} found={found} a6"


FUN_12A4A = 0x12A4A
harness._lib.elem_bitmap_build2.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.elem_bitmap_build2.restype = ctypes.c_uint32


def _groups_pokes(groups, tv3=2, tv2=2):
    """groups = lista de grupos; cada grupo es una lista de bloques (n, b1, b2).
    Cada grupo se codifica [len=1+3n][bloques...] y van consecutivos en 0x23e9c."""
    pokes = {}
    addr = 0x23e9c
    for chunks in groups:
        pokes[addr] = bytes([1 + 3 * len(chunks)])
        for i, (n, b1, b2) in enumerate(chunks):
            base = addr + 1 + i * 3
            pokes[base] = bytes([n])
            pokes[base + 1] = bytes([b1])
            pokes[base + 2] = bytes([b2])
        addr += 1 + 3 * len(chunks)
    pokes[0x41fb0] = (0x10).to_bytes(2, "big")
    pokes[0x41fb2] = bytes([tv3, tv2])
    for i in range(0x200):
        pokes[0x41fb4 + i] = bytes([(i * 7 + 3) & 0xFF])
    for i in range(0x2000):
        pokes[0x3b1ac + i] = bytes([(i * 5 + 1) & 0xFF])
    return pokes


def test_fun_12a4a():
    cases = (
        (((0, 0, 0),), ((0, 0, 0),)),
        (((0, 0, 0), (0, 0, 0)), ((0, 0, 0),)),
        (((0, 0, 0x40),), ((0, 0, 0),)),
        (((0, 0, 0x80),), ((0, 0, 0),)),
        (((0, 0, 0xC0),), ((0, 0, 0),)),
        (((0, 0x01, 0x00),), ((0, 0, 0),)),
    )
    for groups in cases:
        for d1 in (0, 1):
            pokes = _groups_pokes(groups)
            diffs, info = differential(
                FUN_12A4A, {"d1": d1, "_pokes": pokes},
                lambda lib, buf, n=d1: lib.elem_bitmap_build2(buf, n))
            assert not diffs, f"groups={groups} d1={d1}\n{report(diffs)}"
            assert info["ret"] == info["regs"]["a1"], f"groups={groups} d1={d1} a1"


FUN_12D96 = 0x12D96
FUN_12DD8 = 0x12DD8
harness._lib.elem_blit_pair.argtypes = [_u8p]
harness._lib.elem_blit_pair.restype = None
harness._lib.elem_blit_pair_vbl.argtypes = [_u8p]
harness._lib.elem_blit_pair_vbl.restype = None
SCREEN_A = 0x80000


def _two_entries_pokes():
    pokes = _groups_pokes((((0, 0, 0),), ((0, 0, 0),)))
    pokes[0x1a45e] = SCREEN_A.to_bytes(4, "big")
    for i in range(0x10000):
        pokes[SCREEN_A + i] = bytes([(i * 3 + 1) & 0xFF])
    return pokes


def test_fun_12d96():
    diffs, _ = differential(FUN_12D96, {"_pokes": _two_entries_pokes()},
                            lambda lib, buf: lib.elem_blit_pair(buf))
    assert not diffs, report(diffs)


def test_fun_12dd8():
    pokes = _two_entries_pokes()
    pokes[0x1acd4] = bytes([0x00])
    diffs, _ = differential(
        FUN_12DD8, {"_pokes": pokes},
        lambda lib, buf: lib.elem_blit_pair_vbl(buf),
        schedule=[{"pc": 0x1359a, "nth": 1, "addr": 0x1acd4, "width": 1, "value": 1},
                  {"pc": 0x1359a, "nth": 2, "addr": 0x1acd4, "width": 1, "value": 2}],
        wait_sites=[0x1359a])
    assert not diffs, report(diffs)


FUN_12EA2 = 0x12EA2
FUN_12E08 = 0x12E08
harness._lib.reveal_box_b.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.reveal_box_b.restype = ctypes.c_uint32
harness._lib.reveal_box_a.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.reveal_box_a.restype = ctypes.c_uint32


def test_fun_12ea2():
    for box in ((2, 6, 3, 8), (0, 4, 0, 4)):
        pokes = {
            0x1ab40: bytes([box[0]]), 0x1ab41: bytes([box[1]]),
            0x1ab42: bytes([box[2]]), 0x1ab43: bytes([box[3]]),
            0x1a45e: SCREEN_A.to_bytes(4, "big"),
        }
        for i in range(0x4000):
            pokes[SCREEN_A + i] = bytes([(i * 3 + 1) & 0xFF])
        diffs, info = differential(FUN_12EA2, {"a6": 0x50000, "_pokes": pokes},
                                   lambda lib, buf: lib.reveal_box_b(buf, 0x50000),
                                   schedule=[], wait_sites=[])
        assert not diffs, f"box={box}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], f"box={box} a6"


def _local_hint_table():
    pokes = {0x1a851: bytes([0x11]), 0x1ad0a: bytes([0x00]),
             0x1a45e: SCREEN_A.to_bytes(4, "big")}
    for i in range(10):
        base = 0x1a850 + i * 0x10
        pokes[base] = ((i + 1) & 0xffff).to_bytes(2, "big")
        pokes[base + 2] = (SCREEN_A + i * 0x10).to_bytes(4, "big")
        for k, v in enumerate((1, 2, 3, 4, 5)):
            pokes[base + 6 + k * 2] = v.to_bytes(2, "big")
    return pokes


def test_fun_12e08():
    for skip in (0, 1):
        pokes = _local_hint_table()
        pokes.update(_groups_pokes((((0, 0, 0),), ((0, 0, 0),))))
        pokes[0x1a570] = bytes([skip])
        for i in range(0x10000):
            pokes[SCREEN_A + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x30000] = bytes([0, 1])
        diffs, info = differential(FUN_12E08, {"a6": 0x30000, "_pokes": pokes},
                                   lambda lib, buf: lib.reveal_box_a(buf, 0x30000))
        assert not diffs, f"skip={skip}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], f"skip={skip} a6"


FUN_14748 = 0x14748


def test_fun_14748():
    """Segun 0x1acc6 lanza 0x12d96 (0x09/0x66) o 0x12dd8 (0x5f/0x67)."""
    harness._lib.elem_mode_blit.argtypes = [_u8p]
    harness._lib.elem_mode_blit.restype = None
    for mode, waits in ((0x09, 0), (0x66, 0), (0x5F, 2), (0x67, 2), (0x00, 0)):
        pokes = _two_entries_pokes()
        pokes[0x1acc6] = bytes([mode])
        pokes[0x1acd4] = bytes([0x00])
        sched = [{"pc": 0x1359a, "nth": k, "addr": 0x1acd4, "width": 1, "value": k}
                 for k in range(1, waits + 1)]
        diffs, _ = differential(FUN_14748, {"_pokes": pokes},
                                lambda lib, buf: lib.elem_mode_blit(buf),
                                schedule=sched, wait_sites=[0x1359a] if waits else None)
        assert not diffs, f"mode={mode:#x}\n{report(diffs)}"


FUN_129D6 = 0x129D6
harness._lib.text_block_build.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.text_block_build.restype = None


def test_fun_129d6():
    pokes = {
        0x1A534: (0x28).to_bytes(4, "big"),
        0x43000: bytes([0x00, 0x10, 0x00, 0x01, 0x90, 0x80]),
        0x43100: bytes([0x00, 0x20, 0x40]),
    }
    pokes[0x31072] = bytes([0x00, 0x04, 0x02, 0x03, 0x11, 0x22])
    pokes[0x31078] = bytes([0x02, 0x03])
    for i in range(0x100):
        pokes[0x3B1B0 + i] = bytes([(i * 5 + 1) & 0xFF])
    diffs, _ = differential(
        FUN_129D6, {"a5": 0x43000, "a4": 0x43003, "_pokes": pokes},
        lambda lib, buf: lib.text_block_build(buf, 0x43000, 0x43003))
    assert not diffs, report(diffs)
    diffs, _ = differential(
        FUN_129D6, {"a5": 0x43003, "a4": 0x43009, "_pokes": pokes},
        lambda lib, buf: lib.text_block_build(buf, 0x43003, 0x43009))
    assert not diffs, report(diffs)
