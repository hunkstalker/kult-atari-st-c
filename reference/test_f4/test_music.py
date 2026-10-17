"""Tests diferenciales del motor de musica: handlers del PSG (0x3aa38/0x3aa8a/0x3aa7c)
y el silenciador de tres canales 0x3a85e.

El `trap #3` del original vectoriza a 0x23 (direccion 0x8c), que init_hardware llena
con Setexc; el case potea el vector antes de correr 0x3a85e. El candidato no tiene
traps: llama a la reconstruccion del handler.
"""
import ctypes

import harness
from harness import differential, report

_u8p = ctypes.POINTER(ctypes.c_uint8)
_u32p = ctypes.POINTER(ctypes.c_uint32)

FUN_3AA7C = 0x3AA7C
FUN_3AA38 = 0x3AA38
FUN_3AA8A = 0x3AA8A
FUN_3A85E = 0x3A85E
RTE_PC = 0x3AA7A
HANDLER_TRAP3 = 0x3AA38
VEC_TRAP3 = 0x8C

harness._lib.g_fun_3aa7c.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_3aa7c.restype = None
harness._lib.g_fun_3aa38.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32, _u32p]
harness._lib.g_fun_3aa38.restype = None
harness._lib.g_fun_3aa8a.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32, _u32p]
harness._lib.g_fun_3aa8a.restype = None
harness._lib.g_fun_3a85e.argtypes = [_u8p, ctypes.c_uint32, _u32p, _u32p, _u32p]
harness._lib.g_fun_3a85e.restype = None


def test_fun_3aa7c_write_verify():
    """Escribe D6 con D0 y relee para verificar: WRITE + READ en el ledger."""
    for reg, val in ((8, 0x00), (7, 0x3F), (0x0D, 0xAB)):
        diffs, _ = differential(
            FUN_3AA7C, {"d6": reg, "d0": val, "a5": 0xFF8800, "_pokes": {}},
            lambda lib, buf, r=reg, v=val: lib.g_fun_3aa7c(buf, r, v))
        assert not diffs, f"reg={reg} val={val:#x}\n{report(diffs)}"


def test_fun_3aa38_escribe_registro():
    """D2 sin bit15: registro D2+D7, valor D0; D0 se restaura al salir."""
    d0 = ctypes.c_uint32(0xDEADBEEF)
    diffs, info = differential(
        FUN_3AA38, {"d0": 0x1F, "d2": 0, "d7": 8, "_pokes": {}},
        lambda lib, buf: lib.g_fun_3aa38(buf, 0x1F, 0, 8, ctypes.byref(d0)),
        stop_pc=RTE_PC)
    assert not diffs, report(diffs)
    assert d0.value == info["regs"]["d0"] == 0x1F


def test_fun_3aa38_mixer_rmw():
    """D2==7: lee el mixer, conserva bits 6-7 del chip y D0[0..5]."""
    d0 = ctypes.c_uint32(0xDEADBEEF)
    diffs, info = differential(
        FUN_3AA38, {"d0": 0x12, "d2": 7, "d7": 0, "_pokes": {}},
        lambda lib, buf: lib.g_fun_3aa38(buf, 0x12, 7, 0, ctypes.byref(d0)),
        stop_pc=RTE_PC, psg_seed={7: 0x80})
    assert not diffs, report(diffs)
    assert d0.value == info["regs"]["d0"] == 0x12


def test_fun_3aa38_par_relativo():
    """D2 bit15: par de registros 2*D7+D2 y +1; byte bajo y byte alto de D0."""
    d0 = ctypes.c_uint32(0xDEADBEEF)
    diffs, info = differential(
        FUN_3AA38, {"d0": 0x1234, "d2": 0x8005, "d7": 2, "_pokes": {}},
        lambda lib, buf: lib.g_fun_3aa38(buf, 0x1234, 0x8005, 2, ctypes.byref(d0)),
        stop_pc=RTE_PC)
    assert not diffs, report(diffs)
    assert d0.value == info["regs"]["d0"] == 0x1234


def test_fun_3aa8a_sombra_byte():
    """Sin bit15: escribe el byte de D0 en la tabla sombra $3ae7c+D2+D7."""
    d0 = ctypes.c_uint32(0xDEADBEEF)
    diffs, info = differential(
        FUN_3AA8A, {"d0": 0xABCD, "d2": 0, "d7": 8, "_pokes": {}},
        lambda lib, buf: lib.g_fun_3aa8a(buf, 0xABCD, 0, 8, ctypes.byref(d0)))
    assert not diffs, report(diffs)
    assert d0.value == info["regs"]["d0"] == 0xABCD


def test_fun_3aa8a_sombra_word():
    """Bit15: word de D0 en $3ae7c + (2*D7+D2)."""
    d0 = ctypes.c_uint32(0xDEADBEEF)
    diffs, info = differential(
        FUN_3AA8A, {"d0": 0x1234, "d2": 0x8003, "d7": 0, "_pokes": {}},
        lambda lib, buf: lib.g_fun_3aa8a(buf, 0x1234, 0x8003, 0, ctypes.byref(d0)))
    assert not diffs, report(diffs)
    assert d0.value == info["regs"]["d0"] == 0x1234


def test_fun_3a85e_silencia_tres_canales():
    """Tres `trap #3` (D2=8, D7=0..2, D0=0): registros 8/9/10 a 0."""
    pokes = {VEC_TRAP3: HANDLER_TRAP3.to_bytes(4, "big")}
    d0 = ctypes.c_uint32(0xDEADBEEF)
    d2 = ctypes.c_uint32(0xDEADBEEF)
    d7 = ctypes.c_uint32(0xDEADBEEF)
    diffs, info = differential(
        FUN_3A85E, {"d2": 0, "_pokes": pokes},
        lambda lib, buf: lib.g_fun_3a85e(buf, 0, ctypes.byref(d0),
                                         ctypes.byref(d2), ctypes.byref(d7)))
    assert not diffs, report(diffs)
    assert d0.value == info["regs"]["d0"] == 0
    assert d2.value == info["regs"]["d2"] == 0x00000008
    assert d7.value == info["regs"]["d7"] == 2


def test_fun_3a85e_conserva_mitad_alta_de_d2():
    """El original fija D2 con `move.w #8`: la mitad alta del registro se conserva."""
    pokes = {VEC_TRAP3: HANDLER_TRAP3.to_bytes(4, "big")}
    d0 = ctypes.c_uint32(0)
    d2 = ctypes.c_uint32(0)
    d7 = ctypes.c_uint32(0)
    diffs, info = differential(
        FUN_3A85E, {"d2": 0x00010000, "_pokes": pokes},
        lambda lib, buf: lib.g_fun_3a85e(buf, 0x00010000, ctypes.byref(d0),
                                         ctypes.byref(d2), ctypes.byref(d7)))
    assert not diffs, report(diffs)
    assert d2.value == info["regs"]["d2"] == 0x00010008


# ================= init/MFP, despacho y tracker (0x3a556..0x3aab6) =================

FUN_3A604 = 0x3A604
FUN_3A5C2 = 0x3A5C2
FUN_3A57C = 0x3A57C
FUN_3A62E = 0x3A62E
FUN_3A734 = 0x3A734
FUN_3A556 = 0x3A556
FUN_3A712 = 0x3A712
FUN_3A6F4 = 0x3A6F4
FUN_3A992 = 0x3A992
FUN_3AAB6 = 0x3AAB6
CMD_3AD5A = 0x3AD5A
CMD_3AD6A = 0x3AD6A
CMD_3ADB4 = 0x3ADB4
CMD_3AE00 = 0x3AE00
CMD_3AE68 = 0x3AE68
SP = harness.emu.STACK_TOP
STATE = 0x3AE8C
CH0 = 0x3AFBC
RTE_3AD58 = 0x3AD58
CONT_3AB3A = 0x3AB3A

harness._lib.g_fun_3a604.argtypes = [_u8p]
harness._lib.g_fun_3a604.restype = None
harness._lib.g_fun_3a5c2.argtypes = [_u8p, _u32p]
harness._lib.g_fun_3a5c2.restype = ctypes.c_uint32
harness._lib.g_fun_3a57c.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_3a57c.restype = ctypes.c_uint32
harness._lib.g_fun_3a62e.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32]
harness._lib.g_fun_3a62e.restype = None
harness._lib.g_fun_3a734.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_3a734.restype = ctypes.c_uint32
harness._lib.g_fun_3a556.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_fun_3a556.restype = ctypes.c_uint32
harness._lib.g_fun_3a712.argtypes = [_u8p]
harness._lib.g_fun_3a712.restype = None
harness._lib.g_fun_3a6f4.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_fun_3a6f4.restype = None
harness._lib.g_fun_3a992.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32]
harness._lib.g_fun_3a992.restype = ctypes.c_uint32
harness._lib.g_fun_3aab6.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                     ctypes.c_uint32]
harness._lib.g_fun_3aab6.restype = None
for _name, _args in (("g_fun_3ad5a", [_u8p, ctypes.c_uint32]),
                     ("g_fun_3ad6a", [_u8p, ctypes.c_uint32, ctypes.c_uint32]),
                     ("g_fun_3adb4", [_u8p, ctypes.c_uint32, ctypes.c_uint32]),
                     ("g_fun_3ae00", [_u8p, ctypes.c_uint32, ctypes.c_uint32]),
                     ("g_fun_3ae68", [_u8p, ctypes.c_uint32])):
    getattr(harness._lib, _name).argtypes = _args
    getattr(harness._lib, _name).restype = None


def _b32(value):
    return (value & 0xFFFFFFFF).to_bytes(4, "big")


def _poke32(pokes, addr, value):
    pokes[addr] = _b32(value)


def _stack_word_args(d5, d1):
    """Args de 0x3a734/0x3a62e en la pila: word en STACK_TOP+4, word en STACK_TOP+6."""
    return {SP + 4: (d5 & 0xFFFF).to_bytes(2, "big"),
            SP + 6: (d1 & 0xFFFF).to_bytes(2, "big")}


def test_fun_3a604_mfp_setup():
    diffs, _ = differential(FUN_3A604, {"_pokes": {}},
                            lambda lib, buf: lib.g_fun_3a604(buf))
    assert not diffs, report(diffs)


def test_fun_3a5c2_supexec_setexc():
    d0 = ctypes.c_uint32(0)
    diffs, info = differential(
        FUN_3A5C2, {"_pokes": {}},
        lambda lib, buf: lib.g_fun_3a5c2(buf, ctypes.byref(d0)))
    assert not diffs, report(diffs)
    assert d0.value == info["regs"]["d0"]


def _table_pokes(pairs, base=0x3AF00):
    pokes = {}
    a = base
    for key, handler in pairs:
        pokes[a] = (key & 0xFFFF).to_bytes(2, "big")
        _poke32(pokes, a + 2, handler)
        a += 6
    pokes[a] = (0).to_bytes(2, "big")
    return pokes, base


def test_fun_3a57c_no_encontrado():
    pokes, base = _table_pokes([])
    diffs, info = differential(
        FUN_3A57C, {"a0": base, "d1": 0x1234, "d5": 0xAB, "a6": STATE,
                    "_pokes": pokes},
        lambda lib, buf: lib.g_fun_3a57c(buf, base, 0x1234, 0xAB, STATE))
    assert not diffs, report(diffs)
    assert info["ret"] == 1, "tabla agotada -> C=1"


def test_fun_3a57c_handlers():
    """Los tres codelets del despacho (0x400 duraciones, 0x800 fade, 0x1000 canal)."""
    for key, handler, d5 in ((0x0400, 0x3A59C, 0xAB), (0x0800, 0x3A5AA, 0x12),
                             (0x1000, 0x3A5B8, 0x34)):
        pokes, base = _table_pokes([(key, handler)])
        diffs, info = differential(
            FUN_3A57C, {"a0": base, "d1": key, "d5": d5, "a6": STATE,
                        "_pokes": pokes},
            lambda lib, buf, b=base, k=key, v=d5: lib.g_fun_3a57c(buf, b, k, v, STATE))
        assert not diffs, f"key={key:#x}\n{report(diffs)}"
        assert info["ret"] == 0, f"key={key:#x} -> C=0"


def test_fun_3a62e_carga_cancion():
    pokes = {SP + 4: (0).to_bytes(2, "big"),
             SP + 6: (0x3B1AC).to_bytes(4, "big"),
             0x8C: HANDLER_TRAP3.to_bytes(4, "big")}
    diffs, _ = differential(
        FUN_3A62E, {"d2": 0, "_pokes": pokes},
        lambda lib, buf: lib.g_fun_3a62e(buf, 0, 0x3B1AC, 0))
    assert not diffs, report(diffs)


def test_fun_3a734_comandos():
    cases = ((0x00A7, 0x0000),   # llamada de init_hardware
             (0x0400, 0x00AB),   # duraciones via despacho
             (0x0800, 0x0012),
             (0x1000, 0x0034),
             (0x000F, 0x000F),   # seleccion de canales, sin comandos
             (0x0010, 0x0007),   # minimo de 0x24
             (0x00FF, 0x000F))   # bit7 (flags) + volumen + fade
    for d1, d5 in cases:
        diffs, info = differential(
            FUN_3A734, {"_pokes": _stack_word_args(d5, d1)},
            lambda lib, buf, a=d5, b=d1: lib.g_fun_3a734(buf, a, b))
        assert not diffs, f"d1={d1:#x} d5={d5:#x}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["d0"], f"d1={d1:#x}"


def test_fun_3a712_para_timer_a():
    diffs, _ = differential(FUN_3A712, {"_pokes": {}},
                            lambda lib, buf: lib.g_fun_3a712(buf))
    assert not diffs, report(diffs)


def test_fun_3a6f4_supexec_para_y_silencia():
    pokes = {0x8C: HANDLER_TRAP3.to_bytes(4, "big")}
    diffs, _ = differential(
        FUN_3A6F4, {"d2": 0, "_pokes": pokes},
        lambda lib, buf: lib.g_fun_3a6f4(buf, 0))
    assert not diffs, report(diffs)


def test_fun_3a556_init_hardware():
    d0 = ctypes.c_uint32(0)
    diffs, info = differential(
        FUN_3A556, {"d2": 0, "_pokes": {}},
        lambda lib, buf: lib.g_fun_3a556(buf, 0))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["d0"]


VOICE = 0x3BF00
RECORD = 0x3BF40


def _voice_pokes(acc=100, modacc=0, step=0, flags=0x01, count=0, reload=0,
                 loop_add=0x08, rec=(2, 5, 3)):
    pokes = {}
    pokes[VOICE + 0] = ((acc & 0xFFFF).to_bytes(2, "big"))
    pokes[VOICE + 2] = ((modacc & 0xFFFF).to_bytes(2, "big"))
    pokes[VOICE + 4] = _b32(RECORD)
    pokes[VOICE + 8] = bytes([loop_add & 0xFF])
    pokes[VOICE + 0xA] = bytes([step & 0xFF])
    pokes[VOICE + 0xB] = bytes([flags & 0xFF])
    pokes[VOICE + 0xC] = bytes([count & 0xFF])
    pokes[VOICE + 0xD] = bytes([reload & 0xFF])
    pokes[RECORD] = bytes([b & 0xFF for b in rec])
    return pokes


def test_fun_3a992_paso_normal():
    pokes = _voice_pokes(acc=100, modacc=0x10, step=1, flags=0x01, count=0, reload=0,
                         loop_add=0x08, rec=(2, 5, 3))
    diffs, info = differential(
        FUN_3A992, {"a0": VOICE, "d0": 1000, "_pokes": pokes},
        lambda lib, buf: lib.g_fun_3a992(buf, VOICE, 1000))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["d0"]


def test_fun_3a992_ciclo_terminado():
    pokes = _voice_pokes(flags=0x04)
    diffs, info = differential(
        FUN_3A992, {"a0": VOICE, "d0": 500, "_pokes": pokes},
        lambda lib, buf: lib.g_fun_3a992(buf, VOICE, 500))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["d0"]


def test_fun_3a992_wrap():
    """step 9 + 3 == 12: cierra envolvente, aplica loop_add a A0[0] y vuelve a step 3."""
    pokes = _voice_pokes(acc=100, step=9, flags=0x01, count=2, reload=0,
                         loop_add=0x08, rec=(1, 5, 3))
    diffs, info = differential(
        FUN_3A992, {"a0": VOICE, "d0": 1000, "_pokes": pokes},
        lambda lib, buf: lib.g_fun_3a992(buf, VOICE, 1000))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["d0"]


_SETUP_CACHE = {}


def _setup_pokes(ticks=0):
    """Imagen post-init (y tras `ticks` ticks del reproductor) como pokes."""
    if ticks not in _SETUP_CACHE:
        img = harness.make_image({})
        merged = {}
        im, w, _ = harness.emu.run(img, FUN_3A556, {"a6": 0x30000},
                                   max_insns=400000)
        merged.update(w)
        for _ in range(ticks):
            im, w, _ = harness.emu.run(im, 0x3A872, {"a6": 0x30000},
                                       stop_pc=0x3A934, max_insns=200000,
                                       psg_seed={7: 0x3F})
            merged.update(w)
        _SETUP_CACHE[ticks] = {a: bytes([v]) for a, v in merged.items()
                               if a < harness.emu.STACK_GUARD_LO}
    return dict(_SETUP_CACHE[ticks])


def test_fun_3aab6_tick_canal0():
    pokes = _setup_pokes(0)
    diffs, _ = differential(
        FUN_3AAB6, {"a3": CH0, "a6": STATE, "d7": 0, "_pokes": pokes},
        lambda lib, buf: lib.g_fun_3aab6(buf, CH0, STATE, 0),
        stop_pc=RTE_3AD58, psg_seed={7: 0x3F})
    assert not diffs, report(diffs)


def test_fun_3aab6_tick_canales12():
    pokes = _setup_pokes(0)
    for d7 in (1, 2):
        diffs, _ = differential(
            FUN_3AAB6, {"a3": CH0 + d7 * 0x50, "a6": STATE, "d7": d7,
                        "_pokes": pokes},
            lambda lib, buf, v=d7: lib.g_fun_3aab6(buf, CH0 + v * 0x50, STATE, v),
            stop_pc=RTE_3AD58)
        assert not diffs, f"canal {d7}\n{report(diffs)}"


def test_fun_3aab6_tick3_canal0():
    pokes = _setup_pokes(3)
    diffs, _ = differential(
        FUN_3AAB6, {"a3": CH0, "a6": STATE, "d7": 0, "_pokes": pokes},
        lambda lib, buf: lib.g_fun_3aab6(buf, CH0, STATE, 0),
        stop_pc=RTE_3AD58)
    assert not diffs, report(diffs)


STREAM = 0x3BF00


def _stream_pokes(data, tick_setup=True):
    pokes = _setup_pokes(0) if tick_setup else {}
    for i in range(4):
        pokes.pop(CH0 + 8 + i, None)
    pokes[CH0 + 8] = _b32(STREAM)
    pokes[STREAM] = bytes(b & 0xFF for b in data)
    return pokes


def test_cmd_3ad5a_transposicion():
    pokes = _stream_pokes([0x12])
    diffs, _ = differential(
        CMD_3AD5A, {"a3": CH0, "a6": STATE, "d7": 0, "_pokes": pokes},
        lambda lib, buf: lib.g_fun_3ad5a(buf, CH0), stop_pc=CONT_3AB3A)
    assert not diffs, report(diffs)


def test_cmd_3ad6a_instrumento_voz2():
    pokes = _stream_pokes([0x03])
    diffs, _ = differential(
        CMD_3AD6A, {"a3": CH0, "a6": STATE, "d7": 0, "_pokes": pokes},
        lambda lib, buf: lib.g_fun_3ad6a(buf, CH0, STATE), stop_pc=CONT_3AB3A)
    assert not diffs, report(diffs)


def test_cmd_3adb4_instrumento_voz2_loop():
    pokes = _stream_pokes([0x05])
    diffs, _ = differential(
        CMD_3ADB4, {"a3": CH0, "a6": STATE, "d7": 0, "_pokes": pokes},
        lambda lib, buf: lib.g_fun_3adb4(buf, CH0, STATE), stop_pc=CONT_3AB3A)
    assert not diffs, report(diffs)


def test_cmd_3ae00_envolvente():
    pokes = _stream_pokes([0x3F, 0x12])
    diffs, _ = differential(
        CMD_3AE00, {"a3": CH0, "a6": STATE, "d7": 0, "_pokes": pokes},
        lambda lib, buf: lib.g_fun_3ae00(buf, CH0, 0), stop_pc=CONT_3AB3A)
    assert not diffs, report(diffs)


def test_cmd_3ae68_volumen_directo():
    pokes = _stream_pokes([0x05])
    diffs, _ = differential(
        CMD_3AE68, {"a3": CH0, "a6": STATE, "d7": 0, "_pokes": pokes},
        lambda lib, buf: lib.g_fun_3ae68(buf, CH0), stop_pc=0x3AE7A)
    assert not diffs, report(diffs)
