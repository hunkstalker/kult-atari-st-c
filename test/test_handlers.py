"""Tests de handlers de comando pequenos (handlers.c).

FUN_0001228a @ 0x1228a: prompt_wait + A6 += 1.
FUN_000121fa @ 0x121fa: selecciona/blitea icono y un trazo 2x15.
FUN_00012436 @ 0x12436: copia mascaras y cae en 0x12466.
FUN_000127fa @ 0x127fa: save enmascarado a 0xd8f0.
FUN_000127e0 @ 0x127e0: A0 -= 0x8000 + 0x127fa + 0x124da.
"""
import ctypes

import harness
from harness import differential, report, console_key

_u8p = ctypes.POINTER(ctypes.c_uint8)
SCREEN = 0x90000
BACK = 0x88000
VBL_COUNTER = 0x1acd4
VBL_SITE = 0x1359a


def _hint_table():
    pokes = {0x1a851: bytes([0x11]), 0x1ad0a: bytes([0x00]),
             0x1a45e: SCREEN.to_bytes(4, "big")}
    for i in range(10):
        base = 0x1a850 + i * 0x10
        pokes[base] = ((i + 1) & 0xffff).to_bytes(2, "big")
        pokes[base + 2] = (SCREEN + i * 0x10).to_bytes(4, "big")
        for k, v in enumerate((1, 2, 3, 4, 5)):
            pokes[base + 6 + k * 2] = v.to_bytes(2, "big")
    return pokes


FUN_1228A = 0x1228A
harness._lib.prompt_wait_entry.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.prompt_wait_entry.restype = ctypes.c_uint32


def test_fun_1228a():
    """Modo 0x87: prompt_wait sale por Cnecin con una tecla staged; A6 avanza 1."""
    pokes = {
        0x1a45e: SCREEN.to_bytes(4, "big"),
        0x1a46a: (0x8000).to_bytes(4, "big"),
        0x1acc6: bytes([0x87]),
        0x1a44f: bytes([0x00]),
        0x1a456: bytes([0x00]),
        VBL_COUNTER: bytes([0x00]),
    }
    pokes.update(console_key("a"))
    diffs, info = differential(FUN_1228A, {"a6": 0x30000, "_pokes": pokes},
                               lambda lib, buf: lib.prompt_wait_entry(buf, 0x30000))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


FUN_121FA = 0x121FA
harness._lib.icon_blit_handler.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.icon_blit_handler.restype = ctypes.c_uint32


def test_fun_121fa():
    pokes = _hint_table()
    pokes[0x30000] = bytes([0, 3])       # a6[1] = 3 -> entrada 2
    pokes[0x1a484] = (0x1234).to_bytes(2, "big")
    for i in range(0x8000):
        pokes[SCREEN + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[BACK + i] = bytes([(i * 11 + 7) & 0xFF])
    diffs, info = differential(FUN_121FA, {"a6": 0x30000, "_pokes": pokes},
                               lambda lib, buf: lib.icon_blit_handler(buf, 0x30000))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


FUN_12436 = 0x12436
harness._lib.mask_rows_handler.argtypes = [_u8p]
harness._lib.mask_rows_handler.restype = None


def test_fun_12436():
    pokes = {
        0x1a49e: (0xf000).to_bytes(2, "big"),
        0x1a4a0: (0x000f).to_bytes(2, "big"),
        0x1a4a2: (0x0002).to_bytes(2, "big"),
        0x1a4a4: (3).to_bytes(2, "big"),
        0x1a4b2: SCREEN.to_bytes(4, "big"),
        VBL_COUNTER: bytes([0x00]),
    }
    for i in range(0x400):
        pokes[0xd8f0 + i] = bytes([(i * 5 + 1) & 0xFF])
        pokes[SCREEN + i] = bytes([(i * 13 + 5) & 0xFF])
    diffs, _ = differential(
        FUN_12436, {"_pokes": pokes}, lambda lib, buf: lib.mask_rows_handler(buf),
        schedule=[{"pc": VBL_SITE, "nth": 1, "addr": VBL_COUNTER, "width": 1, "value": 1}],
        wait_sites=[VBL_SITE])
    assert not diffs, report(diffs)


FUN_127FA = 0x127FA
FUN_127E0 = 0x127E0
harness._lib.masked_save_row.argtypes = [_u8p] + [ctypes.c_uint32] * 4
harness._lib.masked_save_row.restype = None
harness._lib.masked_save.argtypes = [_u8p] + [ctypes.c_uint32] * 5
harness._lib.masked_save.restype = None


def _save_pokes():
    pokes = {VBL_COUNTER: bytes([0x00])}
    for i in range(0x2000):
        pokes[0xd8f0 + i] = bytes([(i * 5 + 1) & 0xFF])
        pokes[SCREEN + i] = bytes([(i * 13 + 5) & 0xFF])
    return pokes


def test_fun_127fa():
    for d3 in (1, 2, 4):
        for d7 in (0, 4):
            pokes = _save_pokes()
            diffs, _ = differential(
                FUN_127FA, {"a0": SCREEN, "d2": 0x10, "d3": d3, "d7": d7, "_pokes": pokes},
                lambda lib, buf, h=d3, p=d7: lib.masked_save_row(buf, SCREEN, 0x10, h, p))
            assert not diffs, f"d3={d3} d7={d7}\n{report(diffs)}"


def test_fun_127e0():
    pokes = _save_pokes()
    diffs, _ = differential(
        FUN_127E0, {"a0": SCREEN, "a1": 0x40000, "d2": 0x10, "d3": 3, "d7": 0,
                    "_pokes": pokes},
        lambda lib, buf: lib.masked_save(buf, SCREEN, 0x40000, 0x10, 3, 0),
        schedule=[{"pc": VBL_SITE, "nth": 1, "addr": VBL_COUNTER, "width": 1, "value": 1}],
        wait_sites=[VBL_SITE])
    assert not diffs, report(diffs)


FUN_134E4 = 0x134E4
harness._lib.copy_grid.argtypes = [_u8p] + [ctypes.c_uint32] * 5
harness._lib.copy_grid.restype = None


def test_fun_134e4():
    for d2, d3 in ((8, 1), (8, 3), (16, 2)):
        pokes = {0x1a45e: SCREEN.to_bytes(4, "big")}
        for i in range(0x4000):
            pokes[SCREEN + i] = bytes([(i * 3 + 1) & 0xFF])
            pokes[0x40000 + i] = bytes([(i * 7 + 3) & 0xFF])
        diffs, _ = differential(
            FUN_134E4, {"d0": 4, "d1": 4, "d2": d2, "d3": d3, "a1": 0x40000,
                        "_pokes": pokes},
            lambda lib, buf, w=d2, h=d3: lib.copy_grid(buf, 4, 4, w, h, 0x40000))
        assert not diffs, f"d2={d2} d3={d3}\n{report(diffs)}"


FUN_13370 = 0x13370
harness._lib.reveal_diamond.argtypes = [_u8p] + [ctypes.c_uint32] * 4
harness._lib.reveal_diamond.restype = None


def test_fun_13370():
    for d0, d1, d2, d3 in ((4, 4, 4, 4), (8, 8, 8, 6), (10, 6, 6, 10)):
        pokes = {0x1a45e: SCREEN.to_bytes(4, "big"),
                 0x1a462: BACK.to_bytes(4, "big")}
        for i in range(0x10000):
            pokes[SCREEN + i] = bytes([(i * 3 + 1) & 0xFF])
            pokes[BACK + i] = bytes([(i * 11 + 7) & 0xFF])
        diffs, _ = differential(
            FUN_13370, {"d0": d0, "d1": d1, "d2": d2, "d3": d3, "_pokes": pokes},
            lambda lib, buf, a=d0, b=d1, c=d2, e=d3:
            lib.reveal_diamond(buf, a, b, c, e))
        assert not diffs, f"d0={d0} d1={d1} d2={d2} d3={d3}\n{report(diffs)}"


FUN_13474 = 0x13474
harness._lib.icon_reveal.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.icon_reveal.restype = ctypes.c_uint32


def test_fun_13474():
    for skip in (0, 1):
        pokes = _hint_table()
        pokes[0x30000] = bytes([0, 1])
        pokes[0x1a570] = bytes([skip])
        pokes[0x1a462] = BACK.to_bytes(4, "big")
        for i in range(0x10000):
            pokes[SCREEN + i] = bytes([(i * 3 + 1) & 0xFF])
            pokes[BACK + i] = bytes([(i * 11 + 7) & 0xFF])
        diffs, info = differential(FUN_13474, {"a6": 0x30000, "_pokes": pokes},
                                   lambda lib, buf: lib.icon_reveal(buf, 0x30000))
        assert not diffs, f"skip={skip}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], f"skip={skip} a6"


FUN_12240 = 0x12240
FUN_1224E = 0x1224E
harness._lib.wait_button_a.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.wait_button_a.restype = ctypes.c_uint32
harness._lib.wait_button_b.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.wait_button_b.restype = ctypes.c_uint32
WF_SITE = 0x136b0
PKT = 0x1a44f
BUTTONS = 0x1a456


def _wait_pokes():
    return {PKT: bytes([0x00]), BUTTONS: bytes([0x00]), 0x1a84e: bytes([0x00]),
            0x1a570: bytes([0x00])}


def _sched(values):
    return [{"pc": WF_SITE, "nth": k, "addr": PKT, "width": 1, "value": v}
            for k, v in enumerate(values, 1)]


def test_fun_12240():
    a6 = ctypes.c_uint32(0)
    # salida inmediata: bit 15 puesto; y N == 0
    for base, byte in ((0x8000, 0xFF), (0x0000, 0x00)):
        pokes = _wait_pokes()
        pokes[0x1a84e] = base.to_bytes(2, "big")
        pokes[0x50000] = bytes([0x00, byte])
        diffs, info = differential(FUN_12240, {"a6": 0x50000, "_pokes": pokes},
                                   lambda lib, buf: lib.wait_button_a(buf, 0x50000))
        assert not diffs, f"base={base:#x} byte={byte:#x}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"]
    # boton en la espera k: N=1 da 0x33 (51) esperas; N=2, 0x67 (103)
    for n, k in ((1, 1), (1, 3), (1, 51), (2, 52), (2, 102)):
        pokes = _wait_pokes()
        pokes[0x50000] = bytes([0x00, n & 0xFF])
        vals = [0x00] * (k - 1) + [0x01]
        diffs, info = differential(
            FUN_12240, {"a6": 0x50000, "_pokes": pokes},
            lambda lib, buf: lib.wait_button_a(buf, 0x50000),
            schedule=_sched(vals), wait_sites=[WF_SITE], max_insns=3_000_000)
        assert not diffs, f"n={n} k={k}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], f"n={n} k={k}"


def test_fun_1224e():
    for base in (0x8000, 0xFFFC):                   # bit15 puesto / N == 0 (d5=4)
        pokes = _wait_pokes()
        pokes[0x1a84e] = base.to_bytes(2, "big")
        diffs, info = differential(FUN_1224E, {"a6": 0x50000, "_pokes": pokes},
                                   lambda lib, buf: lib.wait_button_b(buf, 0x50000))
        assert not diffs, f"base={base:#x}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], f"base={base:#x} a6"
    for base, k in ((0xFFFD, 1), (0xFFFD, 3), (0xFFFD, 51), (0x0000, 204)):
        pokes = _wait_pokes()
        pokes[0x1a84e] = base.to_bytes(2, "big")       # d5 = 4 + base
        vals = [0x00] * (k - 1) + [0x01]
        diffs, info = differential(
            FUN_1224E, {"a6": 0x50000, "_pokes": pokes},
            lambda lib, buf: lib.wait_button_b(buf, 0x50000),
            schedule=_sched(vals), wait_sites=[WF_SITE], max_insns=3_000_000)
        assert not diffs, f"k={k}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], f"k={k} a6"


FUN_11F02 = 0x11F02
FUN_11F2E = 0x11F2E
harness._lib.blit_dispatch_a.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.blit_dispatch_a.restype = ctypes.c_uint32
harness._lib.blit_dispatch_b.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.blit_dispatch_b.restype = ctypes.c_uint32


def _cmd_elem_pokes(w=4, h=3):
    pokes = {
        0x2751e: bytes([5, 0, 0, 0x00, 0x00]),
        0x1a252: bytes([w]), 0x1a253: bytes([h]),
        0x1a254: (0x1111).to_bytes(2, "big"),
        0x1a256: (0x2222).to_bytes(2, "big"),
        0x1a258: (0x3333).to_bytes(2, "big"),
        0x1a25a: (0x4444).to_bytes(2, "big"),
        0x31072: (0x10).to_bytes(2, "big"), 0x31074: bytes([2, 2]),
        0x1a45e: SCREEN.to_bytes(4, "big"),
        0x1a851: bytes([0x22]), 0x1ad0a: bytes([0]),
        VBL_COUNTER: bytes([0x00]),
    }
    for i in range(0x100):
        pokes[0x31076 + i] = bytes([(i * 7 + 3) & 0xFF])
    for i in range(0x2000):
        pokes[0x3b1ae + i] = bytes([(i * 5 + 1) & 0xFF])
        pokes[0xD8F0 + i] = bytes([(i * 11 + 5) & 0xFF])
    for i in range(0x10000):
        pokes[SCREEN + i] = bytes([(i * 3 + 1) & 0xFF])
    for i in range(10 * 0x10):
        pokes[0x1a850 + i] = bytes([0x00])
    return pokes


def _vbl_k(n):
    return [{"pc": VBL_SITE, "nth": k, "addr": VBL_COUNTER, "width": 1, "value": k}
            for k in range(1, n + 1)]


def _run_cmd(entry, fn):
    # ancho 16 -> 0x12698 da groups>=2; con groups==1 el original gira (degenerado).
    for skip, waits in ((0, 3), (1, 1)):
        pokes = _cmd_elem_pokes(16, 3)
        pokes[0x50000] = bytes([0x00, 1, 8, 4])
        pokes[0x1a570] = bytes([skip])
        diffs, info = differential(
            entry, {"a6": 0x50000, "_pokes": pokes},
            lambda lib, buf, f=fn: f(buf, 0x50000),
            schedule=_vbl_k(waits), wait_sites=[VBL_SITE])
        assert not diffs, f"{entry:#x} skip={skip}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], f"{entry:#x} skip={skip} a6"


def test_fun_11f02():
    _run_cmd(FUN_11F02, harness._lib.blit_dispatch_a)


def test_fun_11f2e():
    _run_cmd(FUN_11F2E, harness._lib.blit_dispatch_b)


def _abort_pokes(kind):
    """Script [00 ff] con las dos salidas de 0x128a4; A_ELEM_* ya validos."""
    pokes = _cmd_elem_pokes(16, 3)
    pokes[0x50000] = bytes([0x00, 0xFF])
    pokes[0x1ad0a] = bytes([1 if kind == "flag" else 0])
    if kind == "clase":
        pokes[0x1a81e] = (0x60000).to_bytes(4, "big")
        pokes[0x60000] = (0).to_bytes(4, "big")
        pokes[0x1a772] = (0x40000).to_bytes(4, "big")
        pokes[0x40003] = bytes([0x37])
        for i in range(24):
            base = 0x19e19 + i * 6
            pokes[base] = bytes([0x80])
            for k in range(1, 6):
                pokes[base + k] = bytes([(i + k) & 0xFF])
    return pokes


def _run_abort(entry, fn):
    for kind in ("flag", "clase"):
        pokes = _abort_pokes(kind)
        diffs, info = differential(
            entry, {"a6": 0x50000, "_pokes": pokes},
            lambda lib, buf, f=fn: f(buf, 0x50000),
            max_insns=20_000_000)
        assert not diffs, f"{entry:#x} {kind}\n{report(diffs)}"
        assert info["ret"] == 0x50002, f"{entry:#x} {kind}: {info['ret']:#x}"
        assert info["regs"]["a6"] == 0x50002, f"{entry:#x} {kind} a6"


def test_fun_11f02_abort():
    _run_abort(FUN_11F02, harness._lib.blit_dispatch_a)


def test_fun_11f2e_abort():
    _run_abort(FUN_11F2E, harness._lib.blit_dispatch_b)


def test_fun_11f64_abort():
    _run_abort(0x11F64, harness._lib.overlay_dispatch_1)


def test_fun_11f98_abort():
    _run_abort(0x11F98, harness._lib.overlay_dispatch_2)


def test_fun_132aa_abort():
    _run_abort(FUN_132AA, harness._lib.reveal_box_build)


def test_fun_132ec_abort():
    _run_abort(FUN_132EC, harness._lib.reveal_box_wait_a)


def test_fun_1332e_abort():
    _run_abort(FUN_1332E, harness._lib.reveal_box_wait_b)


def test_fun_134da_abort():
    _run_abort(FUN_134DA, harness._lib.elem_blit_vbl)


FUN_132AA = 0x132AA
harness._lib.reveal_box_build.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.reveal_box_build.restype = ctypes.c_uint32


def test_fun_132aa():
    # 132aa: 127e0 (124da = 1 espera) + 12466 (1 espera); 13370 no espera.
    for skip, waits in ((0, 2), (1, 1)):
        pokes = _cmd_elem_pokes(16, 3)
        pokes[0x50000] = bytes([0x00, 1, 8, 4])
        pokes[0x1a570] = bytes([skip])
        diffs, info = differential(
            FUN_132AA, {"a6": 0x50000, "_pokes": pokes},
            lambda lib, buf: lib.reveal_box_build(buf, 0x50000),
            schedule=_vbl_k(waits), wait_sites=[VBL_SITE])
        assert not diffs, f"skip={skip}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], f"skip={skip} a6"


FUN_132EC = 0x132EC
FUN_1332E = 0x1332E
harness._lib.reveal_box_wait_a.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.reveal_box_wait_a.restype = ctypes.c_uint32
harness._lib.reveal_box_wait_b.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.reveal_box_wait_b.restype = ctypes.c_uint32


def test_fun_132ec():
    # 13e14: (16<<2) + 0x1a52c = 64+3 VBLs; + 127e0 (1) + 12466 (1).
    for skip, waits in ((0, 69), (1, 1)):
        pokes = _cmd_elem_pokes(16, 3)
        pokes[0x50000] = bytes([0x00, 1, 8, 4])
        pokes[0x1a570] = bytes([skip])
        diffs, info = differential(
            FUN_132EC, {"a6": 0x50000, "_pokes": pokes},
            lambda lib, buf: lib.reveal_box_wait_a(buf, 0x50000),
            schedule=_vbl_k(waits), wait_sites=[VBL_SITE])
        assert not diffs, f"skip={skip}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], f"skip={skip} a6"


def test_fun_1332e():
    # 13eae: 0x1a52c + (16<<2) + 0x1a52c+1 = 3+64+4 VBLs; + 127e0 (1) + 12466 (1).
    for skip, waits in ((0, 73), (1, 1)):
        pokes = _cmd_elem_pokes(16, 3)
        pokes[0x50000] = bytes([0x00, 1, 8, 4])
        pokes[0x1a570] = bytes([skip])
        diffs, info = differential(
            FUN_1332E, {"a6": 0x50000, "_pokes": pokes},
            lambda lib, buf: lib.reveal_box_wait_b(buf, 0x50000),
            schedule=_vbl_k(waits), wait_sites=[VBL_SITE])
        assert not diffs, f"skip={skip}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], f"skip={skip} a6"


# ---- bucle principal: piezas inmediatas ----

FUN_RUN_COMMAND = 0x103A8
FUN_1031C = 0x1031C
harness._lib.run_command.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.run_command.restype = None
harness._lib.handle_frame_input.argtypes = [_u8p]
harness._lib.handle_frame_input.restype = None


def test_run_command():
    pokes = {0x1a50e: (0).to_bytes(2, "big")}
    diffs, _ = differential(FUN_RUN_COMMAND, {"_pokes": pokes},
                            lambda lib, buf: lib.run_command(
                                buf, harness.emu.STACK_TOP))
    assert not diffs, report(diffs)


def _frame_pokes():
    pokes = {
        0x1a450: (100).to_bytes(2, "big"),
        0x1a452: (50).to_bytes(2, "big"),
        0x19700: (0).to_bytes(2, "big"),
        0x19702: (0).to_bytes(2, "big"),
        0x1a48a: bytes([0]),
        0x199ca: (0x30000).to_bytes(4, "big"),
        0x1a45e: (0x78000).to_bytes(4, "big"),
        0x19ace: (0x78000).to_bytes(4, "big"),
        0x1ab4f: bytes([0]),
        0x1ab45: bytes([0x10]), 0x1ab44: bytes([0x10]),
        0x1ab46: bytes([0x20]), 0x1ab47: bytes([0x20]),
        0x1acd4: bytes([0x00]),
        0x30000: bytes((i * 7 + 3) & 0xFF for i in range(64)),
    }
    for i in range(0x2000):
        pokes[0x78000 + i] = bytes([(i * 5 + 1) & 0xFF])
    for i in range(0x100):
        pokes[0x199ce + i] = bytes([(i * 7 + 1) & 0xFF])
    return pokes


def test_fun_1031c():
    """0x1ab4f=0 -> una espera; selectores iguales -> sin 0x10a54/0x10acc."""
    pokes = _frame_pokes()
    diffs, _ = differential(
        FUN_1031C, {"_pokes": pokes}, lambda lib, buf: lib.handle_frame_input(buf),
        schedule=[{"pc": VBL_SITE, "nth": 1, "addr": VBL_COUNTER, "width": 1, "value": 1}],
        wait_sites=[VBL_SITE])
    assert not diffs, report(diffs)


FUN_147CE = 0x147CE
harness._lib.cursor_select_b.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.cursor_select_b.restype = ctypes.c_uint32


def test_fun_147ce():
    for cx in (0x28, 0x04, 0x30, 0x40):
        pokes = {
            0x1a450: cx.to_bytes(2, "big"),
            0x1a452: (0x10).to_bytes(2, "big"),
            0x1a50e: (0x1234).to_bytes(2, "big"),
            0x1ab46: bytes([0x00]),
            0x1a48a: bytes([0x00]),
        }
        for i in range(7):
            base = 0x19b06 + i * 4
            pokes[base] = bytes([i * 4])
            pokes[base + 1] = bytes([i * 4 + 4])
            pokes[base + 2] = bytes([0x00])
            pokes[base + 3] = bytes([0xFF])
        diffs, _ = differential(FUN_147CE, {"a4": 0x19B06, "_pokes": pokes},
                                lambda lib, buf: lib.cursor_select_b(buf, 0x19B06))
        assert not diffs, f"cx={cx:#x}\n{report(diffs)}"


FUN_134DA = 0x134DA
harness._lib.elem_blit_vbl.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.elem_blit_vbl.restype = ctypes.c_uint32


def test_fun_134da():
    pokes = _cmd_elem_pokes(16, 3)
    pokes[0x50000] = bytes([0x00, 1, 8, 4])
    pokes[0x1a570] = bytes([0x01])
    diffs, info = differential(
        FUN_134DA, {"a6": 0x50000, "_pokes": pokes},
        lambda lib, buf: lib.elem_blit_vbl(buf, 0x50000),
        schedule=_vbl_k(1), wait_sites=[VBL_SITE])
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


# ---- dispatchers 0x11f64/0x11f98/0x11fd8/0x1200a/0x12044/0x12076 ----

for _name, _entry in (("overlay_dispatch_1", 0x11F64), ("overlay_dispatch_2", 0x11F98),
                      ("overlay_dispatch_3", 0x11FD8), ("overlay_dispatch_4", 0x1200A),
                      ("overlay_dispatch_5", 0x12044), ("overlay_dispatch_6", 0x12076)):
    _fn = getattr(harness._lib, _name)
    _fn.argtypes = [_u8p, ctypes.c_uint32]
    _fn.restype = ctypes.c_uint32


def _icon_pokes(w, h, x=8, y=4, d2=None, d7=0):
    p = _cmd_elem_pokes(w, h)
    p[0x50000] = bytes([0x00, 1, 8, 4])
    p[0x1a570] = bytes([0])
    entry = (bytes([0x00, 0x02]) + (0x90000).to_bytes(4, "big")
             + d7.to_bytes(2, "big") + x.to_bytes(2, "big")
             + y.to_bytes(2, "big")
             + (d2 if d2 is not None else w).to_bytes(2, "big")
             + h.to_bytes(2, "big"))
    for i, b in enumerate(entry):
        p[0x1a850 + i] = bytes([b])
    for i in range(0x10):
        p[0x1a860 + i] = bytes([0])
    return p


def _run_dispatch(entry, fn, pokes, waits):
    diffs, info = differential(
        entry, {"a6": 0x50000, "_pokes": pokes},
        lambda lib, buf, f=fn: f(buf, 0x50000),
        schedule=_vbl_k(waits) if waits else [],
        wait_sites=[VBL_SITE])
    assert not diffs, f"{entry:#x}\n{report(diffs)}"
    assert info["ret"] == info["regs"]["a6"], f"{entry:#x} a6"


def test_fun_11f64():
    for skip, waits in ((0, 5), (1, 1)):
        pokes = _icon_pokes(2, 3)
        pokes[0x1a570] = bytes([skip])
        _run_dispatch(0x11F64, harness._lib.overlay_dispatch_1, pokes, waits)


def test_fun_11f98():
    for skip, waits in ((0, 5), (1, 1)):
        pokes = _icon_pokes(2, 3)
        pokes[0x1a570] = bytes([skip])
        _run_dispatch(0x11F98, harness._lib.overlay_dispatch_2, pokes, waits)


def test_fun_11fd8():
    for skip, waits in ((0, 3), (1, 0)):
        pokes = _icon_pokes(16, 3)
        pokes[0x1a570] = bytes([skip])
        _run_dispatch(0x11FD8, harness._lib.overlay_dispatch_3, pokes, waits)


def test_fun_1200a():
    for skip, waits in ((0, 3), (1, 0)):
        pokes = _icon_pokes(16, 3)
        pokes[0x1a570] = bytes([skip])
        _run_dispatch(0x1200A, harness._lib.overlay_dispatch_4, pokes, waits)
    # H-1: con d7 almacenado != (x<<2)&0xf, 0x10fec recalcula la fase (no out[2]).
    for d7 in (3, 7, 0x0b):
        pokes = _icon_pokes(16, 3, x=8, d7=d7)
        pokes[0x1a570] = bytes([0])
        _run_dispatch(0x1200A, harness._lib.overlay_dispatch_4, pokes, 3)


def test_fun_12044():
    for skip, waits in ((0, 8), (1, 0)):
        pokes = _icon_pokes(16, 3, d2=2)
        pokes[0x1a570] = bytes([skip])
        _run_dispatch(0x12044, harness._lib.overlay_dispatch_5, pokes, waits)


def test_fun_12076():
    for skip, waits in ((0, 8), (1, 0)):
        pokes = _icon_pokes(16, 3, d2=2)
        pokes[0x1a570] = bytes([skip])
        _run_dispatch(0x12076, harness._lib.overlay_dispatch_6, pokes, waits)


harness._lib.scroll_up.argtypes = [_u8p] + [ctypes.c_uint32] * 5
harness._lib.scroll_up.restype = None
harness._lib.scroll_down.argtypes = [_u8p] + [ctypes.c_uint32] * 5
harness._lib.scroll_down.restype = None


def test_fun_11dee():
    for d1 in (0, 1):
        for d3 in (2, 3):
            pokes = _icon_pokes(16, 3)
            diffs, _ = differential(
                0x11DEE, {"d1": d1, "d2": 16, "d3": d3, "d7": 0,
                          "a0": 0x90000, "_pokes": pokes},
                lambda lib, buf, a=d1, c=d3: lib.scroll_up(buf, a, 16, c, 0, 0x90000),
                schedule=_vbl_k(d1 + 1), wait_sites=[VBL_SITE])
            assert not diffs, f"d1={d1} d3={d3}\n{report(diffs)}"


def test_fun_11e5e():
    for d1 in (0, 1):
        for d3 in (2, 3):
            pokes = _icon_pokes(16, 3)
            diffs, _ = differential(
                0x11E5E, {"d1": d1, "d2": 16, "d3": d3, "d7": 0,
                          "a0": 0x90000, "_pokes": pokes},
                lambda lib, buf, a=d1, c=d3: lib.scroll_down(buf, a, 16, c, 0, 0x90000),
                schedule=_vbl_k(d1 + 1), wait_sites=[VBL_SITE])
            assert not diffs, f"d1={d1} d3={d3}\n{report(diffs)}"


FUN_147C2 = 0x147C2
harness._lib.cursor_select_a.argtypes = [_u8p]
harness._lib.cursor_select_a.restype = ctypes.c_uint32


def test_fun_147c2():
    for cx in (0x0a, 0x14, 0x28, 0x30):
        pokes = {
            0x1a450: cx.to_bytes(2, "big"),
            0x1a452: (0x10).to_bytes(2, "big"),
            0x1a50e: (0x1234).to_bytes(2, "big"),
            0x1ab46: bytes([0x00]),
            0x1a48a: bytes([0x00]),
        }
        for i in range(10):
            base = 0x19b26 + i * 4
            pokes[base] = bytes([i * 4])
            pokes[base + 1] = bytes([i * 4 + 4])
            pokes[base + 2] = bytes([0x00])
            pokes[base + 3] = bytes([0xFF])
        diffs, _ = differential(FUN_147C2, {"_pokes": pokes},
                                lambda lib, buf: lib.cursor_select_a(buf))
        assert not diffs, f"cx={cx:#x}\n{report(diffs)}"


FUN_14926 = 0x14926
harness._lib.class_find.argtypes = [_u8p]
harness._lib.class_find.restype = None


def test_fun_14926():
    """Busca la clase (byte1 & 0xf) en las 0x29 entradas de 0x1aa72."""
    for target in (0, 1, 0x28, None):
        pokes = {0x1ACC9: bytes([3]), 0x1ACE6: bytes([0x55]),
                 0x1A772: (0xDEADBEEF).to_bytes(4, "big")}
        for i in range(0x29):
            base = 0x1AA72 + i * 5
            cls = 3 if i == target else 0x0A
            pokes[base] = bytes([i & 0xFF, cls, 0x11, 0x22, 0x33])
        diffs, _ = differential(FUN_14926, {"_pokes": pokes},
                                lambda lib, buf: lib.class_find(buf))
        assert not diffs, f"target={target}\n{report(diffs)}"


FUN_14966 = 0x14966
harness._lib.action_bar_hit_test.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.action_bar_hit_test.restype = None


def _rect10(x0, x1, y0, y1, sel, cmd, idx):
    return bytes([x0, x1, y0, y1, sel, 0xAA, (cmd >> 8) & 0xFF, cmd & 0xFF, idx, 0xBB])


def _cursor_pokes(cx, cy):
    return {0x1A450: (cx << 2).to_bytes(2, "big"), 0x1A452: cy.to_bytes(2, "big")}


def _rect_pokes(rects, extra):
    pokes = _cursor_pokes(5, 7)
    for i, r in enumerate(rects):
        for j, b in enumerate(r):
            pokes[0x1A6AE + i * 10 + j] = bytes([b])
    pokes.update(extra)
    return pokes


def test_fun_14966():
    """Hit-test de las D3 entradas de 10 B de 0x1a6ae (cursor, accion e indice)."""
    sentinels = {0x1AB46: bytes([0x55]), 0x1A48A: bytes([0x55]),
                 0x1ACE9: bytes([0x55]), 0x1A75E: b"\x55" * 4}
    miss = _rect10(0x40, 0x50, 0x40, 0x50, 0x00, 0x0000, 0)
    hit = _rect10(4, 8, 6, 9, 0x11, 0xC204, 3)
    # encontrada en la primera entrada
    pokes = _rect_pokes([hit], sentinels)
    diffs, _ = differential(FUN_14966, {"d3": 3, "_pokes": pokes},
                            lambda lib, buf: lib.action_bar_hit_test(buf, 3))
    assert not diffs, report(diffs)
    # encontrada en la segunda entrada (D3 = 2)
    pokes = _rect_pokes([miss, hit], sentinels)
    diffs, _ = differential(FUN_14966, {"d3": 2, "_pokes": pokes},
                            lambda lib, buf: lib.action_bar_hit_test(buf, 2))
    assert not diffs, report(diffs)
    # ninguna: D3 = 1 falla por el borrow, D3 = 3 recorre las tres
    for d3 in (1, 3):
        pokes = _rect_pokes([miss, miss, miss], sentinels)
        diffs, _ = differential(FUN_14966, {"d3": d3, "_pokes": pokes},
                                lambda lib, buf: lib.action_bar_hit_test(buf, d3))
        assert not diffs, f"d3={d3}\n{report(diffs)}"
    # indice 0: (0-1)*6 = 0x5fa
    hit0 = _rect10(4, 8, 6, 9, 0x00, 0x918E, 0)
    pokes = _rect_pokes([hit0], sentinels)
    diffs, _ = differential(FUN_14966, {"d3": 1, "_pokes": pokes},
                            lambda lib, buf: lib.action_bar_hit_test(buf, 1))
    assert not diffs, report(diffs)


FUN_1695A = 0x1695A
harness._lib.obj_name_table.argtypes = [_u8p]
harness._lib.obj_name_table.restype = None


def test_fun_1695a():
    """Formatea el nombre del objeto seleccionado: ramas <0x23 / >=0x23."""
    for cls, b1, b2 in ((0x22, 0x11, 0x30), (0x23, 0x22, 0x27), (0x50, 0x99, 0x80)):
        pokes = {0x1A81A: (0x50000).to_bytes(4, "big")}
        pokes[0x50000] = bytes([cls, b1, b2])
        for a in range(0x19970, 0x19975):
            pokes[a] = bytes([0xAA])
        diffs, _ = differential(FUN_1695A, {"_pokes": pokes},
                                lambda lib, buf: lib.obj_name_table(buf))
        assert not diffs, f"cls={cls:#x}\n{report(diffs)}"


FUN_155DC = 0x155DC
harness._lib.clock.argtypes = [_u8p]
harness._lib.clock.restype = None


def test_fun_155dc_reloj():
    """Reloj: 0x134da (1 VBL) + una pasada con VBL, 4 print_char y frame IKBD."""
    pokes = _cmd_elem_pokes(16, 3)
    pokes[0x1A570] = bytes([0x01])            # 0x134da por la rama corta
    pokes[0x19710] = bytes([0x00, 1, 8, 4])   # script fijo del reloj
    pokes[0x1A456] = bytes([0x00])
    pokes[0x1AB60] = (0x1234).to_bytes(2, "big")
    pokes[0x1A44F] = bytes([0xFF])
    schedule = _vbl_k(2) + [
        {"pc": 0x136B0, "nth": 1, "addr": 0x1A44F, "width": 1, "value": 0x01},
    ]
    diffs, _ = differential(
        FUN_155DC, {"_pokes": pokes},
        lambda lib, buf: lib.clock(buf),
        schedule=schedule, wait_sites=[VBL_SITE, 0x136B0])
    assert not diffs, report(diffs)
