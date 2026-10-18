"""Test diferencial de ym_write (FUN_0001929a @ 0x1929a).

Selecciona el registro D0 y escribe D1; para el mixer (registro 7) hace un
read-modify-write leyendo el puerto de datos $ff8802, que en hardware real es bus
abierto (0xFF) — el kit lo sirve como OS_PSG_DATA_READ. El ledger PSG del candidato
se compara automaticamente contra el del oraculo.
"""
import ctypes

import harness
from harness import differential, report

YM_WRITE = 0x1929A

_u8p = ctypes.POINTER(ctypes.c_uint8)
harness._lib.ym_write.argtypes = [_u8p, ctypes.c_uint32, ctypes.c_uint32,
                                    ctypes.POINTER(ctypes.c_uint32),
                                    ctypes.POINTER(ctypes.c_uint32)]
harness._lib.ym_write.restype = None


def test_ym_write():
    # (registro, valor): el 7 pasa por el RMW del mixer; el resto es escritura directa.
    for reg, val in ((7, 0x3F), (7, 0x18), (7, 0x00), (7, 0x80),
                     (0, 0x11), (0x0D, 0x00), (8, 0x0F), (6, 0x1F)):
        d0 = ctypes.c_uint32(0xDEADBEEF)
        d1 = ctypes.c_uint32(0xDEADBEEF)
        diffs, info = differential(
            YM_WRITE, {"d0": reg, "d1": val, "_pokes": {}},
            lambda lib, buf, r=reg, v=val: lib.ym_write(buf, r, v,
                                                          ctypes.byref(d0),
                                                          ctypes.byref(d1)))
        assert not diffs, f"reg={reg} val={val:#x}\n{report(diffs)}"
        assert d0.value == info["regs"]["d0"], f"reg={reg} val={val:#x} d0"
        assert d1.value == info["regs"]["d1"], f"reg={reg} val={val:#x} d1"


FUN_19282 = 0x19282
harness._lib.func_19282.argtypes = [_u8p]
harness._lib.func_19282.restype = None


def test_fun_19282():
    """Reset PSG: registros 0xd..0 a 0 y mixer (7) a 0x18 (con su RMW)."""
    diffs, _ = differential(FUN_19282, {"_pokes": {}},
                            lambda lib, buf: lib.func_19282(buf))
    assert not diffs, report(diffs)


FUN_18FC4 = 0x18FC4
FUN_18FC6 = 0x18FC6
FUN_18F7A = 0x18F7A
FUN_18F84 = 0x18F84
FUN_18F8E = 0x18F8E
FUN_18F98 = 0x18F98
FUN_18FA2 = 0x18FA2
FUN_18FA4 = 0x18FA4
FUN_18F6E = 0x18F6E
FUN_1906A = 0x1906A
FUN_19084 = 0x19084
FUN_1909E = 0x1909E
for _n, _nargs in (("sound_timer_start", 2), ("func_18fc6", 3), ("func_18f7a", 3),
                   ("func_18f84", 1), ("func_18f8e", 2), ("func_18f98", 2),
                   ("func_1906a", 2), ("sound_timer_stop", 1), ("func_1909e", 1)):
    getattr(harness._lib, _n).argtypes = [_u8p] + [ctypes.c_uint32] * (_nargs - 1)
    getattr(harness._lib, _n).restype = None
harness._lib.func_18fa2.argtypes = [_u8p, ctypes.POINTER(ctypes.c_uint32)]
harness._lib.func_18fa2.restype = None
harness._lib.func_18fa4.argtypes = [_u8p]
harness._lib.func_18fa4.restype = None
harness._lib.func_18f6e.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.func_18f6e.restype = ctypes.c_uint32

# Banco sintetico de samples: entrada [len=8][cnt=3][tadr][datos...]
BANK = 0x55B0A


def _bank_pokes():
    pokes = {}
    pokes[BANK] = bytes([0x00, 0x08, 0x03, 0x55, 0x11, 0x22, 0x33, 0x44])
    return pokes


def test_fun_18fc4():
    """Timer A: banco por defecto (D4 bit7=0), D1 = 0 (no pisa la recarga)."""
    for d4 in (0, 5):
        pokes = _bank_pokes()
        if d4 == 5:  # cinco entradas vacias antes de la elegida
            base = BANK
            for _ in range(5):
                pokes[base] = bytes([0x00, 0x02]); base += 2
            pokes[base] = bytes([0x00, 0x08, 0x03, 0x55, 0x11, 0x22, 0x33, 0x44])
        diffs, _ = differential(
            FUN_18FC4, {"d4": d4, "_pokes": pokes},
            lambda lib, buf, d=d4: lib.sound_timer_start(buf, d))
        assert not diffs, f"d4={d4}\n{report(diffs)}"


def test_fun_18fc6():
    """Entrada con D1: si != 0 pisa el byte de recarga del TADR."""
    for d1 in (0, 0x77):
        pokes = _bank_pokes()
        diffs, _ = differential(
            FUN_18FC6, {"d1": d1, "d4": 0, "_pokes": pokes},
            lambda lib, buf, v=d1: lib.func_18fc6(buf, v, 0))
        assert not diffs, f"d1={d1}\n{report(diffs)}"


def test_fun_18fc4_bit7():
    """D4 bit7: banco por la tabla 0x1a4e8; con 0x1a57c = 0xff no carga recurso."""
    pokes = _bank_pokes()
    pokes[0x1A57C] = bytes([0xFF])
    pokes[0x1A4E8] = BANK.to_bytes(4, "big")
    diffs, _ = differential(
        FUN_18FC4, {"d4": 0x80, "_pokes": pokes},
        lambda lib, buf: lib.sound_timer_start(buf, 0x80))
    assert not diffs, report(diffs)


def test_fun_18f7a():
    """Envoltorio Supexec -> 0x18fc6 con D1/D4."""
    pokes = _bank_pokes()
    pokes[0x1AB4F] = bytes([0x00])
    for d1, d4 in ((0, 0), (0x77, 0), (0x11, 0x05)):
        p = dict(pokes)
        if d4 == 5:
            base = BANK
            for _ in range(5):
                p[base] = bytes([0x00, 0x02]); base += 2
            p[base] = bytes([0x00, 0x08, 0x03, 0x55, 0x11, 0x22, 0x33, 0x44])
        diffs, _ = differential(
            FUN_18F7A, {"d1": d1, "d4": d4, "_pokes": p},
            lambda lib, buf, a=d1, b=d4: lib.func_18f7a(buf, a, b))
        assert not diffs, f"d1={d1} d4={d4}\n{report(diffs)}"


def test_fun_18f8e():
    """Envoltorio Supexec -> 0x18fc4 (entrada canonica)."""
    pokes = _bank_pokes()
    pokes[0x1AB4F] = bytes([0x00])
    diffs, _ = differential(
        FUN_18F8E, {"d4": 0, "_pokes": pokes},
        lambda lib, buf: lib.func_18f8e(buf, 0))
    assert not diffs, report(diffs)


def test_fun_18f98():
    """Envoltorio Supexec -> 0x1906a (Timer A + vector 0x190f6)."""
    pokes = _bank_pokes()
    pokes[0x1AB4F] = bytes([0x00])
    diffs, _ = differential(
        FUN_18F98, {"d4": 0, "_pokes": pokes},
        lambda lib, buf: lib.func_18f98(buf, 0))
    assert not diffs, report(diffs)


def test_fun_1906a():
    """Arranque con vector 0x190f6 y copia de 0x192ca."""
    pokes = _bank_pokes()
    diffs, _ = differential(
        FUN_1906A, {"d4": 0, "_pokes": pokes},
        lambda lib, buf: lib.func_1906a(buf, 0))
    assert not diffs, report(diffs)


def test_fun_19084():
    """Para el Timer A (MFP) y limpia 0x192d0."""
    diffs, _ = differential(FUN_19084, {"_pokes": {}},
                            lambda lib, buf: lib.sound_timer_stop(buf))
    assert not diffs, report(diffs)


def test_fun_18fa2_18fa4():
    """Envoltorios Supexec -> 0x19084; 0x18fa2 avanza A6."""
    pokes = {0x1AB4F: bytes([0x00])}
    diffs, _ = differential(FUN_18FA4, {"_pokes": dict(pokes)},
                            lambda lib, buf: lib.func_18fa4(buf))
    assert not diffs, report(diffs)
    a6 = ctypes.c_uint32(0x50000)
    diffs, info = differential(
        FUN_18FA2, {"a6": 0x50000, "_pokes": dict(pokes)},
        lambda lib, buf: lib.func_18fa2(buf, ctypes.byref(a6)))
    assert not diffs, report(diffs)
    assert a6.value == info["regs"]["a6"]


def test_fun_18f6e():
    """Handler de byte: D4/D1 de A6[1]/A6[2], A6 += 3 y Supexec a 0x18fc6."""
    pokes = _bank_pokes()
    pokes[0x1AB4F] = bytes([0x00])
    pokes[0x50000] = bytes([0x00, 0x00, 0x77])
    diffs, info = differential(
        FUN_18F6E, {"a6": 0x50000, "_pokes": pokes},
        lambda lib, buf: lib.func_18f6e(buf, 0x50000))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


def test_fun_18f84_1909e():
    """Envoltorio Supexec -> 0x1909e (apagado: PSG + Timer A)."""
    pokes = {0x1AB4F: bytes([0x00])}
    diffs, _ = differential(FUN_18F84, {"_pokes": dict(pokes)},
                            lambda lib, buf: lib.func_18f84(buf))
    assert not diffs, report(diffs)
    diffs, _ = differential(FUN_1909E, {"_pokes": {}},
                            lambda lib, buf: lib.func_1909e(buf))
    assert not diffs, report(diffs)


FUN_190B8 = 0x190B8
FUN_190F6 = 0x190F6
FUN_1910E = 0x1910E
for _n in ("func_190b8", "func_190f6", "func_1910e"):
    getattr(harness._lib, _n).argtypes = [_u8p]
    getattr(harness._lib, _n).restype = None

RTE_PC = 0x19144
SAMPLE_BASE = 0x19310
LIST = 0x19400


def _isr_pokes(cur, end):
    pokes = {
        0x192C6: cur.to_bytes(4, "big"),
        0x192CA: end.to_bytes(4, "big"),
        0x192C2: SAMPLE_BASE.to_bytes(4, "big"),
        0x192CE: bytes([0x00]),
        0x192CF: bytes([0x00]),
        0x192D0: bytes([0x00]),
    }
    for i in range(8):
        pokes[SAMPLE_BASE + i] = bytes([(i * 4) & 0xFC])
    return pokes


def test_fun_1910e():
    """ISR PCM hacia delante: lee un byte, escribe 2 registros PSG y bclr+rte."""
    pokes = _isr_pokes(SAMPLE_BASE + 1, SAMPLE_BASE + 8)
    diffs, _ = differential(FUN_1910E, {"_pokes": pokes},
                            lambda lib, buf: lib.func_1910e(buf), stop_pc=RTE_PC)
    assert not diffs, report(diffs)


def test_fun_190f6():
    """ISR PCM hacia atras (variante 0x1906a)."""
    pokes = _isr_pokes(SAMPLE_BASE + 4, SAMPLE_BASE + 8)
    diffs, _ = differential(FUN_190F6, {"_pokes": pokes},
                            lambda lib, buf: lib.func_190f6(buf), stop_pc=RTE_PC)
    assert not diffs, report(diffs)


def _isr_list_pokes(next_ptr):
    pokes = _isr_pokes(next_ptr, SAMPLE_BASE)
    pokes[0x192BE] = (0x00010000).to_bytes(4, "big")
    pokes[0x1930E] = LIST.to_bytes(4, "big")
    # [word != 0][TADR byte][word ptr1][word ptr2]
    pokes[LIST] = bytes([0x00, 0x55, 0x10, 0x00, 0x20, 0x00])
    return pokes


def test_fun_190b8_bloque():
    """Carga un bloque de la lista de samples y actualiza los punteros."""
    pokes = _isr_list_pokes(SAMPLE_BASE)
    diffs, _ = differential(FUN_190B8, {"_pokes": pokes},
                            lambda lib, buf: lib.func_190b8(buf), stop_pc=RTE_PC)
    assert not diffs, report(diffs)


def test_fun_1910e_repeticion():
    """Sin datos: repite (rebobina) y decrementa el TADR."""
    pokes = _isr_list_pokes(SAMPLE_BASE)
    pokes[0x192CE] = bytes([0x01])
    pokes[0x192CF] = bytes([0x01])
    pokes[0x192D0] = bytes([0x01])
    diffs, _ = differential(FUN_1910E, {"_pokes": pokes},
                            lambda lib, buf: lib.func_1910e(buf), stop_pc=RTE_PC)
    assert not diffs, report(diffs)


def test_fun_1910e_parada():
    """Sin datos y sin repeticion: para el Timer A (0x19084)."""
    pokes = _isr_pokes(SAMPLE_BASE + 8, SAMPLE_BASE + 8)
    diffs, _ = differential(FUN_1910E, {"_pokes": pokes},
                            lambda lib, buf: lib.func_1910e(buf), stop_pc=RTE_PC)
    assert not diffs, report(diffs)
