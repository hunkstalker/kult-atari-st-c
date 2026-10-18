"""Differential tests para FUN_000159d2 @ 0x159d2 (intérprete de animación).

Primer corte: prólogo, selección de la entrada de la tabla 0x23ee8 con walk_entry
(0x12950) y despacho de los bytes 0xfe (fija $1a47a/$1a47c con sign-extension),
0xfd (sonido; no-op con el bypass $1ab4f) y 0xfc (Supexec; no-op con bypass).

Los 8 handlers que caen al motor de sprites 0x15bde quedan fuera (PENDIENTES), así
que los scripts sintéticos solo usan 0xfe/0xfd/0xfc.
"""
import ctypes

import harness
from harness import differential, report

import os as _os, re as _re
_NAME = {}
for _ln in open(_os.path.join(_os.path.dirname(_os.path.dirname(_os.path.abspath(__file__))), "work", "names.txt")):
    _m = _re.match(r"fn\s+0x([0-9a-fA-F]+)\s+(\S+)", _ln)
    if _m:
        _NAME[int(_m.group(1), 16)] = _m.group(2)


def _sym(addr, lib=None):
    return getattr(lib if lib is not None else harness._lib, _NAME[addr])

ANIM = 0x159d2
TABLE = 0x23ee8       # tabla de animación real en la imagen
SCRIPT = 0x30000      # qword sintético: [x][index][d0][d2]

_u8p = ctypes.POINTER(ctypes.c_uint8)
harness._lib.anim_entry_a.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.anim_entry_a.restype = None


def _entry(data):
    """Una entrada de la tabla: [u8 len][data], con len incluyéndose a sí mismo."""
    assert 1 <= len(data) + 1 <= 0xFF, f"entrada de {len(data) + 1} bytes no cabe"
    return bytes([len(data) + 1]) + bytes(data)


def _pokes():
    """Centinelas en los globales que toca el prólogo, bypass de sonido activo."""
    return {
        0x1a472: (0xaaaa).to_bytes(2, "big"),
        0x1a474: (0xbbbb).to_bytes(2, "big"),
        0x1a476: (0xcccc).to_bytes(2, "big"),
        0x1a478: (0xdddd).to_bytes(2, "big"),
        0x1a47a: (0xeeee).to_bytes(2, "big"),
        0x1a47c: (0xffff).to_bytes(2, "big"),
        0x1a4b6: (0x11223344).to_bytes(4, "big"),
        0x1ab4f: bytes([1]),        # bypass: los wrappers de sonido son no-ops
    }


def _run(pokes, index):
    pokes = dict(pokes)
    pokes[SCRIPT] = bytes([0x00, index, 0x12, 0x34])
    return differential(ANIM, {"a6": SCRIPT, "_pokes": pokes},
                        lambda lib, buf: lib.anim_entry_a(buf, SCRIPT))


def test_prologue_params_and_zeroing():
    """A6[2]/A6[3] van a $1a472/$1a474; $1a476/$1a478 se ponen a 0."""
    pokes = _pokes()
    pokes[TABLE] = _entry(bytes([0xfe, 0x00, 0x01]))
    diffs, info = _run(pokes, 1)
    assert not diffs, report(diffs)
    assert info["writes"], "el oráculo no escribió nada"


def test_fe_opcode_sign_extension():
    """0xfe: $1a47a/$1a47c = sign-extend de los dos bytes siguientes; A6 += 3."""
    for a, b in ((0x01, 0x80), (0x7f, 0xff), (0x00, 0x00), (0x80, 0x7f)):
        pokes = _pokes()
        pokes[TABLE] = _entry(bytes([0xfe, a, b]))
        diffs, _ = _run(pokes, 1)
        assert not diffs, f"a={a:#x} b={b:#x}\n{report(diffs)}"


def test_fe_multiple_updates_last_wins():
    """Varios 0xfe seguidos: el último fija los globals."""
    pokes = _pokes()
    pokes[TABLE] = _entry(bytes([0xfe, 0x01, 0x80, 0xfe, 0x7f, 0xff, 0xfe, 0xaa, 0x55]))
    diffs, _ = _run(pokes, 1)
    assert not diffs, report(diffs)


def test_entry_index_uses_walk_entry():
    """El índice A6[1] elige la entrada 1-based vía walk_entry."""
    e0 = _entry(bytes([0x00, 0x00]))                     # entrada 0 (se salta)
    e1 = _entry(bytes([0xfe, 0x05, 0xf0]))               # entrada 1
    pokes = _pokes()
    pokes[TABLE] = e0 + e1
    diffs, _ = _run(pokes, 2)
    assert not diffs, report(diffs)


def test_fd_and_fc_are_bypass_noops():
    """Con $1ab4f != 0, 0xfd (sonido) y 0xfc (Supexec) no tocan memoria."""
    pokes = _pokes()
    pokes[TABLE] = _entry(bytes([0xfd, 0x13, 0x00, 0xfc, 0xfd, 0x14, 0x01, 0xfc]))
    diffs, _ = _run(pokes, 1)
    assert not diffs, report(diffs)




def test_fd_starts_sound_without_bypass():
    """Con $1ab4f == 0, 0xfd dispara el envoltorio de sonido (18f8e/18f98).

    El original lee el indice de A6[1] y elige por A6[2]: !=0 -> 0x18f98, si no
    -> 0x18f8e. Sin este test, omitir las llamadas (como hacia el C) pasaba
    desapercibido: el ledger de eventos del oraculo es quien lo delata.
    """
    for idx, sel in ((0x13, 0x00), (0x14, 0x01)):
        pokes = _pokes()
        pokes[0x1ab4f] = bytes([0])          # sin bypass: el sonido corre
        pokes[TABLE] = _entry(bytes([0xfd, idx, sel, 0xfc]))
        diffs, _ = _run(pokes, 1)
        assert not diffs, f"idx={idx:#x} sel={sel:#x}\n{report(diffs)}"


# ---- helpers del motor de sprites ----

FUN_15A90 = 0x15a90
FUN_15D7E = 0x15d7e
SPR_PTR = 0x1a4c6
DEST = 0x90000
SRC = DEST - 0x8000

harness._lib.anim_bank_select.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.anim_bank_select.restype = ctypes.c_uint32
harness._lib.anim_restore.argtypes = [_u8p]
harness._lib.anim_restore.restype = None


def test_fun_15a90_sprite_bank():
    """0x15a90 elige el banco en $1a4c6 y devuelve A0."""
    for d0 in (0x00, 0x40, 0x80, 0xc0, 0x3f, 0x7f, 0xbf, 0xc1, 0xff):
        diffs, info = differential(
            FUN_15A90, {"d0": d0, "_pokes": {SPR_PTR: (0xdeadbeef).to_bytes(4, "big")}},
            lambda lib, buf, v=d0: lib.anim_bank_select(buf, v))
        assert not diffs, f"d0={d0:#x}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a0"], f"d0={d0:#x} a0"


def test_fun_15d7e_restore_under_element():
    """0x15d7e restaura la zona bajo el elemento y limpia $1a478."""
    for x, y, w, h in ((0, 0, 4, 1), (8, 3, 16, 2), (17, 5, 40, 3), (0x30, 0x20, 1, 4)):
        pokes = {
            0x1a45e: DEST.to_bytes(4, "big"),
            0x1a472: x.to_bytes(2, "big"),
            0x1a474: y.to_bytes(2, "big"),
            0x1a476: w.to_bytes(2, "big"),
            0x1a478: h.to_bytes(2, "big"),
            0x0d8ea: bytes([0x41]),
            0x1d402: bytes([0x5a]),
        }
        for i in range(0x1000):
            pokes[SRC + i] = bytes([(i * 3 + 1) & 0xff])
            pokes[DEST + i] = bytes([(i * 11 + 7) & 0xff])
        diffs, _ = differential(FUN_15D7E, {"_pokes": pokes},
                                lambda lib, buf: lib.anim_restore(buf))
        assert not diffs, f"x={x} y={y} w={w} h={h}\n{report(diffs)}"


FUN_15CD8 = 0x15cd8
FLAGS = 0x1a574
STEP = 0x1a46e
SRC2 = 0x40000

harness._lib.anim_counter.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.POINTER(ctypes.c_uint32),
                                     ctypes.POINTER(ctypes.c_uint32),
                                     ctypes.POINTER(ctypes.c_uint32),
                                     ctypes.POINTER(ctypes.c_uint32)]
harness._lib.anim_counter.restype = None


def test_fun_15cd8_step_adjust():
    """0x15cd8 ajusta contador/fuente según los flags $1a574; salidas por registro."""
    for fl in (0, 1, 2, 3, 4, 5, 6, 7):
        for step in (0, 1, 2, 5, 0x7fff, 0x8000, 0xffff):
            for d0, d2, d3, d4, d5 in ((10, 3, 7, 2, 1), (0, 0, 0, 0, 0),
                                       (0x20, 8, 4, 4, 2), (5, 1, 1, 1, 9),
                                       (10, 0xffff, 7, 2, 1),
                                       (10, 3, 7, 2, 0xfff9)):
                pokes = {FLAGS: bytes([fl]), STEP: step.to_bytes(2, "big")}
                regs = {"d0": d0, "d2": d2, "d3": d3, "d4": d4, "d5": d5, "a1": SRC2,
                        "_pokes": pokes}
                o0, o2, o3, o1 = (ctypes.c_uint32() for _ in range(4))
                diffs, info = differential(
                    FUN_15CD8, regs,
                    lambda lib, buf, a=d0, b=d2, c=d3, e=d4, f=d5:
                    lib.anim_counter(buf, a, b, c, e, f, SRC2,
                                    ctypes.byref(o0), ctypes.byref(o2),
                                    ctypes.byref(o3), ctypes.byref(o1)))
                tag = f"fl={fl} step={step} d0={d0} d2={d2} d3={d3} d4={d4} d5={d5}"
                assert not diffs, f"{tag}\n{report(diffs)}"
                assert o0.value == info["regs"]["d0"], f"{tag} d0 {o0.value:#x}"
                assert o2.value == info["regs"]["d2"], f"{tag} d2 {o2.value:#x}"
                assert o3.value == info["regs"]["d3"], f"{tag} d3 {o3.value:#x}"
                assert o1.value == info["regs"]["a1"], f"{tag} a1 {o1.value:#x}"


FUN_15DA6 = 0x15da6
VBL_SITE = 0x1359a
VBL_COUNTER = 0x1acd4

harness._lib.anim_engine_entry.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.anim_engine_entry.restype = None


def test_fun_15da6_draw_element():
    """0x15da6: respaldo + blit + VBL (schedule) + reveal/restore + estado."""
    for x, y, w, h, st, reveal in ((8, 3, 8, 2, 2, 0), (8, 3, 8, 2, 2, 1),
                                   (0, 0, 4, 1, 1, 0), (17, 5, 8, 3, 1, 1)):
        pokes = {
            0x1a45e: DEST.to_bytes(4, "big"),
            0x1a470: (0).to_bytes(2, "big"),     # sin retardo calibrado
            0x1a578: bytes([reveal]),
            0x1a56a: (0).to_bytes(2, "big"),
            0x1a577: bytes([1]),
            0x1a472: (0).to_bytes(2, "big"),
            0x1a474: (0).to_bytes(2, "big"),
            0x1a476: (0).to_bytes(2, "big"),
            0x1a478: (0).to_bytes(2, "big"),     # recorte desactivado
            VBL_COUNTER: bytes([0x10]),
        }
        for i in range(0x4000):
            pokes[SRC + i] = bytes([(i * 3 + 1) & 0xff])
            pokes[DEST + i] = bytes([(i * 11 + 7) & 0xff])
            pokes[SRC2 + i] = bytes([(i * 5 + 9) & 0xff])
        diffs, _ = differential(
            FUN_15DA6,
            {"d0": x, "d1": y, "d2": w, "d3": h, "d5": st, "a1": SRC2, "_pokes": pokes},
            lambda lib, buf, a=x, b=y, c=w, d=h, e=st:
            lib.anim_engine_entry(buf, a, b, c, d, e, SRC2),
            schedule=[{"pc": VBL_SITE, "nth": 1, "addr": VBL_COUNTER, "width": 1, "value": 0x11}],
            wait_sites=[VBL_SITE])
        assert not diffs, f"x={x} y={y} w={w} h={h} st={st} reveal={reveal}\n{report(diffs)}"


def test_fun_15da6_clip_with_previous_rect():
    """0x15da6 con `$1a478 != 0`: recorte contra el rectangulo previo.

    Es el camino que se activa a partir del segundo elemento del motor; con el
    clip desactivado el C pasaba, con el activo divergia (los globales del rect).
    """
    for x, y, w, h, st, reveal, rx, ry, rw, rh in (
        (1, 0, 8, 2, 2, 0, 1, 0, 8, 2),
        (3, 2, 8, 2, 1, 0, 1, 0, 8, 2),
        (0, 0, 8, 2, 1, 0, 5, 2, 8, 2),
        (9, 4, 4, 3, 2, 1, 2, 1, 6, 4),
        (16, 8, 8, 2, 1, 0, 0x40, 0x20, 8, 2),
    ):
        pokes = {
            0x1a45e: DEST.to_bytes(4, "big"),
            0x1a470: (0).to_bytes(2, "big"),
            0x1a578: bytes([reveal]),
            0x1a56a: (0).to_bytes(2, "big"),
            0x1a577: bytes([1]),
            0x1a472: rx.to_bytes(2, "big"),
            0x1a474: ry.to_bytes(2, "big"),
            0x1a476: rw.to_bytes(2, "big"),
            0x1a478: rh.to_bytes(2, "big"),
            VBL_COUNTER: bytes([0x10]),
        }
        for i in range(0x4000):
            pokes[SRC + i] = bytes([(i * 3 + 1) & 0xff])
            pokes[DEST + i] = bytes([(i * 11 + 7) & 0xff])
            pokes[SRC2 + i] = bytes([(i * 5 + 9) & 0xff])
        diffs, _ = differential(
            FUN_15DA6,
            {"d0": x, "d1": y, "d2": w, "d3": h, "d5": st, "a1": SRC2, "_pokes": pokes},
            lambda lib, buf, a=x, b=y, c=w, d=h, e=st:
            lib.anim_engine_entry(buf, a, b, c, d, e, SRC2),
            schedule=[{"pc": VBL_SITE, "nth": 1, "addr": VBL_COUNTER, "width": 1, "value": 0x11}],
            wait_sites=[VBL_SITE])
        assert not diffs, (f"x={x} y={y} w={w} h={h} st={st} reveal={reveal} "
                           f"prev=({rx},{ry},{rw},{rh})\n{report(diffs)}")


# ---- motor 0x15bde y handlers de la tabla 0x19eaa ----

SPR_BANK = 0xD8F0
# 0x15b94 (idx 7) usa el reveal ($1a578=1), que con un paso de 0 no termina; su
# camino reveal ya se cubre en 0x15da6 (reveal=1) y en test_reveal.py.
HANDLERS = {0x15bcc: 1, 0x15ba6: 6, 0x15bb2: 2, 0x15bbc: 4}
for _addr, _idx in HANDLERS.items():
    _fn = _sym(_addr)
    _fn.argtypes = [_u8p, ctypes.c_uint32]
    _fn.restype = None


def _engine_pokes(script_extra=b""):
    pokes = {
        0x1a45e: DEST.to_bytes(4, "big"),
        0x1a472: (0).to_bytes(2, "big"),
        0x1a474: (0).to_bytes(2, "big"),
        0x1a476: (8).to_bytes(2, "big"),     # ancho previo (0x15bbc lo usa como contador)
        0x1a478: (0).to_bytes(2, "big"),     # recorte desactivado
        0x1a47a: (0).to_bytes(2, "big"),
        0x1a47c: (0).to_bytes(2, "big"),
        VBL_COUNTER: bytes([0x10]),
        SPR_BANK: bytes([8, 2]),          # cabecera del sprite: ancho 8, alto 2
    }
    for i in range(0x800):
        pokes[SRC + i] = bytes([(i * 3 + 1) & 0xff])
        pokes[DEST + i] = bytes([(i * 11 + 7) & 0xff])
        pokes[SPR_BANK + 2 + i] = bytes([(i * 5 + 9) & 0xff])
    return pokes


def test_engine_handlers():
    """Cada handler fija sus globales y cae al motor 0x15bde (script sintético)."""
    for addr, idx in HANDLERS.items():
        # byte0 = (nº bloques << 3) | handler ; bloque: 1 elemento ; elemento ; cola
        script = bytes([(1 << 3) | idx, 1, 0, 0])
        pokes = _engine_pokes()
        pokes[SCRIPT] = script
        diffs, _ = differential(
            addr, {"a6": SCRIPT, "_pokes": pokes},
            lambda lib, buf, f=_sym(addr), s=script:
            f(buf, SCRIPT),
            schedule=[{"pc": VBL_SITE, "nth": 1, "addr": VBL_COUNTER, "width": 1, "value": 0x11}],
            wait_sites=[VBL_SITE])
        assert not diffs, f"handler {addr:#x} idx={idx}\n{report(diffs)}"


# ---- blitters de bytes del handler 0x15abe ----

SHUF = 0x12b40
BLIT_FWD = 0x12ad6
BLIT_REV = 0x12b04

harness._lib.bit_permute_byte.argtypes = [ctypes.c_uint32]
harness._lib.bit_permute_byte.restype = ctypes.c_uint32
harness._lib.elem_blit.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.elem_blit.restype = None
harness._lib.elem_blit_rev.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.elem_blit_rev.restype = None


def test_fun_12b40_bit_shuffle():
    for d1 in range(256):
        diffs, info = differential(SHUF, {"d1": d1},
                                   lambda lib, buf, v=d1: lib.bit_permute_byte(v))
        assert not diffs, f"d1={d1:#x}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["d6"], f"d1={d1:#x}"


def _blit8_pokes():
    pokes = {}
    for i in range(0x1000):
        pokes[SRC + i] = bytes([(i * 7 + 3) & 0xff])
        pokes[DEST - 0x100 + i] = bytes([(i * 13 + 5) & 0xff])
    return pokes


def test_byte_blitters():
    for d2, d3, a3 in ((1, 1, 0x40), (2, 3, 0x40), (4, 2, 0x10), (8, 5, 0x40)):
        pokes = _blit8_pokes()
        regs = {"a0": DEST, "a1": SRC, "d2": d2, "d3": d3, "a3": a3, "_pokes": pokes}
        diffs, _ = differential(
            BLIT_FWD, regs,
            lambda lib, buf, a=d2, b=d3, c=a3: lib.elem_blit(buf, DEST, SRC, a, b, c))
        assert not diffs, f"fwd d2={d2} d3={d3} a3={a3:#x}\n{report(diffs)}"
        # `differential` CONSUMES `_pokes` from the dict (regs.pop), so the reversed pass
        # needs its own copy or it would run on an empty image and verify nothing.
        regs = {"a0": DEST, "a1": SRC, "d2": d2, "d3": d3, "a3": a3, "_pokes": _blit8_pokes()}
        diffs, _ = differential(
            BLIT_REV, regs,
            lambda lib, buf, a=d2, b=d3, c=a3: lib.elem_blit_rev(buf, DEST, SRC, a, b, c))
        assert not diffs, f"rev d2={d2} d3={d3} a3={a3:#x}\n{report(diffs)}"


# ---- handler 0x15abe (idx 0) ----

FUN_15ABE = 0x15abe
TBL_27FE2 = 0x27fe2
TBL_28AD2 = 0x28ad2

harness._lib.anim_bank_build.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.anim_bank_build.restype = None


def _abe_pokes(flag_hi):
    # entrada 0 de 0x27fe2: [len=6][cabecera banco=1,0][subentrada idx=0, x_lo, x_hi]
    entry = bytes([6, 1, 0, 0, 0, flag_hi])
    # bitmap 0 de 0x28ad2: len word 12, [filas=2][cols=1] + datos
    pokes = {
        SCRIPT: bytes([0x00, 0x00]),
        TBL_27FE2: entry,
        TBL_28AD2: bytes([0x00, 0x0c, 2, 1]),
        0x1a45e: DEST.to_bytes(4, "big"),
    }
    for i in range(0x40):
        pokes[TBL_28AD2 + 4 + i] = bytes([(i * 7 + 3) & 0xff])
    return pokes


def test_fun_15abe_handler():
    for flag_hi in (0x00, 0x80):
        pokes = _abe_pokes(flag_hi)
        diffs, _ = differential(FUN_15ABE, {"a6": SCRIPT, "_pokes": pokes},
                                lambda lib, buf: lib.anim_bank_build(buf, SCRIPT))
        assert not diffs, f"flag_hi={flag_hi:#x}\n{report(diffs)}"


def test_fun_159d2_dispatches_handler():
    """El despacho de 0x159d2 llama al handler (byte & 7) y usa el A6 devuelto."""
    # entrada 0 de la tabla: [len=4][byte0=(1<<3)|1, bh=1, elem=0]; el motor relee A6
    # tras el bloque, así que la "cola" leída es el byte bh (véase anim_engine).
    table_entry = bytes([4, (1 << 3) | 1, 1, 0])
    pokes = _engine_pokes()
    pokes[TABLE] = table_entry
    pokes[SCRIPT] = bytes([0x00, 0x01, 0x00, 0x00])   # index=1
    diffs, _ = differential(
        ANIM, {"a6": SCRIPT, "_pokes": pokes},
        lambda lib, buf: lib.anim_entry_a(buf, SCRIPT),
        schedule=[{"pc": VBL_SITE, "nth": 1, "addr": VBL_COUNTER, "width": 1, "value": 0x11}],
        wait_sites=[VBL_SITE])
    assert not diffs, report(diffs)


FUN_15F6C = 0x15F6C
FUN_15FD4 = 0x15FD4
harness._lib.script_arg.argtypes = [_u8p, ctypes.POINTER(ctypes.c_uint32),
                                     ctypes.POINTER(ctypes.c_uint32)]
harness._lib.script_arg.restype = ctypes.c_uint32
harness._lib.script_alu.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.script_alu.restype = ctypes.c_uint32


def test_fun_15f6c():
    TABLE = 0x1a752
    for mode in (0, 0x20, 0x80, 0x80 | 0x20, 0x80 | 0x40, 0x80 | 0x20 | 0x40):
        pokes = {}
        script = bytearray()
        script.append(mode)
        if mode & 0x80:
            if mode & 0x40:
                script.append(0)                 # d4: 0 entradas de texto
            script.append(0)                     # d2 = offset
            pokes[TABLE + (mode & 0x1f) * 4] = (0x50000).to_bytes(4, "big")
            pokes[0x50000] = bytes([0x12, 0x34, 0x56])
            pokes[0x50001] = bytes([0x34])
        else:
            script.append(0x12)
            if mode & 0x20:
                script.append(0x34)
        for i, b in enumerate(script):
            pokes[0x40000 + i] = bytes([b])
        a6 = ctypes.c_uint32(0x40000)
        a0 = ctypes.c_uint32(0)
        diffs, info = differential(
            FUN_15F6C, {"a6": 0x40000, "_pokes": pokes},
            lambda lib, buf, p=a6, q=a0: lib.script_arg(buf, ctypes.byref(p),
                                                         ctypes.byref(q)))
        assert not diffs, f"mode={mode:#x}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["d0"], f"mode={mode:#x} d0"
        assert a6.value == info["regs"]["a6"], f"mode={mode:#x} a6"
        assert a0.value == info["regs"]["a0"], f"mode={mode:#x} a0"


def test_fun_15fd4():
    """El original usa operaciones .w (add.w/sub.w/eor.w y move.w/clr.w del
    resultado): la palabra alta de D6 se conserva. El test antiguo solo usaba
    D6 con palabra alta 0, asi que el enmascarado del C no se veia; ahora se
    incluyen valores con palabra alta y se compara el resultado completo."""
    for d6 in (0, 0x1234, 0x8000, 0xffff, 0xC20000, 0x12345678, 0x80000000):
        for d0 in (0, 0x1234, 0x8000, 0xffff, 0xC20000, 0x00000040):
            for d7 in (0x00, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01,
                       0x40, 0x40 | 0x20, 0x40 | 0x10, 0x40 | 0x08,
                       0x40 | 0x04, 0x40 | 0x02, 0x40 | 0x01, 0x40 | 0x3f):
                diffs, info = differential(
                    FUN_15FD4, {"d6": d6, "d0": d0, "d7": d7, "_pokes": {}},
                    lambda lib, buf, a=d6, b=d0, c=d7: lib.script_alu(buf, a, b, c))
                assert not diffs, f"d6={d6:#x} d0={d0:#x} d7={d7:#x}\n{report(diffs)}"
                assert info["ret"] == info["regs"]["d6"], (
                    f"d6={d6:#x} d0={d0:#x} d7={d7:#x} ret={info['ret']:#x} "
                    f"oraculo={info['regs']['d6']:#x}")


FUN_1606E = 0x1606E
FUN_16088 = 0x16088
FUN_160AA = 0x160AA
harness._lib.script_expr.argtypes = [_u8p, ctypes.POINTER(ctypes.c_uint32),
                                     ctypes.POINTER(ctypes.c_uint32)]
harness._lib.script_expr.restype = ctypes.c_uint32
harness._lib.script_store.argtypes = [_u8p, ctypes.POINTER(ctypes.c_uint32)]
harness._lib.script_store.restype = None
harness._lib.script_jump_zero.argtypes = [_u8p, ctypes.POINTER(ctypes.c_uint32)]
harness._lib.script_jump_zero.restype = None


def test_fun_1606e():
    # operando = [flags][valor]; expresión = [arg][op arg]...[terminador 0x80]
    cases = [
        (bytes([0x00, 0x05, 0x20, 0x00, 0x03, 0x80]), 8),
        (bytes([0x00, 0x12, 0x08, 0x00, 0xF0, 0x80]), 0x10),
        (bytes([0x00, 0x7F, 0x10, 0x00, 0x01, 0x80]), 0x7E),
    ]
    for script, _exp in cases:
        pokes = {}
        for i, b in enumerate(script):
            pokes[0x40000 + i] = bytes([b])
        a6 = ctypes.c_uint32(0x40000)
        a0 = ctypes.c_uint32(0)
        diffs, info = differential(
            FUN_1606E, {"a6": 0x40000, "_pokes": pokes},
            lambda lib, buf, p=a6, q=a0: lib.script_expr(buf, ctypes.byref(p),
                                                         ctypes.byref(q)))
        assert not diffs, f"{script.hex()}\n{report(diffs)}"
        assert info["ret"] == (info["regs"]["d6"] & 0xffff), script.hex()
        assert a6.value == info["regs"]["a6"], script.hex()


def test_fun_16088_160aa():
    # 0x16088 byte: destino directo (d0=0x50, a0=0 -> store byte) y valor 0x55.
    pokes = {}
    script = bytes([0xAA, 0x00, 0x50, 0x00, 0x55, 0x80])
    for i, b in enumerate(script):
        pokes[0x40000 + i] = bytes([b])
    a6 = ctypes.c_uint32(0x40000)
    diffs, info = differential(FUN_16088, {"a6": 0x40000, "_pokes": pokes},
                               lambda lib, buf, p=a6: lib.script_store(buf, ctypes.byref(p)))
    assert not diffs, report(diffs)
    assert a6.value == info["regs"]["a6"]
    # 0x16088 word: destino indirecto con bit5 -> d0 bit31, store word en a0.
    pokes2 = {0x1a752: (0x50000).to_bytes(4, "big"), 0x50000: bytes([0x00, 0x00])}
    script2 = bytes([0xAA, 0xA0, 0x00, 0x00, 0x55, 0x80])
    for i, b in enumerate(script2):
        pokes2[0x40000 + i] = bytes([b])
    a6b = ctypes.c_uint32(0x40000)
    diffs, info = differential(FUN_16088, {"a6": 0x40000, "_pokes": pokes2},
                               lambda lib, buf, p=a6b: lib.script_store(buf, ctypes.byref(p)))
    assert not diffs, report(diffs)
    assert a6b.value == info["regs"]["a6"]
    # 0x160aa: expresión 0 -> 0x1350c (a6 = TEMPL + word)
    pokes3 = {0x1d402: bytes([0, 2])}
    script3 = bytes([0xAA, 0x00, 0x00, 0x80])
    for i, b in enumerate(script3):
        pokes3[0x40000 + i] = bytes([b])
    a6c = ctypes.c_uint32(0x40000)
    diffs, info = differential(FUN_160AA, {"a6": 0x40000, "_pokes": pokes3},
                               lambda lib, buf, p=a6c: lib.script_jump_zero(buf, ctypes.byref(p)))
    assert not diffs, report(diffs)
    assert a6c.value == info["regs"]["a6"]


ANIM_BODY = 0x159E2
harness._lib.anim_entry_b.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32, ctypes.c_uint32]
harness._lib.anim_entry_b.restype = ctypes.c_uint32


def test_fun_159e2():
    """Entrada gemela de 0x159d2: D0/D1/D2 ya en registros y A6 sin tocar."""
    for d0, d1, d2 in ((0x12, 1, 0x34), (0x00, 2, 0xFF), (0x7F, 1, 0x01)):
        pokes = _pokes()
        pokes[TABLE] = (_entry(bytes([0xFE, 0x00, 0x01])) +
                        _entry(bytes([0xFE, 0x7F, 0x80])))
        diffs, info = differential(
            ANIM_BODY, {"a6": SCRIPT, "d0": d0, "d1": d1, "d2": d2, "_pokes": pokes},
            lambda lib, buf, a=d0, b=d1, c=d2: lib.anim_entry_b(buf, SCRIPT, a, b, c))
        assert not diffs, f"d0={d0:#x} d1={d1:#x} d2={d2:#x}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"]


FUN_14F24 = 0x14F24
harness._lib.anim_threshold.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.anim_threshold.restype = None


def _umbral_table():
    """0x44 entradas de 1 byte (fin inmediato) para que 0x159e2 no ejecute nada."""
    return {0x23EE8 + i: bytes([0x01]) for i in range(0x44)}


def test_fun_14f24():
    """Umbral 0x1ad20: sin disparo (mismo nibble alto) y con disparo (0x159e2)."""
    # sin disparo: 0x1ad20 = 0x12, d0 = 1 -> 0x11 (mismo nibble alto)
    pokes = _umbral_table()
    pokes[0x1AD20] = bytes([0x12])
    diffs, _ = differential(0x14F24, {"a6": 0x50000, "d0": 1, "_pokes": pokes},
                            lambda lib, buf: lib.anim_threshold(buf, 0x50000, 1))
    assert not diffs, report(diffs)
    # disparo por cambio de nibble: 0x1ad20 = 0x10, d0 = 1 -> 0x0f
    pokes = _umbral_table()
    pokes[0x1AD20] = bytes([0x10])
    diffs, _ = differential(0x14F24, {"a6": 0x50000, "d0": 1, "_pokes": pokes},
                            lambda lib, buf: lib.anim_threshold(buf, 0x50000, 1),
                            stop_pc=0x14F5C)
    assert not diffs, report(diffs)
    # disparo por subdesbordamiento: 0x1ad20 = 2, d0 = 5
    pokes = _umbral_table()
    pokes[0x1AD20] = bytes([2])
    diffs, _ = differential(0x14F24, {"a6": 0x50000, "d0": 5, "_pokes": pokes},
                            lambda lib, buf: lib.anim_threshold(buf, 0x50000, 5),
                            stop_pc=0x14F5C)
    assert not diffs, report(diffs)


def _vbl_schedule(n):
    """Un store por espera de VBL (cada elemento de 15da6 espera un VBL)."""
    return [{"pc": VBL_SITE, "nth": i, "addr": VBL_COUNTER, "width": 1,
             "value": 0x10 + i} for i in range(1, n + 1)]


def test_engine_multi_block_elements():
    """El motor con varios bloques/elementos (el C solo estaba probado 1x1).

    `15ba6` (idx 6) fija el contador a `$1a478` = 0 y el motor restaura/para en
    el primer elemento; el resto recorre todos los elementos.
    """
    cases = [
        (bytes([(2 << 3) | 1, (1 << 3) | 2, 0x10, 0x30,
                (2 << 3) | 2, 0x20, 0x40, 0x00]), 4),
        (bytes([(3 << 3) | 1, (1 << 3) | 1, 0x08,
                (2 << 3) | 1, 0x18, (1 << 3) | 1, 0x28, 0x00]), 3),
    ]
    # 0x15ba6 (idx 6) usa contador 0 y con estos scripts el estado sintetico
    # dispara el bucle de restauracion; su parada ya esta cubierta por
    # test_engine_handlers (1 elemento).
    for addr, idx in HANDLERS.items():
        if addr == 0x15ba6:
            continue
        for script, elems in cases:
            waits = elems
            pokes = _engine_pokes()
            pokes[SCRIPT] = script
            diffs, _ = differential(
                addr, {"a6": SCRIPT, "_pokes": pokes},
                lambda lib, buf, f=_sym(addr), s=script:
                f(buf, SCRIPT),
                schedule=_vbl_schedule(waits),
                wait_sites=[VBL_SITE])
            assert not diffs, (f"handler {addr:#x} idx={idx} elems={elems} "
                               f"script={script.hex()}\n{report(diffs)}")


def test_engine_returns_a6_like_the_oracle():
    """El A6 devuelto por el motor (el VM lo usa para avanzar el guion).

    El harness diferencial compara memoria, no registros: sin este test, un A6
    distinto pasaria desapercibido y el guion leeria bytes equivocados (fases
    de animacion repetidas / aceleradas).
    """
    cases = [
        (bytes([0x09, 0x09, 0x00, 0x00]), 1),                        # 1 bloque, cola 00
        (bytes([0x09, 0x09, 0x00, 0x03]), 1),                        # 1 bloque, cola 03
        (bytes([0x11, 0x09, 0x00, 0x09, 0x38, 0x00]), 2),            # 2 bloques
        (bytes([0x11, 0x0a, 0x10, 0x30, 0x12, 0x20, 0x40, 0x00]), 4),  # 2 bloques x 2
    ]
    for addr in (0x15bcc, 0x15bb2, 0x15bbc):
        _sym(addr).restype = ctypes.c_uint32
        for script, waits in cases:
            pokes = _engine_pokes()
            pokes[SCRIPT] = script
            diffs, info = differential(
                addr, {"a6": SCRIPT, "_pokes": pokes},
                lambda lib, buf, a=addr: _sym(a, lib)(buf, SCRIPT),
                stop_pc=0x15CC2,
                schedule=_vbl_schedule(waits),
                wait_sites=[VBL_SITE])
            assert not diffs, f"handler {addr:#x} script={script.hex()}\n{report(diffs)}"
            assert info["ret"] == info["regs"]["a6"], (
                f"handler {addr:#x} script={script.hex()}: "
                f"orac={info['regs']['a6']:#x} cand={info['ret']:#x}")
