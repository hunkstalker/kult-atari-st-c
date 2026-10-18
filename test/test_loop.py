"""Tests de las piezas del bucle principal (loop.c): busqueda de objeto por clase."""
import ctypes

import harness
from harness import console_key, console_keys, differential, report, stage_files

_u8p = ctypes.POINTER(ctypes.c_uint8)
OBJ_BEGIN = 0x30000
OBJ_N = 6

harness._lib.func_1778e.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.POINTER(ctypes.c_uint32),
                                     ctypes.POINTER(ctypes.c_uint32),
                                     ctypes.POINTER(ctypes.c_uint32)]
harness._lib.func_1778e.restype = ctypes.c_int
harness._lib.func_177bc.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.POINTER(ctypes.c_uint32)]
harness._lib.func_177bc.restype = ctypes.c_int


def _obj_pokes(classes):
    pokes = {0x1a7ca: OBJ_BEGIN.to_bytes(4, "big"),
             0x1a7ce: (OBJ_BEGIN + OBJ_N * 8).to_bytes(4, "big")}
    for i in range(OBJ_N):
        pokes[OBJ_BEGIN + i * 8 + 4] = bytes([classes[i] & 0x3f])
    return pokes


def test_fun_1778e():
    classes = [0x10, 0x11, 0x12, 0x13, 0x14, 0x15]
    for d0 in (0x10, 0x12, 0x15, 0x16):
        pokes = _obj_pokes(classes)
        o0 = ctypes.c_uint32()
        o1 = ctypes.c_uint32()
        o2 = ctypes.c_uint32()
        diffs, info = differential(
            0x1778E, {"d0": d0, "d2": 0, "_pokes": pokes},
            lambda lib, buf: lib.func_1778e(buf, d0, 0, ctypes.byref(o0),
                                             ctypes.byref(o1), ctypes.byref(o2)))
        assert not diffs, f"d0={d0:#x}\n{report(diffs)}"
        assert o0.value == info["regs"]["a0"], f"d0={d0:#x} a0"
        assert o1.value == info["regs"]["d1"], f"d0={d0:#x} d1"
        assert o2.value == info["regs"]["d2"], f"d0={d0:#x} d2"


def test_fun_177bc():
    classes = [0x10, 0x11, 0x12, 0x13, 0x14, 0x15]
    for d0, byte3 in ((2, 2), (0, 0), (5, 3)):
        pokes = _obj_pokes(classes)
        for i in range(0x40):
            pokes[0x1aa72 + i] = bytes([i])
        pokes[0x1aa72 + d0 + 3] = bytes([byte3])
        o0 = ctypes.c_uint32()
        diffs, info = differential(
            0x177BC, {"d0": d0, "d2": 0, "_pokes": pokes},
            lambda lib, buf, n=d0: lib.func_177bc(buf, n, 0, ctypes.byref(o0)))
        assert not diffs, f"d0={d0} byte3={byte3}\n{report(diffs)}"
        assert o0.value == info["regs"]["d0"], f"d0={d0} byte3={byte3} d0"


harness._lib.func_1103e.argtypes = [_u8p] + [ctypes.c_uint32] * 5
harness._lib.func_1103e.restype = None
harness._lib.func_15ac8.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_15ac8.restype = None


def test_fun_1103e():
    for d2, d3, d7 in ((4, 3, 0), (8, 2, 4)):
        pokes = {}
        for i in range(0x4000):
            pokes[0x90000 + i] = bytes([(i * 3 + 1) & 0xFF])
            pokes[0x40000 + i] = bytes([(i * 7 + 3) & 0xFF])
        diffs, _ = differential(
            0x1103E, {"a0": 0x90000, "a1": 0x40000, "d2": d2, "d3": d3, "d7": d7,
                      "_pokes": pokes},
            lambda lib, buf, a=d2, b=d3, c=d7: lib.func_1103e(buf, 0x90000, 0x40000,
                                                              a, b, c))
        assert not diffs, f"d2={d2} d3={d3} d7={d7}\n{report(diffs)}"


def test_fun_15ac8():
    for b2 in (0x00, 0x80):
        pokes = {
            0x1a4c6: (0xD8F0).to_bytes(4, "big"),
            0x27fe2: bytes([6, 0x08, 0x03, 0x00, 0x00, b2]),
            0x28AD2: (0x10).to_bytes(2, "big"),
            0x28AD4: bytes([2, 2]),
        }
        for i in range(0x200):
            pokes[0x28AD6 + i] = bytes([(i * 7 + 3) & 0xFF])
        for i in range(0x800):
            pokes[0xD8F0 + i] = bytes([(i * 5 + 1) & 0xFF])
        diffs, _ = differential(0x15AC8, {"d0": 0, "_pokes": pokes},
                                lambda lib, buf: lib.func_15ac8(buf, 0))
        assert not diffs, f"b2={b2:#x}\n{report(diffs)}"


def test_fun_15ac8_multiblock():
    """Entrada con DOS bloques distintos: el original recalcula por bloque el
    bitmap (0x10f8e), tamano (d3/d2) y offset (0x15b02). Calcularlo una sola vez
    fuera del bucle solo pintaba el primer bloque (sprites animados truncados)."""
    pokes = {
        0x1a4c6: (0xD8F0).to_bytes(4, "big"),
        # [len=9][hdr0=0x08][hdr1=0x03][blk0: n=0 off=0][blk1: n=1 off=4]
        0x27FE2: bytes([9, 0x08, 0x03, 0x00, 0x00, 0x00, 0x01, 0x04, 0x00]),
        # tabla de bitmaps 0x28ad2: dos entradas de distinto tamano
        0x28AD2: bytes([0x00, 0x08]),        # entry0 len=8
        0x28AD4: bytes([2, 1]),              # d3=2, d2=1
        0x28AD6: bytes([0x11, 0x22, 0x33, 0x44]),
        0x28ADA: bytes([0x00, 0x08]),        # entry1 len=8 (0x28ada = 0x28ad2+8)
        0x28ADC: bytes([1, 2]),              # d3=1, d2=2
        0x28ADE: bytes([0x55, 0x66, 0x77, 0x88]),
    }
    for i in range(0x800):
        pokes[0xD8F0 + i] = bytes([(i * 5 + 1) & 0xFF])
    diffs, _ = differential(0x15AC8, {"d0": 0, "_pokes": pokes},
                            lambda lib, buf: lib.func_15ac8(buf, 0))
    assert not diffs, report(diffs)


harness._lib.func_16636.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.POINTER(ctypes.c_uint32)]
harness._lib.func_16636.restype = ctypes.c_uint32


def _anim_pokes(cls):
    pokes = {
        0x1a45e: (0x90000).to_bytes(4, "big"),
        0x1a4c6: (0xD8F0).to_bytes(4, "big"),
        0x1a57D: bytes([0, 0, 0, 0, 0, 0, 0, 0]),
        0x27FE2: bytes([6, 4, 3, 0x00, 0x00, 0x00]),
        0x28AD2: (0x10).to_bytes(2, "big"),
        0x28AD4: bytes([2, 2]),
        0x30000: bytes([8, 0, 4, 0]),
    }
    for i in range(0x29):
        base = 0x1AA72 + i * 5
        pokes[base] = bytes([0])
        pokes[base + 1] = bytes([i & 0x7])
        pokes[base + 2] = bytes([0])
        pokes[base + 3] = bytes([0])
        pokes[base + 4] = bytes([0])
    pokes[0x1AA72 + 1] = bytes([cls & 0xF])
    for i in range(0x200):
        pokes[0x28AD6 + i] = bytes([(i * 7 + 3) & 0xFF])
    for i in range(0x1000):
        pokes[0xD8F0 + i] = bytes([(i * 5 + 1) & 0xFF])
    for i in range(0x10000):
        pokes[0x90000 + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x88000 + i] = bytes([(i * 11 + 7) & 0xFF])
    return pokes


def test_fun_16636():
    # 0x166da/0x166ec NO usan el d0 de retorno de 0x16636 (calculan el flag antes),
    # asi que se verifica por la memoria (banco + blit al back buffer).
    for d3 in (0, 1, 5, 0xF):
        pokes = _anim_pokes(d3)
        o = (ctypes.c_uint32 * 4)()
        diffs, info = differential(
            0x16636, {"d3": d3, "a2": 0x30000, "_pokes": pokes},
            lambda lib, buf, c=d3, p=o: lib.func_16636(buf, c, 0x30000, p))
        assert not diffs, f"d3={d3}\n{report(diffs)}"
        if d3 != 0xF:                       # encontrado: expone a0/d2/d3/d7
            for i, reg in enumerate(("a0", "d2", "d3", "d7")):
                assert o[i] == info["regs"][reg], f"d3={d3} {reg}"


harness._lib.func_166ec.argtypes = [_u8p]
harness._lib.func_166ec.restype = ctypes.c_uint32
harness._lib.func_166da.argtypes = [_u8p]
harness._lib.func_166da.restype = ctypes.c_uint32
VBL_COUNTER = 0x1acd4
VBL_SITE = 0x1359a


def _scan_pokes():
    pokes = _anim_pokes(1)
    pokes.update({
        0x1a7ca: (0x40000).to_bytes(4, "big"),
        0x1a7ce: (0x40018).to_bytes(4, "big"),
        0x1a7d2: (0x40000).to_bytes(4, "big"),
        0x1ab4a: bytes([0]),
        0x1a450: (100).to_bytes(2, "big"),
        0x1a452: (50).to_bytes(2, "big"),
        0x19700: (0).to_bytes(2, "big"),
        0x19702: (0).to_bytes(2, "big"),
        0x1a48a: bytes([0]),
        0x199ca: (0x31000).to_bytes(4, "big"),
        0x19ace: (0x90000).to_bytes(4, "big"),
        0x1a776: (0x32000).to_bytes(4, "big"),
        0x0d8ea: bytes([0x41]),
        0x1d402: bytes([0x00]),
    })
    for i in range(3):
        base = 0x40000 + i * 8
        pokes[base] = bytes([0])
        pokes[base + 4] = bytes([0xD0 if i == 0 else 0x00])
    for i in range(0x100):
        pokes[0x31000 + i] = bytes([(i * 5 + 1) & 0xFF])
        pokes[0x199ce + i] = bytes([(i * 7 + 1) & 0xFF])
    for i in range(0x10):
        pokes[0x32000 + i] = bytes([0, 2, 0, 0, 0, 0x60, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0])
    # descriptor de sprite valido en 0x32000 (outer=2, inner=2, dest=0x90000,
    # flags=4, stride=4, datos)
    for i, b in enumerate(bytes([2, 2]) + (0x90000).to_bytes(4, "big") +
                          (4).to_bytes(2, "big") + (4).to_bytes(4, "big") +
                          bytes(range(0x20, 0x30))):
        pokes[0x32000 + i] = bytes([b])
    return pokes


def _run_scan(entry, fn, counter):
    pokes = _scan_pokes()
    pokes[VBL_COUNTER] = bytes([counter])
    pokes[0x1acd4] = bytes([counter])
    diffs, _ = differential(
        entry, {"_pokes": pokes}, lambda lib, buf, f=fn: f(buf),
        schedule=[{"pc": VBL_SITE, "nth": 1, "addr": VBL_COUNTER,
                   "width": 1, "value": (counter + 1) & 0xff}],
        wait_sites=[VBL_SITE])
    assert not diffs, f"{entry:#x}\n{report(diffs)}"


def test_fun_166ec():
    _run_scan(0x166EC, harness._lib.func_166ec, 0x00)


def test_fun_166da():
    _run_scan(0x166DA, harness._lib.func_166da, 0x1E)


harness._lib.wait_frame_cursor.argtypes = [_u8p]
harness._lib.wait_frame_cursor.restype = None
harness._lib.hit_test_objects.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.hit_test_objects.restype = None
harness._lib.func_14832.argtypes = [_u8p]
harness._lib.func_14832.restype = None


def test_fun_10522():
    pokes = _scan_pokes()
    pokes[VBL_COUNTER] = bytes([0x00])
    diffs, _ = differential(
        0x10522, {"_pokes": pokes}, lambda lib, buf: lib.wait_frame_cursor(buf),
        schedule=[{"pc": 0x136b0, "nth": 1, "addr": 0x1a44f, "width": 1, "value": 0x00}],
        wait_sites=[0x136b0])
    assert not diffs, report(diffs)


def _hit_pokes(cursor_x, cursor_y):
    pokes = {
        0x1a7ca: (0x40000).to_bytes(4, "big"),
        0x1a7ce: (0x40010).to_bytes(4, "big"),
        0x1a450: cursor_x.to_bytes(2, "big"),
        0x1a452: cursor_y.to_bytes(2, "big"),
        0x1a50e: (0x9999).to_bytes(2, "big"),
        0x1ab45: bytes([0x55]),
        0x1a48a: bytes([0xAA]),
        0x1acc9: bytes([0x77]),
    }
    pokes[0x40000] = bytes([0x00, 0x20, 0x00, 0x20, 0x80, 0x11])
    pokes[0x40006] = (0x1234).to_bytes(2, "big")
    pokes[0x40008] = bytes([0x30, 0x40, 0x30, 0x40, 0x00, 0x00, 0x00, 0x00])
    for i in range(0x29):
        base = 0x1AA72 + i * 5
        pokes[base] = bytes([0])
        pokes[base + 1] = bytes([i & 0xF])
        pokes[base + 2] = bytes([0])
        pokes[base + 3] = bytes([0])
        pokes[base + 4] = bytes([0])
    return pokes


def test_fun_14886():
    for cx, cy, d2, d3 in ((0x40, 0x10, 0xFF, 0x80), (0xC0, 0x10, 0xFF, 0x80)):
        pokes = _hit_pokes(cx, cy)
        diffs, _ = differential(
            0x14886, {"d2": d2, "d3": d3, "_pokes": pokes},
            lambda lib, buf: lib.hit_test_objects(buf, d2, d3))
        assert not diffs, f"cx={cx:#x} cy={cy:#x}\n{report(diffs)}"


def test_fun_14832():
    for cx, cy in ((0x40, 0x10), (0xC0, 0x10)):
        for cur_class in (0x00, 0x10, 0x20):
            pokes = _hit_pokes(cx, cy)
            pokes[0x1acd9] = bytes([0xFF])
            pokes[0x1acda] = bytes([0x80])
            pokes[0x1ad0f] = bytes([cur_class])
            pokes[0x199ca] = (0).to_bytes(4, "big")
            diffs, _ = differential(0x14832, {"_pokes": pokes},
                                    lambda lib, buf: lib.func_14832(buf))
            assert not diffs, f"cx={cx:#x} class={cur_class:#x}\n{report(diffs)}"


harness._lib.update_icon_list.argtypes = [_u8p]
harness._lib.update_icon_list.restype = None


def test_fun_16582():
    for byte4, cls3 in ((0x50, 0), (0x10, 0)):
        pokes = _anim_pokes(cls3)
        pokes.update({
            0x1a7ca: (0x40000).to_bytes(4, "big"),
            0x1a7ce: (0x40010).to_bytes(4, "big"),
            0x1acd2: bytes([0x00]),
        })
        base = 0x1AA72
        pokes[base] = bytes([0x00])
        pokes[base + 1] = bytes([0x00])
        pokes[base + 2] = bytes([0x11])
        pokes[base + 3] = bytes([cls3])
        pokes[base + 4] = bytes([0x00])
        pokes[0x40000] = bytes([8, 0x20, 4, 0x20, byte4, 0, 0, 0])
        pokes[0x40008] = bytes([0x30, 0x40, 0x30, 0x40, 0x10, 0, 0, 0])
        diffs, _ = differential(0x16582, {"_pokes": pokes},
                                lambda lib, buf: lib.update_icon_list(buf))
        assert not diffs, f"byte4={byte4:#x}\n{report(diffs)}"


harness._lib.func_17856.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_17856.restype = None


def test_fun_17856():
    for byte0 in (0x81, 0x01):
        pokes = {
            0x1a81e: (0x40000).to_bytes(4, "big"),
            0x40000: (0x32000).to_bytes(4, "big"),
            0x1a81a: (0x41000).to_bytes(4, "big"),
            0x41000: bytes([8, 0, 4, 0, 0, 0, 0, 0]),
            0x23EE8: bytes([1]),
            0x1a45e: (0x90000).to_bytes(4, "big"),
        }
        for i, b in enumerate(bytes([2, 2]) + (0x90000).to_bytes(4, "big") +
                              (4).to_bytes(2, "big") + (4).to_bytes(4, "big") +
                              bytes(range(0x20, 0x30))):
            pokes[0x32000 + i] = bytes([b])
        for i in range(0x10000):
            pokes[0x90000 + i] = bytes([(i * 3 + 1) & 0xFF])
            pokes[0x88000 + i] = bytes([(i * 11 + 7) & 0xFF])
        pokes[0x42000] = bytes([byte0, 0x00, 0x00])
        diffs, _ = differential(0x17856, {"a1": 0x42000, "_pokes": pokes},
                                lambda lib, buf: lib.func_17856(buf, 0x42000))
        assert not diffs, f"byte0={byte0:#x}\n{report(diffs)}"


harness._lib.func_18740.argtypes = [_u8p]
harness._lib.func_18740.restype = None


def test_fun_18740():
    for t in (0x0FFF, 0x0E10, 0x1800, 0x189C, 0x1C20):
        for acec in (0x00, 0x40):
            for cmd in (0x10, 0x2C, 0x3B, 0x3D, 0x4A, 0x50):
                pokes = {
                    0x1ab60: t.to_bytes(2, "big"), 0x1acec: bytes([acec]),
                    0x1acd2: bytes([cmd]), 0x1acfd: bytes([0]),
                    0x1ad14: bytes([0]), 0x1ac06: (0).to_bytes(2, "big"),
                }
                for i in range(0x80):
                    pokes[0x1AA72 + i] = bytes([0x11])
                diffs, _ = differential(0x18740, {"_pokes": pokes},
                                        lambda lib, buf: lib.func_18740(buf))
                assert not diffs, f"t={t:#x} acec={acec:#x} cmd={cmd:#x}\n{report(diffs)}"


harness._lib.func_18dc0.argtypes = [_u8p]
harness._lib.func_18dc0.restype = None


def test_fun_18dc0():
    # 0x1ab4f puesto -> el original se salta el Supexec de musica (misma puerta).
    for c in (8, 0x32, 0x10):
        for d7 in (0x10, 0x40):
            pokes = {
                0x1acd2: bytes([c]), 0x1acd7: bytes([d7]),
                0x1a579: bytes([0x00]), 0x1ab4f: bytes([0x01]),
                0x2741E: bytes([(i * 7 + 3) & 0xFF for i in range(0x40)]),
            }
            diffs, _ = differential(0x18dc0, {"_pokes": pokes},
                                    lambda lib, buf: lib.func_18dc0(buf))
            assert not diffs, f"c={c:#x} d7={d7:#x}\n{report(diffs)}"


harness._lib.script_step.argtypes = [_u8p]
harness._lib.script_step.restype = None


def test_fun_17e3c():
    # salidas tempranas
    for ad0b, acec, acb2, ab60 in ((1, 0x00, 0x0000, 0x0000),
                                   (0, 0x3F, 0x0000, 0x0000),
                                   (0, 0x00, 0x0100, 0x0100)):
        pokes = {
            0x1ad0b: bytes([ad0b]), 0x1acec: bytes([acec]),
            0x1acb2: acb2.to_bytes(2, "big"), 0x1ab60: ab60.to_bytes(2, "big"),
            0x1ac06: (0x1234).to_bytes(2, "big"),
        }
        diffs, _ = differential(0x17e3c, {"_pokes": pokes},
                                lambda lib, buf: lib.script_step(buf))
        assert not diffs, f"ad0b={ad0b} acec={acec:#x}\n{report(diffs)}"


harness._lib.game_loop.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.game_loop.restype = None


def test_game_loop():
    pokes = {
        0x1a4d2: (1).to_bytes(2, "big"),
        0x1acd4: bytes([0x00]),
        0x1acd2: bytes([0x00]),
        0x1ad0b: bytes([0x01]),
        0x1ab60: (0).to_bytes(2, "big"),
        0x1a526: (0).to_bytes(2, "big"),
        0x1a44f: bytes([0x00]),
        0x1a456: bytes([0x00]),
        0x1a450: (0x40).to_bytes(2, "big"),
        0x1a452: (0x10).to_bytes(2, "big"),
        0x1AB40: bytes([0, 0x20, 0, 0x20]),
        0x1ab47: bytes([0x64]),
        0x1acf2: bytes([0x00]),
        0x1ace7: bytes([0x00]),
        0x1aab4: bytes([0x00]),
        0x1aae1: bytes([0x00]),
        0x1acec: bytes([0x00]),
        0x1acb2: (0).to_bytes(2, "big"),
        0x1a7ca: (0x40000).to_bytes(4, "big"),
        0x1a7ce: (0x40010).to_bytes(4, "big"),
        0x1ab4f: bytes([0x00]),
        0x1a45e: (0x90000).to_bytes(4, "big"),
        0x199ca: (0x32000).to_bytes(4, "big"),
        0x19ace: (0x90000).to_bytes(4, "big"),
        0x1a48a: bytes([0]),
    }
    pokes[0x40000] = bytes([0, 0x20, 0, 0x20, 0x80, 0x11])
    pokes[0x40006] = (0x1234).to_bytes(2, "big")
    pokes[0x40008] = bytes([0x30, 0x40, 0x30, 0x40, 0, 0, 0, 0])
    for i in range(0x10000):
        pokes[0x90000 + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x88000 + i] = bytes([(i * 11 + 7) & 0xFF])
    for i in range(0x100):
        pokes[0x199ce + i] = bytes([(i * 7 + 1) & 0xFF])
    desc = (bytes([2, 2]) + (0x90000).to_bytes(4, "big") + (4).to_bytes(2, "big") +
            (4).to_bytes(4, "big") + bytes(range(0x20, 0x30)))
    for i, b in enumerate(desc):
        pokes[0x32000 + i] = bytes([b])
    sched = [{"pc": 0x136b0, "nth": 1, "addr": 0x1a44f, "width": 1, "value": 0x00},
             {"pc": 0x1359a, "nth": 1, "addr": 0x1acd4, "width": 1, "value": 0x01}]
    diffs, _ = differential(
        0x101BE, {"_pokes": pokes},
        lambda lib, buf: lib.game_loop(buf, harness.emu.STACK_TOP),
        schedule=sched, wait_sites=[0x136b0, 0x1359a])
    assert not diffs, report(diffs)


harness._lib.func_17792.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32,
                                     ctypes.POINTER(ctypes.c_uint32),
                                     ctypes.POINTER(ctypes.c_uint32),
                                     ctypes.POINTER(ctypes.c_uint32)]
harness._lib.func_17792.restype = ctypes.c_int
harness._lib.func_177e4.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.func_177e4.restype = None
harness._lib.func_1780c.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_1780c.restype = ctypes.c_uint32


def test_fun_17792():
    """Cuerpo de busqueda con mascara D3 propia (0x1778e es D3 = 0x3f)."""
    classes = [0x10, 0x11, 0x12, 0x13, 0x14, 0x15]
    for d0, d3 in ((0x10, 0x3F), (0x12, 0x3F), (0x04, 0x07), (0x99, 0x3F)):
        pokes = _obj_pokes(classes)
        o0 = ctypes.c_uint32()
        o1 = ctypes.c_uint32()
        o2 = ctypes.c_uint32()
        diffs, info = differential(
            0x17792, {"d0": d0, "d2": 0, "d3": d3, "_pokes": pokes},
            lambda lib, buf: lib.func_17792(buf, d0, d3, 0, ctypes.byref(o0),
                                             ctypes.byref(o1), ctypes.byref(o2)))
        assert not diffs, f"d0={d0:#x} d3={d3:#x}\n{report(diffs)}"
        assert o0.value == info["regs"]["a0"], f"d0={d0:#x} d3={d3:#x} a0"
        assert o1.value == info["regs"]["d1"], f"d0={d0:#x} d3={d3:#x} d1"
        assert o2.value == info["regs"]["d2"], f"d0={d0:#x} d3={d3:#x} d2"


def test_fun_177e4():
    for a0, d2 in ((0x30008, 2), (0x30000, 1), (0x30028, 6), (0x30000, 0)):
        pokes = {0x1a81a: (0xDEADBEEF).to_bytes(4, "big"),
                 0x1acc9: bytes([0x55]),
                 0x1a81e: (0xDEADBEEF).to_bytes(4, "big")}
        diffs, _ = differential(0x177E4, {"a0": a0, "d2": d2, "_pokes": pokes},
                                lambda lib, buf: lib.func_177e4(buf, a0, d2))
        assert not diffs, f"a0={a0:#x} d2={d2}\n{report(diffs)}"


def _class_table_pokes():
    classes = [0x10, 0x11, 0x12, 0x13, 0x14, 0x15]
    pokes = _obj_pokes(classes)
    for i in range(0x29):
        pokes[0x1aa72 + i * 5] = bytes([0x10 + i])
        pokes[0x1aa72 + i * 5 + 1] = bytes([3 if i == 0 else 0x0A])
    pokes[0x1a772] = (0xAABBCCDD).to_bytes(4, "big")
    pokes[0x1ace6] = bytes([0x00])
    return pokes


A6_SEL = 0x40000


def test_fun_1780c():
    base = _class_table_pokes()
    # D3 = 0: indexa el objeto directamente (indice 3), la clase 3 existe.
    pokes = dict(base)
    pokes[A6_SEL] = bytes([0x00, 0x00, 0x03])
    diffs, info = differential(
        0x1780C, {"a6": A6_SEL, "d0": 0, "d2": 0, "_pokes": pokes},
        lambda lib, buf: lib.func_1780c(buf, A6_SEL))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]
    # D3 != 0: busca la clase 0x12 -> objeto indice 3.
    pokes = dict(base)
    pokes[A6_SEL] = bytes([0x00, 0x3F, 0x12])
    diffs, info = differential(
        0x1780C, {"a6": A6_SEL, "d0": 0, "d2": 0, "_pokes": pokes},
        lambda lib, buf: lib.func_1780c(buf, A6_SEL))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]
    # Clase no encontrada en la tabla (indice 10): 0x1a772 = 0x1aad6.
    pokes = dict(base)
    pokes[A6_SEL] = bytes([0x00, 0x00, 0x0A])
    diffs, info = differential(
        0x1780C, {"a6": A6_SEL, "d0": 0, "d2": 0, "_pokes": pokes},
        lambda lib, buf: lib.func_1780c(buf, A6_SEL))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


def test_fun_1780c_no_encontrado():
    """Sin coincidencia el original salta a 0x1018e (Pterm) sin escribir nada."""
    base = _class_table_pokes()
    pokes = dict(base)
    pokes[A6_SEL] = bytes([0x00, 0x3F, 0x99])
    diffs, _ = differential(
        0x1780C, {"a6": A6_SEL, "d0": 0, "d2": 0, "_pokes": pokes},
        lambda lib, buf: lib.func_1780c(buf, A6_SEL), stop_pc=0x1018E)
    assert not diffs, report(diffs)


FUN_16F90 = 0x16F90
FUN_17094 = 0x17094
FUN_172CA = 0x172CA
harness._lib.hint_table_relocate.argtypes = [_u8p]
harness._lib.hint_table_relocate.restype = None
harness._lib.func_17094.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_17094.restype = None
harness._lib.func_172ca.argtypes = [_u8p]
harness._lib.func_172ca.restype = None


def test_fun_16f90():
    """Suma 0x101a8 a cada long no nulo de 0x1a752..0x1a82e."""
    pokes = {}
    for i in range(0x37):
        v = 0 if i % 3 == 0 else (0x1000 + i * 7)
        pokes[0x1A752 + i * 4] = v.to_bytes(4, "big")
    diffs, _ = differential(FUN_16F90, {"_pokes": pokes},
                            lambda lib, buf: lib.hint_table_relocate(buf))
    assert not diffs, report(diffs)


def _hint_common(cls):
    """Tabla 0x1aa72 con bytes concretos (offsets 0,25,26,170,171,175,176)."""
    pokes = {
        0x1A7CA: (0x50000).to_bytes(4, "big"),
        0x1A7CE: (0x50008).to_bytes(4, "big"),
        0x1ACD2: bytes([0x08]),
        0x1ACFC: bytes([0xFF]),
        0x1AB60: (0x0100).to_bytes(2, "big"),
        0x1ACD7: bytes([0x10]),
        0x1ACC7: bytes([0x22]),
        0x1AD14: bytes([0x00]),
        0x1A80A: (0xAABBCCDD).to_bytes(4, "big"),
        0x1A80E: (0xAABBCCDD).to_bytes(4, "big"),
    }
    for off in (0, 25, 26, 170, 171, 175, 176):
        pokes[0x1AA72 + off] = bytes([0x11])
    pokes[0x50000] = bytes([0, 0, 0, 0, cls, 0, 0, 0])
    return pokes


def test_fun_17094():
    """Hint de clase 0x50: clear, a014, a015 y el corte 0x4e."""
    # (clase_obj, 0x1acd2, 0x1acfc, 0x1acd7, tabla_casa, byte0)
    cases = [
        (0x50, 0x08, 0x80, 0x10, True, 0x11),    # corte 0x1acd2==8 + bit7
        (0x51, 0x08, 0x00, 0x10, True, 0x11),    # sin objeto clase 0x50
        (0x50, 0x08, 0x00, 0x10, False, 0x11),   # 0x1acc7 sin entrada
        (0x50, 0x11, 0x00, 0x10, True, 0x11),    # img[a1] == 0x1acd2 -> a015
        (0x50, 0x11, 0x00, 0x30, True, 0x99),    # [0x27,0x4e) -> a014
        (0x50, 0x11, 0x00, 0x10, True, 0x99),    # <0x27 -> store + a015
        (0x50, 0x11, 0x00, 0x50, True, 0x99),    # >=0x4e -> clear y sale
    ]
    for i, (cls, cd2, cfc, cd7, hit, b0) in enumerate(cases):
        pokes = _hint_common(cls)
        pokes[0x1ACD2] = bytes([cd2])
        pokes[0x1ACFC] = bytes([cfc])
        pokes[0x1ACD7] = bytes([cd7])
        pokes[0x19B46] = bytes([0x22] + [0] * 0xC) if hit else bytes([0x99] + [0] * 0xC)
        pokes[0x1AA72] = bytes([b0])
        diffs, _ = differential(FUN_17094, {"a1": 0x1AA72, "_pokes": pokes},
                                lambda lib, buf: lib.func_17094(buf, 0x1AA72))
        assert not diffs, f"case {i}\n{report(diffs)}"


def test_fun_172ca():
    """Hint de clase 0x51: 0x3b, a01e, a01f y no-encontrado."""
    # (clase_obj, 0x1acd2, 0x1ad14, 0x1acd7, tabla_casa, byte25)
    cases = [
        (0x51, 0x3B, 0x00, 0x10, True, 0x11),    # atajo 0x3b
        (0x51, 0x3B, 0x01, 0x10, True, 0x3B),    # img[a1+25] == 0x1acd2 -> a01f
        (0x50, 0x08, 0x00, 0x10, True, 0x11),    # sin objeto clase 0x51
        (0x51, 0x08, 0x00, 0x10, False, 0x11),   # sin entrada en 0x19c8b
        (0x51, 0x11, 0x00, 0x10, True, 0x99),    # <0xb2 -> clear y sale
        (0x51, 0x11, 0x00, 0xB2, True, 0x99),    # [0xb2,0xd9) -> a01e
        (0x51, 0x11, 0x00, 0xD9, True, 0x99),    # >=0xd9 -> store + a01f
    ]
    for i, (cls, cd2, ad14, cd7, hit, b25) in enumerate(cases):
        pokes = _hint_common(cls)
        pokes[0x1ACD2] = bytes([cd2])
        pokes[0x1AD14] = bytes([ad14])
        pokes[0x1ACD7] = bytes([cd7])
        pokes[0x19C8B] = bytes([0x22] + [0] * 6) if hit else bytes([0x99] + [0] * 6)
        pokes[0x1AA72 + 25] = bytes([b25])
        diffs, _ = differential(FUN_172CA, {"_pokes": pokes},
                                lambda lib, buf: lib.func_172ca(buf))
        assert not diffs, f"case {i}\n{report(diffs)}"


FUN_188CE = 0x188CE
harness._lib.func_188ce.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_188ce.restype = ctypes.c_uint32


def _sce_pokes(script_byte, classes=(0x12, 0x16), ab08=1):
    pokes = _obj_pokes(list(classes) + [0x00] * (OBJ_N - len(classes)))
    for i in range(0x100):
        pokes[0x1AA72 + i] = bytes([0x11])
    pokes[0x1AA72 + 0x87 + 3] = bytes([0x02])      # cls = (2&7)|0x10 = 0x12
    pokes[0x1AA72 + 0x96 + 3] = bytes([0x06])      # cls = 0x16
    pokes[0x1AAF9] = bytes([0x10])
    pokes[0x1ACD2] = bytes([0x10])
    pokes[0x1AB08] = bytes([ab08])
    pokes[0x1AD1D] = bytes([0xAA])
    pokes[0x1ACC9] = bytes([0x05])
    for i in range(0x10):                          # tabla de iconos: blit no-op
        pokes[0x1A776 + i * 4] = (0).to_bytes(4, "big")
    for i in range(0x44):                          # animaciones 0x39/0x3a sin cuerpo
        pokes[0x23EE8 + i] = bytes([0x01])
    pokes[0x52000] = bytes([0x00, script_byte])
    return pokes


def test_fun_188ce_animaciones():
    for ab08 in (0, 1):
        pokes = _sce_pokes(0x3B, ab08=ab08)
        diffs, info = differential(
            FUN_188CE, {"a6": 0x52000, "_pokes": pokes},
            lambda lib, buf: lib.func_188ce(buf, 0x52000))
        assert not diffs, f"ab08={ab08}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"]


def test_fun_188ce_no_dispara():
    for script_byte in (0x3A, 0x3F, 0x40):
        pokes = _sce_pokes(script_byte)
        diffs, info = differential(
            FUN_188CE, {"a6": 0x52000, "_pokes": pokes},
            lambda lib, buf: lib.func_188ce(buf, 0x52000))
        assert not diffs, f"byte={script_byte:#x}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"]


def test_fun_188ce_modo_distinto():
    pokes = _sce_pokes(0x3B)
    pokes[0x1ACD2] = bytes([0x11])
    diffs, info = differential(
        FUN_188CE, {"a6": 0x52000, "_pokes": pokes},
        lambda lib, buf: lib.func_188ce(buf, 0x52000))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


FUN_17480 = 0x17480
harness._lib.func_17480.argtypes = [_u8p]
harness._lib.func_17480.restype = None


def _sel_pokes(cmd, acec=0x10, ad00=0, acd7=0x10, tbl=0x10, entry4=0, bit6=0, found=True):
    pokes = {
        0x1ACD2: bytes([cmd]), 0x1ACEC: bytes([acec]), 0x1AD00: bytes([ad00]),
        0x1ACD7: bytes([acd7]), 0x1A579: bytes([2]),
        0x1ABF8: (0).to_bytes(2, "big"), 0x1ACF2: bytes([0x00]),
        0x1ACFF: bytes([0x77]), 0x1A772: (0xDEADBEEF).to_bytes(4, "big"),
        0x1A812: (0).to_bytes(4, "big"), 0x1A816: (0).to_bytes(4, "big"),
    }
    pokes[0x2741E + 3] = bytes([tbl])              # 0x16210: 2 -> 3
    pokes.update(_obj_pokes([0x11 if found else 0x12, 0, 0, 0, 0, 0]))
    a1 = 0x1AA72 + 5                                # ad00=0 -> d1=5
    pokes[a1 + 1] = bytes([bit6])
    pokes[a1 + 4] = bytes([entry4])
    return pokes


def test_fun_17480_etiquetas_37():
    pokes = _sel_pokes(0x37)
    diffs, _ = differential(FUN_17480, {"_pokes": pokes},
                            lambda lib, buf: lib.func_17480(buf))
    assert not diffs, report(diffs)


def test_fun_17480_bordes():
    """$1acec >= 0x3f o $1acd2 >= 0x2c: limpia las cuatro etiquetas y sale."""
    for acec, cmd in ((0x3F, 0x10), (0x00, 0x2C)):
        pokes = _sel_pokes(cmd, acec=acec)
        diffs, _ = differential(FUN_17480, {"_pokes": pokes},
                                lambda lib, buf: lib.func_17480(buf))
        assert not diffs, f"acec={acec:#x} cmd={cmd:#x}\n{report(diffs)}"


def test_fun_17480_no_encontrado_y_bit6():
    pokes = _sel_pokes(0x10, found=False)
    diffs, _ = differential(FUN_17480, {"_pokes": pokes},
                            lambda lib, buf: lib.func_17480(buf))
    assert not diffs, report(diffs)
    pokes = _sel_pokes(0x10, bit6=0x40)
    diffs, _ = differential(FUN_17480, {"_pokes": pokes},
                            lambda lib, buf: lib.func_17480(buf))
    assert not diffs, report(diffs)


def test_fun_17480_comandos():
    """Path A (cmd < 6): umbrales de la tabla 0x2741e (d0)."""
    for tbl in (0x10, 0x17, 0x20, 0x33, 0x34, 0x50):
        pokes = _sel_pokes(0x10, tbl=tbl)
        diffs, _ = differential(FUN_17480, {"_pokes": pokes},
                                lambda lib, buf: lib.func_17480(buf))
        assert not diffs, f"tbl={tbl:#x}\n{report(diffs)}"


def test_fun_17480_d2_path_a():
    """d2 segun $1acd7 y el campo 4 de la entrada (path A)."""
    for acd7, e4 in ((0x59, 0x06), (0x59, 0x13), (0x59, 0x18), (0x5a, 0x18),
                     (0x59, 0x00), (0x59, 0x1b)):
        pokes = _sel_pokes(0x10, acd7=acd7, entry4=e4)
        diffs, _ = differential(FUN_17480, {"_pokes": pokes},
                                lambda lib, buf: lib.func_17480(buf))
        assert not diffs, f"acd7={acd7:#x} e4={e4:#x}\n{report(diffs)}"


def test_fun_17480_path_b():
    """cmd >= 6: d2=1 con $1acd7 < 0x27, d2=2 con >= 0xa6; umbral d0 en 0x1a."""
    for acd7, e4, tbl in ((0x20, 0x06, 0x10), (0x20, 0x00, 0x10), (0xa6, 0x00, 0x10),
                          (0x26, 0x06, 0x19), (0xa6, 0x06, 0x1a), (0x20, 0x06, 0x34)):
        pokes = _sel_pokes(0x06, acd7=acd7, entry4=e4, tbl=tbl)
        diffs, _ = differential(FUN_17480, {"_pokes": pokes},
                                lambda lib, buf: lib.func_17480(buf))
        assert not diffs, f"acd7={acd7:#x} e4={e4:#x} tbl={tbl:#x}\n{report(diffs)}"


def test_fun_17480_indice_rotatorio():
    """$1ad00 avanza 5 y vuelve a 5 al pasar de 0x18."""
    for ad00 in (0x00, 0x12, 0x13, 0x14, 0x18, 0x30):
        pokes = _sel_pokes(0x10, ad00=ad00)
        diffs, _ = differential(FUN_17480, {"_pokes": pokes},
                                lambda lib, buf: lib.func_17480(buf))
        assert not diffs, f"ad00={ad00:#x}\n{report(diffs)}"


FUN_1857C = 0x1857C
FUN_18944 = 0x18944
harness._lib.func_1857c.argtypes = [_u8p]
harness._lib.func_1857c.restype = None
harness._lib.func_18944.argtypes = [_u8p]
harness._lib.func_18944.restype = None


def _icon_tbl_pokes(cmd, find_at=None, count80=(0, 1)):
    pokes = {
        0x1ACD2: bytes([cmd]),
        0x1ACF7: bytes([0x55]),
        0x1A45E: (0x90000).to_bytes(4, "big"),
        0x1A534: (0x28).to_bytes(4, "big"),
    }
    for i in range(63):
        a = 0x1A8F0 + i * 6
        pokes[a] = bytes([0x00, (i * 3) & 0xFF])
    if find_at is not None:
        a = 0x1A8F0 + find_at * 6
        pokes[a] = bytes([0x40, cmd])
    for i in range(14):
        a = 0x1A9D4 + i * 6
        pokes[a] = bytes([0x80 if i in count80 else 0x00, 0x00])
    for i in range(0x4000):
        pokes[0x90000 + i] = bytes([(i * 3 + 1) & 0xFF])
    return pokes


def test_fun_1857c():
    for find_at in (None, 0, 5, 0x3E):
        for count80 in ((), (0,), (0, 7, 13)):
            pokes = _icon_tbl_pokes(0x10, find_at=find_at, count80=count80)
            diffs, _ = differential(FUN_1857C, {"_pokes": pokes},
                                    lambda lib, buf: lib.func_1857c(buf))
            assert not diffs, f"find={find_at} c80={count80}\n{report(diffs)}"


def test_fun_18944():
    for ab08 in (0, 1):
        pokes = _sce_pokes(0x3B, ab08=ab08)
        pokes[0x1AD1D] = bytes([0x01])
        pokes[0x1A7CA] = (0x30000).to_bytes(4, "big")
        pokes[0x1A7CE] = (0x30030).to_bytes(4, "big")
        for i in range(6):
            pokes[0x30000 + i * 8 + 4] = bytes([0x60 if i == 0 else 0x00])
        diffs, _ = differential(FUN_18944, {"_pokes": pokes},
                                lambda lib, buf: lib.func_18944(buf))
        assert not diffs, f"ab08={ab08}\n{report(diffs)}"
    pokes = _sce_pokes(0x3B)
    pokes[0x1AD1D] = bytes([0x00])
    diffs, _ = differential(FUN_18944, {"_pokes": pokes},
                            lambda lib, buf: lib.func_18944(buf))
    assert not diffs, report(diffs)


# ================= lote 0x18xxx: wrappers y helpers =================

FUN_1857A = 0x1857A
harness._lib.func_1857a.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_1857a.restype = ctypes.c_uint32

for _n in ("func_18888", "func_18890"):
    getattr(harness._lib, _n).argtypes = [_u8p, ctypes.c_uint32]
    getattr(harness._lib, _n).restype = ctypes.c_uint32

FUN_18A10 = 0x18A10
harness._lib.func_18a10.argtypes = [_u8p]
harness._lib.func_18a10.restype = None

FUN_18192 = 0x18192
FUN_1816E = 0x1816E
harness._lib.func_18192.argtypes = [_u8p]
harness._lib.func_18192.restype = None
harness._lib.func_1816e.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_1816e.restype = ctypes.c_uint32


def test_fun_1857a():
    for find_at in (None, 5):
        pokes = _icon_tbl_pokes(0x10, find_at=find_at)
        diffs, info = differential(
            FUN_1857A, {"a6": 0x30000, "_pokes": pokes},
            lambda lib, buf: lib.func_1857a(buf, 0x30000))
        assert not diffs, f"find={find_at}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"]


def _body188_pokes():
    pokes = {
        0x27FE2: bytes([6, 0x08, 0x03, 0x00, 0x00, 0x00]),
        0x28AD2: (0x10).to_bytes(2, "big"),
        0x28AD4: bytes([2, 2]),
        0x1A45E: (0x90000).to_bytes(4, "big"),
        0x1A462: (0x88000).to_bytes(4, "big"),
        0x30000: bytes([0x00, 0x00, 8, 4]),      # a6[1]=entrada, a6[2]/[3]=x/y
    }
    for i in range(0x200):
        pokes[0x28AD6 + i] = bytes([(i * 7 + 3) & 0xFF])
    for i in range(0x1000):
        pokes[0xD8F0 + i] = bytes([(i * 5 + 1) & 0xFF])
    for i in range(0x10000):
        pokes[0x90000 + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x88000 + i] = bytes([(i * 11 + 7) & 0xFF])
    return pokes


def test_fun_18888_18890():
    for entry, fn, a6v in ((0x18888, harness._lib.func_18888, 0x8000),
                           (0x18890, harness._lib.func_18890, 0)):
        pokes = _body188_pokes()
        diffs, info = differential(
            entry, {"a6": 0x30000, "_pokes": pokes},
            lambda lib, buf, f=fn: f(buf, 0x30000))
        assert not diffs, f"{entry:#x} a6v={a6v:#x}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], f"{entry:#x} a6"


def _sound_bank_pokes(skips):
    pokes = {0x1A579: bytes([2]), 0x1A57C: bytes([0xFF]), 0x1AB4F: bytes([0x00])}
    base = 0x55B0A
    for _ in range(skips):
        pokes[base] = bytes([0x00, 0x02])
        base += 2
    pokes[base] = bytes([0x00, 0x08, 0x03, 0x55, 0x11, 0x22, 0x33, 0x44])
    pokes[0x1A4EC] = (0x55B0A).to_bytes(4, "big")
    return pokes


def test_fun_18a10():
    for tbl, skips in ((0x00, 0), (0x01, 0), (0x07, 0), (0x08, 1),
                       (0x10, 2), (0x18, 3), (0xFF, 3)):
        pokes = _sound_bank_pokes(skips)
        pokes[0x2741E + 3] = bytes([tbl])
        diffs, _ = differential(FUN_18A10, {"_pokes": pokes},
                                lambda lib, buf: lib.func_18a10(buf))
        assert not diffs, f"tbl={tbl:#x}\n{report(diffs)}"


def _f18192_pokes(acc7, table_match):
    pokes = {
        0x1ACC7: bytes([acc7]),
        0x1A57C: bytes([0x35]),              # 1066e(5): sin recarga
        0x1A7CA: (0x40000).to_bytes(4, "big"),
        0x1A7CE: (0x40008).to_bytes(4, "big"),
        0x40000: bytes([0x10, 0, 0, 0, 0x10, 0, 0, 0]),
    }
    for i in range(0x80):
        pokes[0x1AA72 + i] = bytes([0x00])
    pokes[0x1AA72 + 0xBE + 3] = bytes([0x00])   # cls = 0x10
    for i in range(16):
        pokes[0x1A776 + i * 4] = (0).to_bytes(4, "big")
    for i in range(0x17):                        # tabla 0x19dbd..0x19e19
        pokes[0x19DBD + i * 4] = bytes([0x08 if (table_match and i == 3) else
                                        0x01, 1, 0, 0])
    for i in range(0x44):
        pokes[0x23EE8 + i] = bytes([0x01])
    return pokes


def test_fun_18192():
    for acc7, match in ((0x08, True), (0x08, False), (0x01, True), (0x01, False)):
        pokes = _f18192_pokes(acc7, match)
        diffs, _ = differential(FUN_18192, {"_pokes": pokes},
                                lambda lib, buf: lib.func_18192(buf))
        assert not diffs, f"acc7={acc7:#x} match={match}\n{report(diffs)}"


def test_fun_1816e():
    pokes = _f18192_pokes(0x08, True)
    diffs, info = differential(
        FUN_1816E, {"a6": 0x30000, "_pokes": pokes},
        lambda lib, buf: lib.func_1816e(buf, 0x30000))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


FUN_18538 = 0x18538
FUN_184CC = 0x184CC
FUN_184D6 = 0x184D6
FUN_1851A = 0x1851A
harness._lib.func_18538.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.func_18538.restype = None
for _n in ("func_184cc", "func_184d6", "func_1851a"):
    getattr(harness._lib, _n).argtypes = [_u8p, ctypes.c_uint32]
    getattr(harness._lib, _n).restype = ctypes.c_uint32


def _sample_pokes(index=0):
    pokes = {
        0x1A75E: (0x1A8F0).to_bytes(4, "big"),
        0x1A8F0: bytes([0x00, 0x00, 0x03, 0x00, 0x00, 0x00]),
        0x1ACD2: bytes([0x10]),
        0x1ACC7: bytes([0x00]),
        0x1AB4F: bytes([0x00]),
        0x1A45E: (0x90000).to_bytes(4, "big"),
        0x1A534: (0x28).to_bytes(4, "big"),
        0x30000: bytes([0x00, index]),
    }
    for i in range(0x80):
        pokes[0x27FE2 + i] = bytes([0x01])
        pokes[0x23EE8 + i] = bytes([0x01])
    for i in range(0x100):
        pokes[0x1A8F0 + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x1A9D4 + i] = bytes([0x80 if i % 8 == 0 else 0x00])
    for i in range(0x4000):
        pokes[0x90000 + i] = bytes([(i * 3 + 1) & 0xFF])
    return pokes


def test_fun_18538():
    for d3 in (0x25, 0x3A, 0x28, 0x00, 0x40):
        for d4 in (0x80, 0x40):
            pokes = _sample_pokes()
            diffs, _ = differential(
                FUN_18538, {"d3": d3, "d4": d4, "_pokes": pokes},
                lambda lib, buf, a=d3, b=d4: lib.func_18538(buf, a, b))
            assert not diffs, f"d3={d3:#x} d4={d4:#x}\n{report(diffs)}"


def test_fun_184cc_184d6():
    for entry, fn in ((FUN_184CC, harness._lib.func_184cc),
                      (FUN_184D6, harness._lib.func_184d6)):
        pokes = _sample_pokes()
        diffs, info = differential(
            entry, {"a6": 0x30000, "_pokes": pokes},
            lambda lib, buf, f=fn: f(buf, 0x30000))
        assert not diffs, f"{entry:#x}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], f"{entry:#x} a6"


def test_fun_1851a():
    for index in (1, 2, 5, 0x3F):
        pokes = _sample_pokes(index=index)
        diffs, info = differential(
            FUN_1851A, {"a6": 0x30000, "_pokes": pokes},
            lambda lib, buf: lib.func_1851a(buf, 0x30000))
        assert not diffs, f"index={index}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], f"index={index} a6"


FUN_18CFC = 0x18CFC
FUN_186B6 = 0x186B6
FUN_1870A = 0x1870A
harness._lib.func_18cfc.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_18cfc.restype = ctypes.c_uint32
harness._lib.func_186b6.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_186b6.restype = ctypes.c_uint32
harness._lib.func_1870a.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_1870a.restype = ctypes.c_uint32


def _objlist_pokes(n=7):
    pokes = {
        0x1A7CA: (0x40000).to_bytes(4, "big"),
        0x1A7CE: (0x40000 + n * 8).to_bytes(4, "big"),
        0x1A45E: (0x90000).to_bytes(4, "big"),
        0x1A462: (0x88000).to_bytes(4, "big"),
        0x1AB4F: bytes([0x00]),
    }
    for i in range(n):
        a = 0x40000 + i * 8
        pokes[a] = bytes([0x10, 0x20, 0x30, 0x40, 0x40 if i % 2 else 0x00, 0, 0, 0])
        pokes[a + 50 - i * 8 if 50 - i * 8 >= 0 else 0] = bytes([0x00])
    for i in range(16):
        pokes[0x1A776 + i * 4] = (0).to_bytes(4, "big")
    for i in range(0x10000):
        pokes[0x90000 + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x88000 + i] = bytes([(i * 11 + 7) & 0xFF])
    return pokes


def test_fun_18cfc():
    for n in (7, 8):
        pokes = _objlist_pokes(n)
        diffs, info = differential(
            FUN_18CFC, {"a6": 0x30000, "_pokes": pokes},
            lambda lib, buf: lib.func_18cfc(buf, 0x30000))
        assert not diffs, f"n={n}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"]


def _f186b6_pokes(entries, start, count, match):
    pokes = {
        0x1A75E: (0xDEADBEEF).to_bytes(4, "big"),
        0x30000: bytes([0x00, start, count, match]),
    }
    for i in range(0x40):
        a = 0x1A8F0 + i * 6
        pokes[a] = bytes([entries[i % len(entries)], 0x00, 0x00])
        pokes[a + 3] = bytes([(i * 5 + 1) & 0xFF])
    return pokes


def test_fun_186b6():
    entries = [0x10, 0x20, 0x30, 0x40, 0x50, 0x20, 0x60, 0x70]
    for start, count, match in ((1, 8, 0x30), (1, 8, 0x70), (1, 8, 0x99),
                                (3, 2, 0x20), (1, 1, 0x10)):
        pokes = _f186b6_pokes(entries, start, count, match)
        diffs, info = differential(
            FUN_186B6, {"a6": 0x30000, "_pokes": pokes},
            lambda lib, buf: lib.func_186b6(buf, 0x30000))
        assert not diffs, f"start={start} count={count} match={match:#x}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"]


def test_fun_1870a():
    pokes = {
        0x1A579: bytes([2]),
        0x1ACC6: bytes([0x87]),
        0x1ACD4: bytes([0x00]),
        0x1A45E: (0x90000).to_bytes(4, "big"),
        0x1A462: (0x88000).to_bytes(4, "big"),
    }
    pokes[0x2741E + 3] = bytes([0x07])          # fuerza otra vuelta
    pokes[0x2741E + 4] = bytes([0x03])          # d4 = 0x14c
    for i in range(0x149):                      # banco 0x3c534, d4 = 0x14c
        pokes[0x3C534 + i * 3] = bytes([3, 0x21 + (i % 0x40), 0x22])
    for i in range(0x100):
        pokes[0x1A850 + i] = bytes([0x00])
    for i in range(0x4000):
        pokes[0x90000 + i] = bytes([(i * 3 + 1) & 0xFF])
    pokes.update(console_key("a"))
    diffs, info = differential(
        FUN_1870A, {"a6": 0x30000, "_pokes": pokes},
        lambda lib, buf: lib.func_1870a(buf, 0x30000))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


FUN_18E12 = 0x18E12
FUN_18E06 = 0x18E06
harness._lib.func_18e12.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_18e12.restype = None
harness._lib.func_18e06.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_18e06.restype = ctypes.c_uint32


def _f18e12_pokes(idx_val):
    pokes = {
        0x1A484: (3).to_bytes(2, "big"),
        0x1AB4F: bytes([0x01]),             # el driver de sonido se salta
        0x1A4DC: (0x55B20).to_bytes(4, "big"),
    }
    for i in range(8):
        pokes[0x19994 + i] = bytes([idx_val if i == 0 else 0x01])
    for i in range(0x14):
        pokes[0x1D402 + 3 + i] = bytes([i])
    for i in range(0x10):                   # banco 0x55b0a: entradas de longitud 4
        pokes[0x55B0A + i * 4] = bytes([0x00, 0x04, 0xAA + i, 0xBB])
    for i in range(0x20):
        pokes[0xD8F0 + i] = bytes([0x11])
    return pokes


def test_fun_18e12():
    for idx_val in (0, 1, 2, 3, 4, 5, 6, 7, 8):
        pokes = _f18e12_pokes(idx_val)
        diffs, _ = differential(
            FUN_18E12, {"d1": 0, "_pokes": pokes},
            lambda lib, buf: lib.func_18e12(buf, 0))
        assert not diffs, f"idx_val={idx_val}\n{report(diffs)}"


def test_fun_18e06():
    for idx_val in (1, 8):
        pokes = _f18e12_pokes(idx_val)
        pokes[0x30000] = bytes([0x00, 0x00])
        diffs, info = differential(
            FUN_18E06, {"a6": 0x30000, "_pokes": pokes},
            lambda lib, buf: lib.func_18e06(buf, 0x30000))
        assert not diffs, f"idx_val={idx_val}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"]


FUN_16902 = 0x16902
harness._lib.func_16902.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_16902.restype = ctypes.c_uint32


def _f16902_pokes(cd7):
    pokes = {
        0x1ACD7: bytes([cd7]),
        0x1A482: (3).to_bytes(2, "big"),
        0x1AB4F: bytes([0x01]),
        0x1ACC6: bytes([0x87]),
        0x1ACD4: bytes([0x00]),
        0x1A45E: (0x90000).to_bytes(4, "big"),
        0x1A462: (0x88000).to_bytes(4, "big"),
        0x1A81A: (0x41000).to_bytes(4, "big"),
        0x41000: bytes([0x10, 0x20, 0x30, 0x40]),
        0x52000: bytes([0x00, 0x08, 0x10, 0xC0]),
    }
    for i in range(0x40):                    # tabla de tracks: cualquier indice -> 1
        pokes[0x19994 + i] = bytes([1])
    for i in range(0x60):                    # entradas de 0x1d402 (con d4 = 6/7/0x23)
        pokes[0x1D402 + i] = bytes([i & 0x3F])
    for i in range(0x10):
        pokes[0x55B0A + i * 4] = bytes([0x00, 0x04, 0xAA + i, 0xBB])
    for i in range(0x100):
        pokes[0x3C534 + i * 3] = bytes([3, 0x21 + (i % 0x40), 0x22])
        pokes[0x1A850 + i] = bytes([0x00])
    for i in range(0x4000):
        pokes[0x90000 + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x88000 + i] = bytes([(i * 11 + 7) & 0xFF])
    pokes.update(console_key("a"))
    return pokes


def test_fun_16902():
    for cd7 in (0x10, 0x60, 0xC0):
        pokes = _f16902_pokes(cd7)
        diffs, info = differential(
            FUN_16902, {"a6": 0x52000, "_pokes": pokes},
            lambda lib, buf: lib.func_16902(buf, 0x52000))
        assert not diffs, f"cd7={cd7:#x}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], f"cd7={cd7:#x} a6"


VBL_SITE = 0x1359A
VBL_COUNTER = 0x1ACD4
IKBD_SITE = 0x136B0
IKBD_PACKET = 0x1A44F


def _wait_schedule():
    return [
        {"pc": IKBD_SITE, "nth": 1, "addr": IKBD_PACKET, "width": 1, "value": 0x01},
        {"pc": IKBD_SITE, "nth": 2, "addr": IKBD_PACKET, "width": 1, "value": 0x01},
        {"pc": VBL_SITE, "nth": 1, "addr": VBL_COUNTER, "width": 1, "value": 0x11},
    ]


def _vbl_k(n):
    return [{"pc": VBL_SITE, "nth": k, "addr": VBL_COUNTER, "width": 1, "value": k}
            for k in range(1, n + 1)]


FUN_1817A = 0x1817A
FUN_1881E = 0x1881E
FUN_1834A = 0x1834A
FUN_18334 = 0x18334
harness._lib.func_1817a.argtypes = [_u8p]
harness._lib.func_1817a.restype = None
harness._lib.func_1881e.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_1881e.restype = ctypes.c_uint32
harness._lib.func_1834a.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.func_1834a.restype = None
harness._lib.func_18334.argtypes = [_u8p]
harness._lib.func_18334.restype = None


def test_fun_1817a():
    pokes = {
        0x1A7CA: (0x40000).to_bytes(4, "big"),
        0x1A7CE: (0x40020).to_bytes(4, "big"),
        0x1AD0B: bytes([0x77]),
        0x1ABF8: b"\x12\x34",
        0x1ACC7: bytes([0x01]),
        0x1A57C: bytes([0x35]),
        0x1AD1D: bytes([0x00]),
    }
    for i in range(0x80):
        pokes[0x1AA72 + i] = bytes([0x00])
    for i in range(16):
        pokes[0x1A776 + i * 4] = (0).to_bytes(4, "big")
    for i in range(0x17):
        pokes[0x19DBD + i * 4] = bytes([0x01, 1, 0, 0])
    for i in range(0x44):
        pokes[0x23EE8 + i] = bytes([0x01])
    for i in range(0x80):
        pokes[0x40000 + i] = bytes([(i * 7 + 1) & 0xFF])
    diffs, _ = differential(FUN_1817A, {"_pokes": pokes},
                            lambda lib, buf: lib.func_1817a(buf))
    assert not diffs, report(diffs)


def _f1881e_pokes():
    pokes = {
        0x1A45E: (0x80000).to_bytes(4, "big"),
        0x1A462: (0x78000).to_bytes(4, "big"),
        0x1A7CA: (0x40000).to_bytes(4, "big"),
        0x1A7CE: (0x40020).to_bytes(4, "big"),
        0x1A782: (0x41000).to_bytes(4, "big"),
        0x1AB4F: bytes([0x01]),
        0x1ACD4: bytes([0x00]),
        0x30000: bytes([0x00, 0x01]),
    }
    pokes[0x40000] = bytes([0x10, 0x20, 0x30, 0x40, 0x50, 0, 0, 0])
    pokes[0x40018] = bytes([0x10, 0x20, 0x30, 0x40, 0, 0, 0, 0])
    pokes.update({                                   # tablas de 0x14138
        0x41FB0: (0x12).to_bytes(2, "big"), 0x41FB2: bytes([2, 4]),
        0x1ACF1: bytes([5]), 0x1ACED: bytes([0]), 0x1ACC7: bytes([1]),
        0x1ACC9: bytes([0]), 0x1ACC6: bytes([0x87]),
    })
    for i in range(0x20):
        pokes[0x41FB4 + i] = bytes([(0x10 + i) & 0xFF])
    pokes[0x24982] = bytes([0x01]) * 0x61            # 0x61 entradas vacias
    pokes[0x24982 + 0x61] = bytes([0x04, 0x00, 0x10, 0x03])  # entrada pedida (3 B)
    for i in range(0x8000):
        pokes[0x80000 + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x78000 + i] = bytes([(i * 11 + 7) & 0xFF])
    return pokes


def test_fun_1881e():
    pokes = _f1881e_pokes()
    diffs, info = differential(
        FUN_1881E, {"a6": 0x30000, "_pokes": pokes},
        lambda lib, buf: lib.func_1881e(buf, 0x30000),
        schedule=_vbl_k(0x3C), wait_sites=[VBL_SITE])
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


def _f1834a_pokes():
    pokes = {
        0x1A45E: (0x90000).to_bytes(4, "big"),
        0x1ACD4: bytes([0x00]),
    }
    for i in range(0x100):
        base = 0xD8F0 + i * 0xA
        pokes[base] = (0x100 + i * 0xA).to_bytes(2, "big")       # offset
        pokes[base + 2] = (i & 0xF).to_bytes(2, "big")           # fase
        pokes[base + 4] = (0x0400).to_bytes(2, "big")
        pokes[base + 6] = (0x0200).to_bytes(2, "big")
        vals = (0x100, 0x200, 0x600, 0x800, 0xF00, 0x147, 0x148, 0xD48)
        pokes[base + 8] = vals[i % len(vals)].to_bytes(2, "big")
    for i in range(0x10000):
        pokes[0x90000 + i] = bytes([(i * 3 + 1) & 0xFF])
    return pokes


def test_fun_1834a():
    for d5 in (0x64, 0x10, 0x1E, 0x1D):
        pokes = _f1834a_pokes()
        diffs, _ = differential(
            FUN_1834A, {"d5": d5, "_pokes": pokes},
            lambda lib, buf, v=d5: lib.func_1834a(buf, 0, v),
            schedule=_vbl_k(1), wait_sites=[VBL_SITE])
        assert not diffs, f"d5={d5:#x}\n{report(diffs)}"


def test_fun_18334():
    pokes = _f1834a_pokes()
    diffs, _ = differential(
        FUN_18334, {"_pokes": pokes},
        lambda lib, buf: lib.func_18334(buf),
        schedule=_vbl_k(0x65), wait_sites=[VBL_SITE], max_insns=3_000_000)
    assert not diffs, report(diffs)


FUN_171B4 = 0x171B4
FUN_173AE = 0x173AE
FUN_17678 = 0x17678
FUN_18128 = 0x18128
for _n in ("func_171b4", "func_173ae", "func_17678"):
    getattr(harness._lib, _n).argtypes = [_u8p]
    getattr(harness._lib, _n).restype = None
harness._lib.func_18128.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_18128.restype = ctypes.c_uint32


def _t17_pokes(ptr_addr, ptr_val):
    pokes = {
        ptr_addr: ptr_val.to_bytes(4, "big"),
        0x1ACD2: bytes([0x10]),
        0x1AB60: (0x1000).to_bytes(2, "big"),
        0x1A45E: (0x90000).to_bytes(4, "big"),
        0x1A462: (0x88000).to_bytes(4, "big"),
        0x1AB4F: bytes([0x00]),
        0x1A81E: (0x40000).to_bytes(4, "big"),
        0x1A81A: (0x40100).to_bytes(4, "big"),
        0x1A7CA: (0x41010).to_bytes(4, "big"),
        0x1A7CE: (0x41018).to_bytes(4, "big"),
        0x40000: (0).to_bytes(4, "big"),
        0x41010: bytes([0x10, 0, 0x30, 0, 0x10, 0, 0, 0]),
        0x41018: bytes([0x00, 0, 0, 0, 0, 0, 0, 0]),
    }
    for i in range(0x80):
        pokes[0x1AA72 + i] = bytes([0x00])
    for i in range(16):
        pokes[0x1A776 + i * 4] = (0).to_bytes(4, "big")
    for i in range(0x44):
        pokes[0x23EE8 + i] = bytes([0x01])
    for i in range(0x100):
        pokes[0x1A850 + i] = bytes([0x00])
    for i in range(0x4000):
        pokes[0x90000 + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x88000 + i] = bytes([(i * 11 + 7) & 0xFF])
    return pokes


def test_fun_171b4_173ae():
    for entry, fn, ptr in ((FUN_171B4, harness._lib.func_171b4, 0x1A80A),
                           (FUN_173AE, harness._lib.func_173ae, 0x1A80E)):
        pokes = _t17_pokes(ptr, 0x41000)
        pokes[0x1AA72 + 3] = bytes([0x00])            # cls = 0x10
        pokes[0x41001] = bytes([0x01])
        pokes[0x41002] = bytes([0x00])
        diffs, _ = differential(entry, {"_pokes": pokes},
                                lambda lib, buf, f=fn: f(buf))
        assert not diffs, f"{entry:#x}\n{report(diffs)}"


def _f17678_pokes(bit6):
    pokes = _t17_pokes(0x1A80A, 0x41000)
    pokes[0x1A812] = (0x42000).to_bytes(4, "big")
    pokes[0x1A816] = (0x42100).to_bytes(4, "big")
    pokes[0x1AD00] = bytes([0x01])
    pokes[0x1ACFF] = bytes([0x77])
    pokes[0x1ACE6] = bytes([0x10])
    pokes[0x42000] = bytes([0xAA, bit6, 0, 0, 0x02, 0, 0, 0])
    pokes[0x42100] = bytes([0x10, 0, 0x30, 0, 0x90, 0, 0, 0])
    return pokes


def test_fun_17678():
    for bit6 in (0x00, 0x40):
        pokes = _f17678_pokes(bit6)
        diffs, _ = differential(FUN_17678, {"_pokes": pokes},
                                lambda lib, buf: lib.func_17678(buf))
        assert not diffs, f"bit6={bit6:#x}\n{report(diffs)}"


def test_fun_18128():
    for byte3, ace6 in ((0x30, 0x10), (0x31, 0x10), (0x38, 0x10), (0x30, 0x0A)):
        pokes = _sample_pokes()
        pokes[0x1A772] = (0x42000).to_bytes(4, "big")
        pokes[0x42000] = bytes([0x10, 0, 0x30, byte3, 0x90, 0, 0, 0])
        pokes[0x1ACE6] = bytes([ace6])
        diffs, info = differential(
            FUN_18128, {"a6": 0x30000, "_pokes": pokes},
            lambda lib, buf: lib.func_18128(buf, 0x30000))
        assert not diffs, f"byte3={byte3:#x}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"]


FUN_178CC = 0x178CC
FUN_179A2 = 0x179A2
harness._lib.func_178cc.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_178cc.restype = ctypes.c_uint32
harness._lib.func_179a2.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_179a2.restype = ctypes.c_uint32

TB_SCREEN = 0x80000
TB_TABLE = 0x3EC8A


def _tb_pokes(index, keys=1):
    indices = [index] if isinstance(index, int) else list(index)
    max_idx = max(indices)
    pokes = {
        0x1A46A: (0x8000).to_bytes(4, "big"),
        0x1A450: (0x30).to_bytes(2, "big"),
        0x1A452: (0x40).to_bytes(2, "big"),
        0x1ACC6: bytes([0x87]),
        0x1A44F: bytes([0x00]),
        0x1A456: bytes([0x00]),
        0x1ACD4: bytes([0x05]),
        0x1A74E: bytes([0x00]),
        0x1A45E: TB_SCREEN.to_bytes(4, "big"),
    }
    wanted = set(indices)
    off = 0
    for e in range(max_idx - 3):                # entrada e -> indice e+4
        if (e + 4) in wanted:
            pokes[TB_TABLE + off] = bytes([3, 0x21, 0x22])
            off += 3
        else:
            pokes[TB_TABLE + off] = bytes([1])
            off += 1
    pokes.update(console_keys([("a", 0)] * keys))
    for i in range(0x4000):
        pokes[TB_SCREEN + i] = bytes([(i * 3 + 1) & 0xFF])
    return pokes


def test_fun_178cc():
    for entry4, idx in ((0x02, 0), (0x00, 0)):
        pokes = _sample_pokes()
        pokes[0x1A812] = (0x42000).to_bytes(4, "big")
        pokes[0x42000] = bytes([0x00, 0x00, 0x00, 0x00, entry4, 0, 0, 0])
        pokes[0x1AD01] = bytes([0x00])
        pokes[0x1AD33] = bytes([0x00]) * 4
        if entry4:
            pokes.update(_tb_pokes(0xAA))
            pokes[0x1A812] = (0x42000).to_bytes(4, "big")
            pokes[0x42000] = bytes([0x00, 0x00, 0x00, 0x00, entry4, 0, 0, 0])
            pokes[0x1AD33] = bytes([0x00]) * 4
        else:
            pokes.update(_tb_pokes(0x140))
            pokes[0x1A812] = (0x42000).to_bytes(4, "big")
            pokes[0x42000] = bytes([0x00, 0x00, 0x00, 0x00, entry4, 0, 0, 0])
            pokes[0x1AD33] = bytes([0x00]) * 4
        diffs, info = differential(
            FUN_178CC, {"a6": 0x30000, "_pokes": pokes},
            lambda lib, buf: lib.func_178cc(buf, 0x30000))
        assert not diffs, f"entry4={entry4:#x}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"]


def test_fun_179a2():
    for byte3, keys, idx in ((0x30, 2, (0x147, 0x32)), (0x38, 1, (0x148,))):
        pokes = _sample_pokes()
        pokes[0x1A772] = (0x42000).to_bytes(4, "big")
        pokes[0x42000] = bytes([0x10, 0, 0x30, byte3, 0x90, 0, 0, 0])
        pokes[0x1ACE6] = bytes([0x10])
        pokes[0x1AD01] = bytes([0x00])
        pokes[0x1AD2E] = bytes([0x00])
        pokes.update(_tb_pokes(list(idx), keys=keys))
        pokes[0x1A772] = (0x42000).to_bytes(4, "big")
        pokes[0x42000] = bytes([0x10, 0, 0x30, byte3, 0x90, 0, 0, 0])
        pokes[0x1ACE6] = bytes([0x10])
        diffs, info = differential(
            FUN_179A2, {"a6": 0x30000, "_pokes": pokes},
            lambda lib, buf: lib.func_179a2(buf, 0x30000))
        assert not diffs, f"byte3={byte3:#x}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"]


FUN_181F0 = 0x181F0
harness._lib.func_181f0.argtypes = [_u8p]
harness._lib.func_181f0.restype = None


def _f181f0_pokes():
    pokes = {
        0x1A45E: (0x90000).to_bytes(4, "big"),
        0x1A579: bytes([0x00]),
        0x1A49E: (0).to_bytes(2, "big"),
        0x1A4A0: (0).to_bytes(2, "big"),
        0x1A4A2: (0).to_bytes(2, "big"),
    }
    pokes[0x2741E + 1] = bytes([0x00])
    for i in range(2, 0x12):
        pokes[0x2741E + i] = bytes([0xFF])
    for i in range(0x10):
        pokes[0x198C0 + i * 4] = (0x10).to_bytes(2, "big")
        pokes[0x198C0 + i * 4 + 2] = (0x10).to_bytes(2, "big")
    for i in range(0x10000):
        pokes[0x90000 + i] = bytes([(i * 3 + 1) & 0xFF])
    return pokes


def test_fun_181f0():
    pokes = _f181f0_pokes()
    diffs, _ = differential(FUN_181F0, {"_pokes": pokes},
                            lambda lib, buf: lib.func_181f0(buf))
    assert not diffs, report(diffs)


FUN_1682A = 0x1682A
FUN_1842C = 0x1842C
harness._lib.reset_clear.argtypes = [_u8p]
harness._lib.reset_clear.restype = None
harness._lib.prepare_room.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.prepare_room.restype = ctypes.c_uint32


def test_fun_1682a():
    data = bytes((i * 7 + 1) & 0xFF for i in range(0x291C))
    pokes, _h = stage_files([("A:CLEAR.BIN", data, 0x291C)])
    pokes[0x1A439] = b"A:CLEAR.BIN\0"
    pokes[0x1A45E] = (0x80000).to_bytes(4, "big")
    pokes[0x1A579] = bytes([0x00])
    pokes[0x2741E + 1] = bytes([0x42])
    diffs, _ = differential(FUN_1682A, {"_pokes": pokes},
                            lambda lib, buf: lib.reset_clear(buf),
                            stop_pc=0x10158)
    assert not diffs, report(diffs)


def test_fun_1842c():
    fond = bytes((i * 11 + 3) & 0xFF for i in range(0x7D22))
    clear = bytes((i * 5 + 1) & 0xFF for i in range(0x291C))
    pokes, _h = stage_files([("FOND.PI1", fond, 0x7D22),
                             ("A:CLEAR.BIN", clear, 0x291C)])
    pokes[0x1A41E] = b"FOND.PI1\0"
    pokes[0x1A439] = b"A:CLEAR.BIN\0"
    pokes[0x1A45E] = (0x80000).to_bytes(4, "big")
    pokes[0x1A462] = (0x78000).to_bytes(4, "big")
    pokes[0x1A466] = (0x40000).to_bytes(4, "big")
    pokes[0x1A57C] = bytes([0xFF])
    pokes[0x1A4E8 + 8] = (0x55B0A).to_bytes(4, "big")
    pokes[0x55B0A] = bytes([0x00, 0x08, 0x03, 0x55, 0x11, 0x22, 0x33, 0x44])
    pokes[0x1A579] = bytes([0x00])
    pokes[0x1AB4F] = bytes([0x00])
    for i in range(0x12):
        pokes[0x2741E + i] = bytes([0xFF])
    for i in range(0x10):
        pokes[0x198C0 + i * 4] = (0x10).to_bytes(2, "big")
        pokes[0x198C0 + i * 4 + 2] = (0x10).to_bytes(2, "big")
    for i in range(0x44):
        pokes[0x23EE8 + i] = bytes([0x01])
    for i in range(16):
        pokes[0x1A776 + i * 4] = (0).to_bytes(4, "big")
    for i in range(0x100):
        pokes[0x1A850 + i] = bytes([0x00])
    for i in range(0x10000):
        pokes[0x80000 + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x78000 + i] = bytes([(i * 11 + 7) & 0xFF])
    diffs, info = differential(
        FUN_1842C, {"a6": 0x30000, "_pokes": pokes},
        lambda lib, buf: lib.prepare_room(buf, 0x30000),
        schedule=_vbl_k(0x65), wait_sites=[VBL_SITE], max_insns=3_000_000,
        stop_pc=0x10158)
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


FUN_171FA = 0x171FA
FUN_173F2 = 0x173F2
FUN_175F6 = 0x175F6
for _n in ("func_171fa", "func_173f2", "func_175f6"):
    getattr(harness._lib, _n).argtypes = [_u8p, ctypes.c_uint32]
    getattr(harness._lib, _n).restype = ctypes.c_uint32


def _f17x_common():
    pokes = {
        0x1ACC6: bytes([0x03]),
        0x1AB60: (0x1000).to_bytes(2, "big"),
        0x1A80A: (0x43000).to_bytes(4, "big"),
        0x1A80E: (0x44000).to_bytes(4, "big"),
        0x1A7CA: (0x41010).to_bytes(4, "big"),
        0x1A7CE: (0x41030).to_bytes(4, "big"),
        0x1A81E: (0x40000).to_bytes(4, "big"),
        0x1A81A: (0x42000).to_bytes(4, "big"),
        0x40000: (0).to_bytes(4, "big"),
        0x42000: bytes([0x10, 0, 0x30, 0, 0x90, 0, 0, 0]),
        0x1A45E: (0x90000).to_bytes(4, "big"),
        0x1A462: (0x88000).to_bytes(4, "big"),
        0x1AB4F: bytes([0x00]),
        0x1AD00: bytes([0x01]),
        0x1A579: bytes([0x00]),
        0x1ACE6: bytes([0x10]),
        0x1ACFC: bytes([0x00]),
        0x43004: bytes([0x01, 0x00, 0x00]),
        0x44004: bytes([0x01, 0x00, 0x00]),
    }
    for i in range(0x100):
        pokes[0x1AA72 + i] = bytes([0x00])
    pokes[0x1AA75] = bytes([0x00])              # cls(0) = 0x10
    pokes[0x1AA76] = bytes([0x00])              # cls(1) = 0x10
    pokes[0x1AA8C] = bytes([0x03])
    for i in range(16):
        pokes[0x1A776 + i * 4] = (0).to_bytes(4, "big")
    for i in range(0x44):
        pokes[0x23EE8 + i] = bytes([0x01])
    for i in range(0x13):
        a = 0x19CD1 + i * 2
        pokes[a] = bytes([0x03 if i == 0 else 0x40 + i, 0x02 if i == 0 else 0x00])
    for i in range(3):
        pokes[0x41010 + i * 8] = bytes([0x10, 0x20, 0x30, 0x40, 0x10, 0, 0, 0])
    for i in range(0x4000):
        pokes[0x90000 + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x88000 + i] = bytes([(i * 11 + 7) & 0xFF])
    return pokes


def test_fun_171fa():
    for early, mode, val in ((True, 0x03, 0x02), (False, 0x03, 0x02),
                             (True, 0x77, 0x02)):
        pokes = _f17x_common()
        pokes[0x1ACC6] = bytes([mode])
        if early:
            pokes[0x1AA72] = bytes([0x55])
        else:
            pokes[0x1AB21] = bytes([0x00])
            pokes[0x1AB22] = bytes([0x03])
            pokes[0x4300A] = bytes([0x01, 0x00, 0x00])
        pokes[0x1AA73] = bytes([0x03])
        diffs, info = differential(
            FUN_171FA, {"a6": 0x30000, "_pokes": pokes},
            lambda lib, buf: lib.func_171fa(buf, 0x30000))
        assert not diffs, f"early={early} mode={mode:#x}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"]


def test_fun_173f2():
    for mode in (0x03, 0x77):
        pokes = _f17x_common()
        pokes[0x1ACC6] = bytes([mode])
        pokes[0x1AA8C] = bytes([0x03])
        diffs, info = differential(
            FUN_173F2, {"a6": 0x30000, "_pokes": pokes},
            lambda lib, buf: lib.func_173f2(buf, 0x30000))
        assert not diffs, f"mode={mode:#x}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"]


def test_fun_175f6():
    for acff in (0, 7):
        pokes = _f17x_common()
        pokes[0x1A812] = (0x42000).to_bytes(4, "big")
        pokes[0x1ABF8] = (0).to_bytes(2, "big")
        pokes[0x1ACF2] = bytes([0x00])
        pokes[0x1ACE7] = bytes([0x55])
        pokes[0x1ACFF] = bytes([acff])
        diffs, info = differential(
            FUN_175F6, {"a6": 0x30000, "_pokes": pokes},
            lambda lib, buf: lib.func_175f6(buf, 0x30000))
        assert not diffs, f"acff={acff}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"]


FUN_16250 = 0x16250
FUN_13686 = 0x13686
FUN_14EE8 = 0x14EE8
FUN_16AB6 = 0x16AB6
FUN_18F0C = 0x18F0C
for _n in ("func_16250", "ikbd_packet_copy", "func_16ab6", "func_18f0c"):
    getattr(harness._lib, _n).argtypes = (
        [_u8p, ctypes.c_uint32, ctypes.c_uint32] if _n in ("func_16ab6", "func_18f0c")
        else ([_u8p, ctypes.c_uint32] if _n == "ikbd_packet_copy" else [_u8p]))
    getattr(harness._lib, _n).restype = None
harness._lib.func_14ee8.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_14ee8.restype = ctypes.c_uint32

for _n, _e in (("func_1699c", 0x1699C), ("func_169a4", 0x169A4),
               ("func_169ac", 0x169AC), ("func_169b4", 0x169B4)):
    getattr(harness._lib, _n).argtypes = [_u8p]
    getattr(harness._lib, _n).restype = None


def _f16250_pokes():
    pokes = {
        0x1A579: bytes([0x00]),
        0x1AB4F: bytes([0x01]),
        0x1ACD4: bytes([0x00]),
    }
    for i in range(0x12):
        pokes[0x2741E + i] = bytes([0xFF])
    return pokes


def test_fun_16250():
    pokes = _f16250_pokes()
    diffs, _ = differential(FUN_16250, {"_pokes": pokes},
                            lambda lib, buf: lib.func_16250(buf),
                            schedule=_vbl_k(1), wait_sites=[VBL_SITE])
    assert not diffs, report(diffs)


def test_fun_13686():
    pokes = {0x42000: bytes([0x11, 0x22, 0x33, 0x44, 0x55, 0x66])}
    diffs, _ = differential(FUN_13686, {"a0": 0x42000, "_pokes": pokes},
                            lambda lib, buf: lib.ikbd_packet_copy(buf, 0x42000))
    assert not diffs, report(diffs)


def test_fun_14ee8():
    for sel, param, base in ((1, 1, 0xA035), (1, 0, 0), (2, 3, 0), (1, 1, 0)):
        pokes = {
            0x1ACC9: bytes([sel]),
            0x30000: bytes([0x00, param]),
        }
        for i in range(0x96):
            pokes[0x1AB62 + i] = bytes([0x00])
        pokes[0x1AB62 + (sel - 1) * 10 + param * 2] = ((0xA035 if base else 0)).to_bytes(2, "big")
        for i in range(0x60):
            pokes[0x1AB50 + i] = bytes([0x00])
        if not base:
            pokes[0x1AB50 + param * 2] = (0).to_bytes(2, "big")
        pokes[0x1A50E] = (0).to_bytes(2, "big")
        diffs, info = differential(
            FUN_14EE8, {"a6": 0x30000, "_pokes": pokes},
            lambda lib, buf: lib.func_14ee8(buf, 0x30000))
        assert not diffs, f"sel={sel} param={param} base={base}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"]


def _f169_pokes(cls2, hit=True):
    pokes = {
        0x1ACC9: bytes([0x00]),
        0x1ACE6: bytes([0x00]),
        0x1A772: (0x42000).to_bytes(4, "big"),
        0x42000: bytes([0x10, 0, cls2, 0, 0x90, 0, 0, 0]),
        0x1A50E: (0).to_bytes(2, "big"),
        0x1AB4F: bytes([0x00]),
        0x1A44F: bytes([0xFF]),
        0x1A456: bytes([0x00]),
        0x1A570: bytes([0x00]),
        VBL_COUNTER: bytes([0x10]),
        0x1AB44: bytes([0x21 if hit else 0x75]),
        0x1AB45: bytes([0x21 if hit else 0x75]),
        0x1AB46: bytes([0x64]), 0x1AB47: bytes([0x64]),
        0x1AB48: bytes([0x75]),
        0x1A7CA: (0x40000).to_bytes(4, "big"),
        0x1A7CE: (0x40008).to_bytes(4, "big"),
        0x1A450: (0).to_bytes(2, "big"),
        0x1A452: (0).to_bytes(2, "big"),
    }
    if hit:
        pokes[0x40000] = bytes([0x00, 0x02, 0x00, 0x02, 0x90, 0x21, 0, 0])
        pokes[0x1AA73] = bytes([0x01])
    else:
        pokes[0x40000] = bytes(8)
    for a in (0x1AC0A, 0x1AC32, 0x1AC5A, 0x1AC82):
        for i in range(0x28):
            pokes[a + i * 2] = (0xA035).to_bytes(2, "big")
    return pokes


def test_fun_1699c_169b4():
    for entry, fn in ((0x1699C, harness._lib.func_1699c),
                      (0x169A4, harness._lib.func_169a4),
                      (0x169AC, harness._lib.func_169ac),
                      (0x169B4, harness._lib.func_169b4)):
        pokes = _f169_pokes(0x2A)
        pokes[0x1ACC9] = bytes([0x01])
        pokes[0x1ACE6] = bytes([0x01])
        diffs, _ = differential(
            entry, {"_pokes": pokes},
            lambda lib, buf, f=fn: f(buf),
            schedule=_wait_schedule(), wait_sites=[IKBD_SITE, VBL_SITE])
        assert not diffs, f"{entry:#x}\n{report(diffs)}"


def test_fun_169_rama91183():
    pokes = _f169_pokes(0x2A, hit=False)
    pokes.update(_tb_pokes([0x183], keys=4))
    pokes[0x1ACC9] = bytes([0x00])
    pokes[0x1ACE6] = bytes([0x01])
    diffs, _ = differential(
        0x169B4, {"_pokes": pokes}, lambda lib, buf: lib.func_169b4(buf),
        schedule=_wait_schedule(), wait_sites=[IKBD_SITE, VBL_SITE])
    assert not diffs, report(diffs)


def _f18f0c_pokes():
    pokes = {
        0x192BE: (0x64C8E).to_bytes(4, "big"),
        0x1AB4F: bytes([0x01]),
        0xD8F0: bytes([0x40, 0x21]),
    }
    for i in range(10):
        pokes[0x55B0A + i * 4] = bytes([0x00, 0x04, 0xAA + i, 0xBB])
    for i in range(8):
        pokes[0x1998A + i * 2] = bytes([i, 0x10 + i])
    return pokes


def test_fun_18f0c_16ab6():
    """0x16ab6 (con el push de registros) verifica el cuerpo 0x18f0c compartido."""
    pokes = _f18f0c_pokes()
    diffs, _ = differential(
        FUN_16AB6, {"a3": 0x1998A, "d7": 8, "_pokes": pokes},
        lambda lib, buf: lib.func_16ab6(buf, 0x1998A, 8))
    assert not diffs, report(diffs)


FUN_13F2A = 0x13F2A
FUN_14E66 = 0x14E66
harness._lib.func_13f2a.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_13f2a.restype = ctypes.c_uint32
harness._lib.func_14e66.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_14e66.restype = ctypes.c_uint32


def test_fun_13f2a():
    pokes = {
        0x1ACF1: bytes([5]),
        0x1A45E: (0x80000).to_bytes(4, "big"),
        0x1A462: (0x78000).to_bytes(4, "big"),
        0x1AB4F: bytes([0x01]),
    }
    pokes.update({
        0x41FB0: (0x12).to_bytes(2, "big"), 0x41FB2: bytes([2, 4]),
        0x1ACC7: bytes([1]), 0x1ACC9: bytes([0]), 0x1ACE6: bytes([0x10]),
    })
    for i in range(0x20):
        pokes[0x41FB4 + i] = bytes([(0x10 + i) & 0xFF])
    pokes[0x24982] = bytes([0x01]) * 0x5B
    pokes[0x24982 + 0x5B] = bytes([0x04, 0x00, 0x10, 0x03])
    for i in range(0x10000):
        pokes[0x80000 + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x78000 + i] = bytes([(i * 11 + 7) & 0xFF])
    diffs, info = differential(
        FUN_13F2A, {"a6": 0x30000, "_pokes": pokes},
        lambda lib, buf: lib.func_13f2a(buf, 0x30000))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


def test_fun_14e66():
    pokes = {
        0x1A456: bytes([0x00]),
        0x1A570: bytes([0x00]),
        0x1AB4F: bytes([0x00]),
        VBL_COUNTER: bytes([0x10]),
        0x1AB44: bytes([0x75]), 0x1AB45: bytes([0x75]),
        0x1AB46: bytes([0x64]), 0x1AB47: bytes([0x64]),
        0x1AB48: bytes([0x75]),
        0x1A772: (0xDEADBEEF).to_bytes(4, "big"),
        0x1A7CA: (0x40000).to_bytes(4, "big"),
        0x1A7CE: (0x40008).to_bytes(4, "big"),
        0x1A44F: bytes([0xFF]),
        0x1A45E: (0x80000).to_bytes(4, "big"),
        0x1A462: (0x78000).to_bytes(4, "big"),
        0x1A46A: (0).to_bytes(4, "big"),
        0x1ACC6: bytes([0x87]),
    }
    pokes[0x40000] = bytes(8)
    for i in range(0x10000):
        pokes[0x80000 + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x78000 + i] = bytes([(i * 11 + 7) & 0xFF])
    diffs, info = differential(
        FUN_14E66, {"a6": 0x30000, "_pokes": pokes},
        lambda lib, buf: lib.func_14e66(buf, 0x30000),
        schedule=_wait_schedule(), wait_sites=[IKBD_SITE, VBL_SITE])
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


FUN_16310 = 0x16310
FUN_14F5E = 0x14F5E
harness._lib.func_16310.argtypes = [_u8p]
harness._lib.func_16310.restype = None
harness._lib.func_14f5e.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_14f5e.restype = ctypes.c_uint32

PAL_BUF = 0x40000


def _f16310_pokes(color, target):
    pokes = {
        0x1A466: PAL_BUF.to_bytes(4, "big"),
        0x1ACF4: bytes([0x00]),
        0x1AB4F: bytes([0x01]),
        0x1ABFA: (0).to_bytes(2, "big"),
        0x1AD0F: bytes([0x00]),
        0x1ACD4: bytes([0x00]),
        0x1A50E: (0).to_bytes(2, "big"),
    }
    for i in range(5):
        pokes[PAL_BUF + 2 + i * 2] = color.to_bytes(2, "big")
        pokes[0x1A10A + i * 2] = target.to_bytes(2, "big")
    return pokes


def test_fun_16310():
    for color, target, waits in ((0x777, 0x777, 0), (0x111, 0x777, 6),
                                 (0x777, 0x111, 6)):
        pokes = _f16310_pokes(color, target)
        sched = _vbl_k(waits) if waits else []
        diffs, _ = differential(
            FUN_16310, {"_pokes": pokes},
            lambda lib, buf: lib.func_16310(buf),
            schedule=sched, wait_sites=[VBL_SITE] if sched else None,
            max_insns=3_000_000)
        assert not diffs, f"color={color:#x} target={target:#x}\n{report(diffs)}"


def test_fun_14f5e():
    pokes = _f16310_pokes(0x777, 0x777)
    pokes[0x1AD20] = bytes([0x15])
    diffs, info = differential(
        FUN_14F5E, {"a6": 0x30000, "_pokes": pokes},
        lambda lib, buf: lib.func_14f5e(buf, 0x30000))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


FUN_16ABE = 0x16ABE
FUN_14F74 = 0x14F74
harness._lib.func_16abe.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_16abe.restype = ctypes.c_uint32
harness._lib.func_14f74.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_14f74.restype = ctypes.c_uint32


def _grid_pokes():
    pokes = {
        0x1AB40: bytes([0x00]), 0x1AB41: bytes([0x40]),
        0x1AB42: bytes([0x00]), 0x1AB43: bytes([0x3C]),
        0x1A45E: (0x80000).to_bytes(4, "big"),
        0x1A462: (0x78000).to_bytes(4, "big"),
        0x1AB4F: bytes([0x01]),
        0x30000: bytes([0x00, 0x00]),
    }
    for i in range(0x50):                        # 0x50 entradas de longitud 4
        pokes[0x41FB0 + i * 4] = bytes([0x00, 0x04, 0x00, 0x00])
    pokes[0x41FB0 + 0x50 * 4] = bytes([0x00, 0x08, 0x02, 0x02,
                                       0x11, 0x22, 0x33, 0x44,
                                       0x55, 0x66, 0x77, 0x88])
    for i in range(0x8000):
        pokes[0x80000 + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x78000 + i] = bytes([(i * 11 + 7) & 0xFF])
    return pokes


def test_fun_16abe():
    pokes = _grid_pokes()
    diffs, info = differential(
        FUN_16ABE, {"a6": 0x30000, "_pokes": pokes},
        lambda lib, buf: lib.func_16abe(buf, 0x30000))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


def test_fun_14f74():
    pokes = _grid_pokes()
    pokes.update({
        0x1AD20: bytes([0x15]),
        0x1AD09: bytes([0x00]),
        0x1ACC7: bytes([0x00]),
        0x1A466: (0x40000).to_bytes(4, "big"),
        0x40002: bytes([0x00, 0x00]),
        0x1ACEC: bytes([0x40]),
        0x1ACD2: bytes([0x01]),
        0x1AB50: (0).to_bytes(2, "big"),
        0x1ACC9: bytes([0x00]),
        0x1AB4F: bytes([0x00]),
        0x1A57C: bytes([0xFF]),
        0x1A4E8: (0x55B0A).to_bytes(4, "big"),
        0x1A44F: bytes([0xFF]),
        0x1A456: bytes([0x00]),
        0x1A570: bytes([0x00]),
        VBL_COUNTER: bytes([0x10]),
        0x1AB44: bytes([0x75]), 0x1AB45: bytes([0x75]),
        0x1AB46: bytes([0x64]), 0x1AB47: bytes([0x64]),
        0x1AB48: bytes([0x75]),
        0x1A7CA: (0x40000).to_bytes(4, "big"),
        0x1A7CE: (0x40008).to_bytes(4, "big"),
        0x1A772: (0xDEADBEEF).to_bytes(4, "big"),
    })
    pokes[0x55B0A] = bytes([0x00, 0x08, 0x03, 0x55, 0x11, 0x22, 0x33, 0x44])
    pokes[0x40000] = bytes(8)
    diffs, info = differential(
        FUN_14F74, {"a6": 0x30000, "_pokes": pokes},
        lambda lib, buf: lib.func_14f74(buf, 0x30000),
        schedule=_wait_schedule(), wait_sites=[IKBD_SITE, VBL_SITE],
        max_insns=3_000_000)
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


FUN_18A30 = 0x18A30
FUN_16A20 = 0x16A20
harness._lib.func_18a30.argtypes = [_u8p]
harness._lib.func_18a30.restype = None
harness._lib.func_16a20.argtypes = [_u8p]
harness._lib.func_16a20.restype = None


def test_fun_18a30():
    pokes = {
        0x1A45E: (0x80000).to_bytes(4, "big"),
        0x1A462: (0x78000).to_bytes(4, "big"),
        0x1A579: bytes([0x00]),
        0x1AB4F: bytes([0x01]),
        0x1ACD4: bytes([0x00]),
    }
    for i in range(0x12):
        pokes[0x2741E + i] = bytes([0xFF])
    for i in range(0x8000):
        pokes[0x80000 + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x78000 + i] = bytes([(i * 11 + 7) & 0xFF])
    diffs, _ = differential(
        FUN_18A30, {"_pokes": pokes},
        lambda lib, buf: lib.func_18a30(buf),
        schedule=_vbl_k(10), wait_sites=[VBL_SITE])
    assert not diffs, report(diffs)


def _f16a20_pokes():
    pokes = {
        0x1A57C: bytes([0xFF]),
        0x1A579: bytes([0x00]),
        0x1AB4F: bytes([0x01]),
        0x1ACD4: bytes([0x00]),
        0x1A45E: (0x80000).to_bytes(4, "big"),
        0x1A462: (0x78000).to_bytes(4, "big"),
        0x1998A: bytes([0x10, 0x20, 0x00, 0x11, 0x01, 0x22]),
        0x41FB0: (0x0002).to_bytes(2, "big"),
        0x41FB2: bytes([0x02, 0x02]),
        0xD8F0: bytes([0x40, 0x21]),
    }
    for i, a in enumerate((0x1997E, 0x19981, 0x19984, 0x19987)):
        pokes[a] = bytes([0x00, 0x10, 0x30])
    for i in range(8):
        pokes[0x41FB4 + i] = bytes([(0x20 + i) & 0xFF])
    for i in range(10):
        pokes[0x55B0A + i * 4] = bytes([0x00, 0x04, 0xAA + i, 0xBB])
    for i in range(0x8000):
        pokes[0x80000 + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x78000 + i] = bytes([(i * 11 + 7) & 0xFF])
    return pokes


def test_fun_16a20():
    pokes = _f16a20_pokes()
    diffs, _ = differential(
        FUN_16A20, {"_pokes": pokes},
        lambda lib, buf: lib.func_16a20(buf),
        schedule=_vbl_k(22), wait_sites=[VBL_SITE], max_insns=3_000_000)
    assert not diffs, report(diffs)


FUN_176C0 = 0x176C0
FUN_17C42 = 0x17C42
FUN_185E6 = 0x185E6
harness._lib.func_176c0.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_176c0.restype = ctypes.c_uint32
harness._lib.func_17c42.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_17c42.restype = ctypes.c_uint32
harness._lib.track_name_print.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.track_name_print.restype = ctypes.c_uint32


def _f176c0_pokes():
    pokes = _f17x_common()
    pokes.update({
        0x1A466: (0x30000).to_bytes(4, "big"),
        0x1ACF4: bytes([3]),
        0x1A57C: bytes([0xFF]),
        0x1ACF7: bytes([0x01]),
        0x1A81E: (0x50000).to_bytes(4, "big"),
        0x50000: (0x50010).to_bytes(4, "big"),
        0x50010: (bytes([2, 2]) + (0x88000).to_bytes(4, "big") + bytes([0, 0]) +
                  (4).to_bytes(2, "big") + bytes(range(0x20, 0x30))),
    })
    for i in range(100):
        pokes[0x1D3C2 + i] = bytes([(0x40 + i) & 0xFF])
    for i in range(32):
        pokes[0x30000 + i] = bytes([0xAA])
    return pokes


def test_fun_176c0():
    for mode in ("icon", "sound", "skip"):
        pokes = _f176c0_pokes()
        if mode == "icon":
            pokes[0x1A57B] = bytes([0x01])
        elif mode == "sound":
            pokes[0x1A57B] = bytes([0x00])
            pokes[0x1ACC7] = bytes([0x3D])
        else:
            pokes[0x1AD09] = bytes([0x12])
        diffs, info = differential(
            FUN_176C0, {"a6": 0x30000, "_pokes": pokes},
            lambda lib, buf: lib.func_176c0(buf, 0x30000))
        assert not diffs, f"mode={mode}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], f"mode={mode}"


def _f17c42_pokes():
    pokes = _f176c0_pokes()
    pokes[0x1A57B] = bytes([0x00])
    pokes[0x1ACC7] = bytes([0x3D])
    pokes.update({
        0x1A7CA: (0x40000).to_bytes(4, "big"),
        0x1A7CE: (0x40010).to_bytes(4, "big"),
        0x1A772: (0x43000).to_bytes(4, "big"),
        0x1AA70: (0x0000).to_bytes(2, "big"),
        0x1ACD2: bytes([0x5A]),
        0x40000: bytes([8, 0x20, 4, 0x20, 0x50, 0, 0, 0]),
        0x40008: bytes([0x30, 0x40, 0x30, 0x40, 0x10, 0, 0, 0]),
        0x40010: bytes([0x30, 0x40, 0x30, 0x40, 0x10, 0, 0, 0]),
        0x43000: bytes([0x10, 0x00, 0x30]),
    })
    for i in range(16):
        pokes[0x1A776 + i * 4] = (0x38000).to_bytes(4, "big")
    pokes[0x38000] = (bytes([2, 3]) + (0x90000).to_bytes(4, "big") +
                      (4).to_bytes(2, "big") + (4).to_bytes(4, "big") +
                      bytes(range(0x20, 0x38)))
    pokes[0x3A000] = (bytes([2, 3]) + (0x88000).to_bytes(4, "big") +
                      (8).to_bytes(2, "big") + (4).to_bytes(4, "big"))
    pokes[0x0D8EA] = bytes([0x41])
    pokes[0x1D402] = bytes([0x5A])
    return pokes


def test_fun_17c42():
    diffs, info = differential(
        FUN_17C42, {"a6": 0x30000, "_pokes": _f17c42_pokes()},
        lambda lib, buf: lib.func_17c42(buf, 0x30000))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


def _f185e6_pokes():
    pokes = {
        0x1AD20: bytes([0x15]),
        0x1AD09: bytes([0x00]),
        0x1ACC6: bytes([0x87]),
        0x1A57C: bytes([0xFF]),
        0x1AB4F: bytes([0x00]),
        0x1ACD4: bytes([0x00]),
        0x1A45E: (0x80000).to_bytes(4, "big"),
        0x1A462: (0x88000).to_bytes(4, "big"),
        0x1ACE9: bytes([0x10]),
        0x1ACD7: bytes([0x65]),
        0x1AB60: (0x1000).to_bytes(2, "big"),
        0x1A81A: (0x42000).to_bytes(4, "big"),
        0x42000: bytes([0x10, 0x00, 0x30, 0x00, 0x90, 0, 0, 0]),
        0x1A482: (0x0100).to_bytes(2, "big"),
        0x199B5: bytes([0x00]),
        0x1A850: (1).to_bytes(2, "big"),
        0x1A851: bytes([0x11]),
        0x1AD0A: bytes([0x00]),
    }
    for i in range(0x150):
        pokes[0x3C534 + i * 3] = bytes([3, 0x21, 0x22])
    for i in range(0x44):
        pokes[0x23EE8 + i] = bytes([0x01])
    for i in range(0x8000):
        pokes[0x80000 + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x88000 + i] = bytes([(i * 11 + 7) & 0xFF])
    pokes.update(console_key("a"))
    return pokes


def test_fun_185e6():
    for ab60 in (0x1000, 0x2000):
        pokes = _f185e6_pokes()
        pokes[0x1AB60] = ab60.to_bytes(2, "big")
        diffs, _ = differential(
            FUN_185E6, {"a6": 0x52000, "_pokes": pokes},
            lambda lib, buf: lib.track_name_print(buf, 0x52000))
        assert not diffs, f"ab60={ab60:#x}\n{report(diffs)}"


FUN_16DEC = 0x16DEC
FUN_189AA = 0x189AA
harness._lib.object_info_card.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.object_info_card.restype = ctypes.c_uint32
harness._lib.object_info_wait.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.object_info_wait.restype = ctypes.c_uint32


def _hint_tbl(pokes):
    for i in range(0x100):
        pokes.pop(0x1A850 + i, None)
    p = {0x1A851: bytes([0x11]), 0x1AD0A: bytes([0x00])}
    for i in range(10):
        base = 0x1A850 + i * 0x10
        p[base] = ((i + 1) & 0xFFFF).to_bytes(2, "big")
        p[base + 2] = (0x90000 + i * 0x10).to_bytes(4, "big")
        for k, v in enumerate((1, 2, 3, 4, 5)):
            p[base + 6 + k * 2] = v.to_bytes(2, "big")
    pokes.update(p)


def _f16dec_pokes(e0=1, e1=0, d4=0x10, d2=8, skip=0x24):
    pokes = _f16902_pokes(0x10)
    pokes.update({
        0x1A772: (0x43000).to_bytes(4, "big"),
        0x43000: bytes([0x00, 0x00, 0x00, 0x30]),
        0x1A484: (0).to_bytes(2, "big"),
        0x246E6: bytes([2, e0, e1]),
        0x52000: bytes([0x00, d2, 0x01, d4]),
        0x2751E: bytes([5, 0, 0, 0x00, 0x00]),
        0x1A252: bytes([3, 3]),
        0x1A254: (0x1111).to_bytes(2, "big"),
        0x1A256: (0x2222).to_bytes(2, "big"),
        0x1A258: (0x3333).to_bytes(2, "big"),
        0x1A25A: (0x4444).to_bytes(2, "big"),
        0x31072: (0x10).to_bytes(2, "big"),
        0x31074: bytes([2, 2]),
    })
    for i in range(0x100):
        pokes[0x31076 + i] = bytes([(i * 7 + 3) & 0xFF])
    for i in range(0x1400):
        pokes[0x3B1AE + i] = bytes([(i * 5 + 1) & 0xFF])
    for i in range(24):
        pokes[0x19E19 + i * 6] = bytes([0x80])
    _hint_tbl(pokes)
    return pokes


def test_fun_16dec():
    pokes = _f16dec_pokes()
    diffs, info = differential(
        FUN_16DEC, {"a6": 0x52000, "_pokes": pokes},
        lambda lib, buf: lib.object_info_card(buf, 0x52000),
        schedule=_vbl_k(1), wait_sites=[VBL_SITE], max_insns=1_200_000)
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


def test_fun_16dec_pista():
    pokes = _f16dec_pokes(e0=0x25)
    for i in range(0x24):
        pokes[0x2751E + 5 * i] = bytes([5])
    pokes[0x2751E + 5 * 0x24] = bytes([5, 0, 0, 0x00, 0x00])
    pokes[0x1A45E] = (0x90000).to_bytes(4, "big")
    pokes[0x1A462] = (0x78000).to_bytes(4, "big")
    for i in range(0x100):
        pokes[0x90000 + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x78000 + i] = bytes([(i * 11 + 7) & 0xFF])
        pokes[0x98000 + i] = bytes([(i * 13 + 5) & 0xFF])
    diffs, info = differential(
        FUN_16DEC, {"a6": 0x52000, "_pokes": pokes},
        lambda lib, buf: lib.object_info_card(buf, 0x52000),
        schedule=_vbl_k(11), wait_sites=[VBL_SITE])
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


def _f189aa_pokes():
    pokes = _f16902_pokes(0x10)
    pokes.update({
        0x1A570: bytes([0x01]),
        0x1A456: bytes([0x00]),
        0x1A44F: bytes([0xFF]),
        0x1A45E: (0x90000).to_bytes(4, "big"),
        0x1A462: (0x78000).to_bytes(4, "big"),
        0x1A484: (0).to_bytes(2, "big"),
        0x1AD20: bytes([0x00]),
        0x19704: bytes([0x00, 0x01, 0x08, 0x04]),
        0x19708: bytes([0x00, 0x08, 0x00, 0x10]),
        0x2751E: bytes([5, 0, 0, 0x00, 0x00]),
        0x1A252: bytes([3, 3]),
        0x1A254: (0x1111).to_bytes(2, "big"),
        0x1A256: (0x2222).to_bytes(2, "big"),
        0x1A258: (0x3333).to_bytes(2, "big"),
        0x1A25A: (0x4444).to_bytes(2, "big"),
        0x31072: (0x10).to_bytes(2, "big"),
        0x31074: bytes([2, 2]),
    })
    for i in range(0x100):
        pokes[0x31076 + i] = bytes([(i * 7 + 3) & 0xFF])
    for i in range(0x1400):
        pokes[0x3B1AE + i] = bytes([(i * 5 + 1) & 0xFF])
    off = 0
    for i in range(0x27):                       # A (a6=0x19708) salta 0x27 entradas
        pokes[0x246E6 + off] = bytes([3])
        off += 3
    pokes[0x246E6 + off] = bytes([2, 1, 0])     # entrada de A en +0x75
    off += 2
    for i in range(0x0d):  # B (a6=0x1970c) salta 0x35: entradas 40..52
        pokes[0x246E6 + off] = bytes([3])
        off += 3
    pokes[0x246E6 + off] = bytes([2, 1, 0])     # entrada de B en +0x9e
    off += 2
    for i in range(0x100):
        pokes[0x78000 + i] = bytes([(i * 11 + 7) & 0xFF])
        pokes[0x98000 + i] = bytes([(i * 13 + 5) & 0xFF])
    _hint_tbl(pokes)
    return pokes


def test_fun_189aa():
    pokes = _f189aa_pokes()
    schedule = _vbl_k(3) + [
        {"pc": 0x136B0, "nth": 1, "addr": 0x1A44F, "width": 1, "value": 0x04},
    ]
    diffs, info = differential(
        FUN_189AA, {"a6": 0x52000, "_pokes": pokes},
        lambda lib, buf: lib.object_info_wait(buf, 0x52000),
        schedule=schedule, wait_sites=[VBL_SITE, 0x136B0], max_insns=3_000_000)
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


FUN_17CA4 = 0x17CA4
harness._lib.func_17ca4.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_17ca4.restype = ctypes.c_uint32


def _f17ca4_pokes(obj2=0x40, obj3=0x20, obj4=0x83):
    pokes = _f17x_common()
    pokes.update({
        0x1A772: (0x43000).to_bytes(4, "big"),
        0x43000: bytes([0x00, 0x00, obj2, obj3, obj4, 0x00]),
        0x1A81A: (0x44000).to_bytes(4, "big"),
        0x44000: bytes([0x10, 0x20, 0x30, 0x40, 0x90, 0, 0, 0]),
        0x1A809: bytes([0x00]),
        0x1AD09: bytes([0x00]),
        0x1ACD2: bytes([0x11]),
        0x1ACC7: bytes([0x41]),
        0x1ACF7: bytes([0x00]),
        0x1A57A: bytes([0x00]),
        0x1AD26: bytes([0x00]),
        0x1A57B: bytes([0x00]),
        0x1AD02: bytes([0xFF]),
        0x192D0: bytes([0x00]),
        0x1AB4F: bytes([0x01]),
        0x1ACAA: (0xA035).to_bytes(2, "big"),
        0x19D2D: bytes([0x41, 0x18, 0x00, 0x00]),
        0x19D55: bytes([0x41, 0x18, 0x00, 0x00]),
        0x19D31: bytes([0x41, 0x10, 0x00, 0x00]),
        0x1AB1C: bytes([0x00, 0x00, 0x00, 0x00]),
    })
    return pokes


def test_fun_17ca4():
    cases = [
        ("early", 0x40, 0x20, 0),   # $1acf7=1 -> andi $1ad02
        ("tabla", 0x40, 0x20, 0),   # 0x19d55
        ("modo38", 0x38, 0x20, 0),  # 0x19d2d + d0=0x2f
        ("modo33", 0x33, 0x20, 0),  # 0x19d2d + d0=0x42
        ("tipo10", 0x40, 0x10, 1),  # script 0x19cfd + 103ae
        ("tipo08", 0x40, 0x08, 1),  # script 0x19d00 + 103ae
        ("corta", 0x40, 0x20, 0),   # $1ad09=0x12 -> rts
    ]
    for name, o2, o3, acf7 in cases:
        pokes = _f17ca4_pokes(o2, o3)
        pokes[0x1ACF7] = bytes([acf7])
        if name == "corta":
            pokes[0x1AD09] = bytes([0x12])
        if name == "early":
            pokes[0x1ACF7] = bytes([0x01])
        diffs, info = differential(
            FUN_17CA4, {"a6": 0x52000, "_pokes": pokes},
            lambda lib, buf: lib.func_17ca4(buf, 0x52000))
        assert not diffs, f"{name}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], name


FUN_17A1A = 0x17A1A
harness._lib.func_17a1a.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_17a1a.restype = ctypes.c_uint32


def _f17a1a_pokes(obj1=0x00, obj2=0x40, obj3=0x30, obj4=0x00, entry=0,
                  acd7=0x10, es33=False):
    pokes = _f17x_common()
    pokes.update({
        0x1A772: (0x43000).to_bytes(4, "big"),
        0x43000: bytes([0x00, obj1, obj2, obj3, obj4, 0x00]),
        0x1A4E0: (0x12345678).to_bytes(4, "big"),
        0x1ACD7: bytes([acd7]),
        0x1A579: bytes([0x00]),
        0x1A858: (0x00F1).to_bytes(2, "big"),
        0x1A85A: (0x0001).to_bytes(2, "big"),
        0x1A57A: bytes([0x00]),
        0x1A57B: bytes([0x00]),
        0x1A570: bytes([0x01]),
        0x1AD02: bytes([0xFF]),
        0x1AD03: bytes([0x00]),
        0x1AD2C: bytes([0x00]),
        0x1ACF7: bytes([0x00]),
        0x1ACAA: (0x0000).to_bytes(2, "big"),
        0x1ABF8: (0x0000).to_bytes(2, "big"),
        0x1AB4F: bytes([0x01]),
        0x1A484: (0).to_bytes(2, "big"),
        0x1A69D: bytes([entry]),
        0x19968: bytes([0x00, 0x01, 0x00, 0x00]),
        0x2741E: bytes([0x00, 0x90]),
        0x1A252: bytes([3, 3]),
        0x1A254: (0x1111).to_bytes(2, "big"),
        0x1A256: (0x2222).to_bytes(2, "big"),
        0x1A258: (0x3333).to_bytes(2, "big"),
        0x1A25A: (0x4444).to_bytes(2, "big"),
        0x2751E: bytes([5, 0, 0, 0x00, 0x00]),
        0x31072: (0x10).to_bytes(2, "big"),
        0x31074: bytes([2, 2]),
        0x1A534: (0x28).to_bytes(4, "big"),
    })
    for i in range(0x100):
        pokes[0x31076 + i] = bytes([(i * 7 + 3) & 0xFF])
    for i in range(0x1400):
        pokes[0x3B1AE + i] = bytes([(i * 5 + 1) & 0xFF])
    _hint_tbl(pokes)
    return pokes


def test_fun_17a1a():
    cases = [
        ("uno", dict(obj2=0x40, obj3=0x30, entry=0)),        # d1=1 -> d2=1
        ("dos", dict(obj2=0x40, obj3=0x10, entry=2, acd7=0xCE)),   # d1=2, 16210 >=0x80
        ("tres_alto", dict(obj2=0x40, obj3=0x10, entry=3, acd7=0x90)),  # d1=3, >=0x80
        ("tres_bajo", dict(obj2=0x40, obj3=0x10, entry=3, acd7=0x10)),  # d1=3, <0x80
        ("cinco", dict(obj2=0x40, obj3=0x10, entry=5)),      # d1=5 sin umbrales
        ("cero", dict(obj2=0x40, obj3=0x10, entry=0)),       # d1=0 -> d2=1
        ("cuatro", dict(obj2=0x40, obj3=0x10, entry=4, acd7=0x64)),  # d1=4, 0x51/0x52
        ("clase2c", dict(obj2=0x2C, obj3=0x10, entry=0)),    # salta a 0x17aae
        ("clase38", dict(obj2=0x38, obj3=0x10, entry=0)),    # conserva d2/d3
        ("sind1", dict(obj2=0x40, obj3=0x10, entry=0)),      # $1ad03 != 0
    ]
    for name, kw in cases:
        pokes = _f17a1a_pokes(**kw)
        if name == "sind1":
            pokes[0x1AD03] = bytes([0x01])
        diffs, info = differential(
            FUN_17A1A, {"a6": 0x52000, "_pokes": pokes},
            lambda lib, buf: lib.func_17a1a(buf, 0x52000),
            schedule=_vbl_k(11), wait_sites=[VBL_SITE], max_insns=2_000_000)
        assert not diffs, f"{name}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], name


FUN_18A98 = 0x18A98
harness._lib.func_18a98.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_18a98.restype = ctypes.c_uint32


def _f18a98_pokes(first):
    pokes = {
        0x1A45E: (0x80000).to_bytes(4, "big"),
        0x1A462: (0x78000).to_bytes(4, "big"),
        0x1A466: (0x40000).to_bytes(4, "big"),
        0x1A571: bytes([0x00 if first else 0x01]),
        0x1A579: bytes([0x00]),
        0x1AB4F: bytes([0x01]),
        0x19714: bytes([0x00, 0x01, 0x10, 0x20]),
        0x1A252: bytes([3, 3]),
        0x1A254: (0x1111).to_bytes(2, "big"),
        0x1A256: (0x2222).to_bytes(2, "big"),
        0x1A258: (0x3333).to_bytes(2, "big"),
        0x1A25A: (0x4444).to_bytes(2, "big"),
        0x2751E: bytes([5, 0, 0, 0x00, 0x00]),
        0x31072: (0x10).to_bytes(2, "big"),
        0x31074: bytes([2, 2]),
    }
    pokes[0x1971C] = (0x20).to_bytes(2, "big")      # 1a entrada: solo [w][h]
    pokes[0x1971E] = (0x20).to_bytes(2, "big")
    for i in range(52):                             # resto: [d0][d1][w][h]
        base = 0x19720 + 8 * i
        pokes[base] = (0x10 + (i % 8) * 8).to_bytes(2, "big")
        pokes[base + 2] = (0).to_bytes(2, "big")
        pokes[base + 4] = (0x20).to_bytes(2, "big")
        pokes[base + 6] = (0x20).to_bytes(2, "big")
    for i in range(0x100):
        pokes[0x31076 + i] = bytes([(i * 7 + 3) & 0xFF])
    for i in range(0x1400):
        pokes[0x3B1AE + i] = bytes([(i * 5 + 1) & 0xFF])
    for i in range(0x10000):
        pokes[0x80000 + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x78000 + i] = bytes([(i * 11 + 7) & 0xFF])
    return pokes


def test_fun_18a98():
    pokes = _f18a98_pokes(first=False)
    diffs, info = differential(
        FUN_18A98, {"a6": 0x30000, "_pokes": pokes},
        lambda lib, buf: lib.func_18a98(buf, 0x30000),
        schedule=_vbl_k(53), wait_sites=[VBL_SITE], max_insns=20_000_000,
        stop_pc=0x18C80)
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


def test_fun_18a98_first():
    pokes = _f18a98_pokes(first=True)
    sched = [{"pc": VBL_SITE, "nth": k, "addr": VBL_COUNTER, "width": 1,
              "value": (k % 255) + 1} for k in range(1, 270)]
    diffs, info = differential(
        FUN_18A98, {"a6": 0x30000, "_pokes": pokes},
        lambda lib, buf: lib.func_18a98(buf, 0x30000),
        schedule=sched, wait_sites=[VBL_SITE], max_insns=40_000_000,
        stop_pc=0x18C80)
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


FUN_12CE4 = 0x12CE4
harness._lib.view_mode_cycle.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.view_mode_cycle.restype = ctypes.c_uint32


def _mode_table(entry, header, packed=b"", tail=b""):
    pokes = {}
    a, body = 0x1AD38, bytes(header) + packed + tail
    for _ in range(entry - 1):
        pokes[a] = bytes([1])
        a += 1
    pokes[a] = bytes([len(body) + 1]) + body
    return pokes


def _f12ce4_pokes(acc6=0x5F):
    pokes = {
        0x23E9C: bytes([4, 0, 0, 0]),
        0x23EA0: bytes([4, 0, 0, 0]),
        0x41FB0: (0x10).to_bytes(2, "big"),
        0x41FB2: bytes([2, 2]),
    }
    for i in range(0x200):
        pokes[0x41FB4 + i] = bytes([(i * 7 + 3) & 0xFF])
    for i in range(0x2000):
        pokes[0x3B1AC + i] = bytes([(i * 5 + 1) & 0xFF])
    pokes.update(_mode_table(acc6, [0x30, 0x31, 0x32, 0x33, 0], b"", bytes(range(16))))
    pokes[0xD8EB] = bytes([0x52])
    pokes[0x1ACCA] = bytes([0x02])              # +1 -> caso 3
    pokes[0x1ACC6] = bytes([acc6])
    pokes[0x1A570] = bytes([0x01])
    pokes[0x1A45E] = (0x80000).to_bytes(4, "big")
    pokes[0x1A462] = (0x78000).to_bytes(4, "big")
    pokes[0x1A534] = (0x28).to_bytes(4, "big")
    pokes[0x1ACD4] = bytes([0x00])
    for i in range(0x10000):
        pokes[0x80000 + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x78000 + i] = bytes([(i * 11 + 7) & 0xFF])
    return pokes


def test_fun_12ce4_caso3():
    pokes = _f12ce4_pokes()
    sched = [{"pc": VBL_SITE, "nth": k, "addr": VBL_COUNTER, "width": 1,
              "value": (k % 255) + 1} for k in range(1, 73)]
    diffs, info = differential(
        FUN_12CE4, {"a6": 0x52000, "_pokes": pokes},
        lambda lib, buf: lib.view_mode_cycle(buf, 0x52000),
        schedule=sched, wait_sites=[VBL_SITE], max_insns=20_000_000,
        stop_pc=0x12D94)
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


def test_fun_12ce4_caso0():
    pokes = _f12ce4_pokes(acc6=0x18)
    pokes[0x1ACCA] = bytes([0x03])              # +1 -> caso 0
    for base in (0x1A82E, 0x1A83E):
        pokes[base] = (0x0010).to_bytes(4, "big")
        pokes[base + 4] = (0x0014).to_bytes(4, "big")
        pokes[base + 8] = (0x81420).to_bytes(4, "big")
        pokes[base + 12] = (0x79420).to_bytes(4, "big")
    sched = [{"pc": VBL_SITE, "nth": k, "addr": VBL_COUNTER, "width": 1,
              "value": (k % 255) + 1} for k in range(1, 89)]
    diffs, info = differential(
        FUN_12CE4, {"a6": 0x52000, "_pokes": pokes},
        lambda lib, buf: lib.view_mode_cycle(buf, 0x52000),
        schedule=sched, wait_sites=[VBL_SITE], max_insns=20_000_000,
        stop_pc=0x12CE2)
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


def test_fun_12ce4_caso1():
    pokes = _f12ce4_pokes(acc6=0x18)
    pokes[0x1ACCA] = bytes([0x00])              # +1 -> caso 1
    sched = [{"pc": VBL_SITE, "nth": k, "addr": VBL_COUNTER, "width": 1,
              "value": (k % 255) + 1} for k in range(1, 83)]
    diffs, info = differential(
        FUN_12CE4, {"a6": 0x52000, "_pokes": pokes},
        lambda lib, buf: lib.view_mode_cycle(buf, 0x52000),
        schedule=sched, wait_sites=[VBL_SITE], max_insns=20_000_000,
        stop_pc=0x12C06)
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


def test_fun_12ce4_caso2():
    pokes = _f12ce4_pokes(acc6=0x09)
    pokes.update(_mode_table(0x5F, [0x30, 0x31, 0x32, 0x33, 0], b"", bytes(range(16))))
    pokes[0x1ACCA] = bytes([0x01])              # +1 -> caso 2
    pokes[0x1D8EB if False else 0xD8EB] = bytes([0x52])
    for base in (0x1A82E, 0x1A83E):
        pokes[base] = (0x0010).to_bytes(4, "big")
        pokes[base + 4] = (0x0014).to_bytes(4, "big")
        pokes[base + 8] = (0x81420).to_bytes(4, "big")
        pokes[base + 12] = (0x79420).to_bytes(4, "big")
    sched = [{"pc": VBL_SITE, "nth": k, "addr": VBL_COUNTER, "width": 1,
              "value": (k % 255) + 1} for k in range(1, 73)]
    diffs, info = differential(
        FUN_12CE4, {"a6": 0x52000, "_pokes": pokes},
        lambda lib, buf: lib.view_mode_cycle(buf, 0x52000),
        schedule=sched, wait_sites=[VBL_SITE], max_insns=20_000_000,
        stop_pc=0x12C6C)
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


FUN_14AC6 = 0x14AC6
FUN_14BFA = 0x14BFA
harness._lib.func_14ac6.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_14ac6.restype = ctypes.c_uint32
harness._lib.func_14bfa.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_14bfa.restype = ctypes.c_uint32


def _f14_fixture(match_idx=0, sel4=0, a2b0=0x10, a2b3=0x00):
    pokes = {
        0x1A45E: (0x80000).to_bytes(4, "big"),
        0x1A488: (0x00FF).to_bytes(2, "big"),
        0x1A510: (0).to_bytes(2, "big"),
        0x1A50E: (0).to_bytes(2, "big"),
        0x1A75E: (0x1A6AE).to_bytes(4, "big"),
        0x1AB46: bytes([0x00]),
        0x1AB47: bytes([0x00]),
        0x1AB4F: bytes([0x00]),
        0x1ACE9: bytes([0x10]),
        0x1ACEC: bytes([0x10]),
        0x1ACD2: bytes([0x05]),
        0x1ACD4: bytes([0x00]),
        0x1ACD7: bytes([0x10]),
        0x1AD04: bytes([0x00]),
        0x1AD29: bytes([0x00]),
        0x1A44F: bytes([0xFF]),
        0x1A456: bytes([0x00]),
        0x1A474: (0).to_bytes(2, "big"),
        0x1A472: (0).to_bytes(2, "big"),
        0x1A6AE: bytes([a2b0, 0x00, 0x08, a2b3, 0x00, 0x00, 0x00, 0x00, 0x00]),
        0x1A812: (0x50000).to_bytes(4, "big"),
        0x50000: bytes([0, 0, 0, 0, sel4 & 0xFF, 0, 0, 0]),
    }
    for i in range(0x3F):
        base = 0x1A8F0 + i * 6
        pokes[base] = (0x8001 if i == match_idx else 0x0001).to_bytes(2, "big")
        if match_idx is None:
            pokes[base] = (0x0001).to_bytes(2, "big")
        pokes[base + 2] = bytes([0x20 + i])
        pokes[base + 3] = bytes([0x30 + i])
        pokes[base + 4] = (0x100 + i).to_bytes(2, "big")
    for i in range(0x70):
        pokes[0x41586 + i * 3] = bytes([3, 0x21 + (i % 0x40), 0x22])
        pokes[0x41B76 + i * 3] = bytes([3, 0x21 + (i % 0x40), 0x22])
    for i in range(0x200):                # plantillas 0x1d402: [len=2][script=0]
        pokes[0x1D402 + 2 * i] = bytes([2])   # -> 0x10630 sale de inmediato
        pokes[0x1D402 + 2 * i + 1] = bytes([0])
    for i in range(0x400):                # banco de texto 0x3ec8a: entradas len=1
        pokes[0x3EC8A + i] = bytes([1])
    return pokes


def _f14_sched():
    return [
        {"pc": VBL_SITE, "nth": 1, "addr": VBL_COUNTER, "width": 1, "value": 1},
        {"pc": VBL_SITE, "nth": 2, "addr": VBL_COUNTER, "width": 1, "value": 2},
        {"pc": 0x136B0, "nth": 1, "addr": 0x1A44F, "width": 1, "value": 0x00},
        {"pc": 0x136B0, "nth": 2, "addr": 0x1A44F, "width": 1, "value": 0x01},
    ]


def test_fun_14ac6():
    for name, kw in (("d0_cero", dict()),
                     ("a2_81", dict(sel4=1, a2b0=0x81)),
                     ("coincide", dict(sel4=1, a2b3=0x30))):
        pokes = _f14_fixture(**kw)
        diffs, info = differential(
            FUN_14AC6, {"a6": 0x52000, "_pokes": pokes},
            lambda lib, buf: lib.func_14ac6(buf, 0x52000),
            schedule=_f14_sched(), wait_sites=[VBL_SITE, 0x136B0],
            max_insns=5_000_000)
        assert not diffs, f"{name}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], name


def test_fun_14ac6_sin_muestras():
    pokes = _f14_fixture(match_idx=None)
    diffs, info = differential(
        FUN_14AC6, {"a6": 0x52000, "_pokes": pokes},
        lambda lib, buf: lib.func_14ac6(buf, 0x52000),
        max_insns=5_000_000)
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


def test_fun_14bfa():
    for name, kw in (("menu", {}),
                     ("sin_muestras", dict(match_idx=None))):
        pokes = _f14_fixture(**kw)
        pokes.update(_tb_pokes(0x99, keys=1))
        n = 0 if kw else 1
        diffs, info = differential(
            FUN_14BFA, {"a6": 0x52000, "_pokes": pokes},
            lambda lib, buf: lib.func_14bfa(buf, 0x52000),
            schedule=_f14_sched() if n else None,
            wait_sites=[VBL_SITE, 0x136B0] if n else None,
            max_insns=5_000_000)
        assert not diffs, f"{name}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], name
