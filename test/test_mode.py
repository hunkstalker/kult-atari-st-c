"""Tests del modo de escena y lista de objetos (mode.c).

FUN_0001442c @ 0x1442c (cmd idx 74): guarda 0x1acc7, corre 0x1443e y lo restaura.
FUN_0001443e @ 0x1443e: fija 0x1acc6 desde A6[1], escribe el marcador en 0x1a695,
  camina la tabla de modos (0x14496) y reinicia el estado (0x14588).
FUN_00014496 @ 0x14496: selecciona la entrada 0x1acc6 de la tabla 0x1ad38, publica
  su cabecera de 5 bytes y empaqueta su lista de objetos en 0x1ab62 (5 words/objeto).
FUN_00014588 @ 0x14588: reinicia los globals de comandos (words 0x1ab5x + bytes).
"""
import ctypes

import harness
from harness import differential, report

_u8p = ctypes.POINTER(ctypes.c_uint8)
_u32p = ctypes.POINTER(ctypes.c_uint32)
MODE_TABLE = 0x1AD38
MODE = 0x1ACC6


def _put_entry(pokes, addr, header, packed=b"", tail=b""):
    """Escribe una entrada [len][header][packed][tail] y devuelve la direccion siguiente."""
    body = bytes(header) + packed + tail
    pokes[addr] = bytes([len(body) + 1]) + body
    return addr + len(body) + 1


def _table_for(mode, header, packed=b"", tail=b""):
    """Tabla 0x1ad38 con la entrada `mode` (1-based) al final; las previas son de 1 byte."""
    pokes = {}
    a = MODE_TABLE
    for _ in range(mode - 1):
        a = _put_entry(pokes, a, [])
    _put_entry(pokes, a, header, packed, tail)
    return pokes


harness._lib.cmd74_wrap.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.cmd74_wrap.restype = ctypes.c_uint32
harness._lib.scene_mode_set.argtypes = [_u8p, ctypes.c_uint32, _u32p, _u32p]
harness._lib.scene_mode_set.restype = ctypes.c_uint32
harness._lib.scene_mode_load.argtypes = [_u8p, _u32p, _u32p]
harness._lib.scene_mode_load.restype = None
harness._lib.command_state_reset.argtypes = [_u8p]
harness._lib.command_state_reset.restype = None


def test_fun_14588():
    """Los 9 words y 9 bytes del reinicio, con basura previa en todos."""
    words = (0x1ABFA, 0x1AB5C, 0x1AB5E, 0x1AB56, 0x1AB50, 0x1AB52, 0x1AB54,
             0x1AB58, 0x1AB5A)
    pokes = {t: b"\xab\xcd" for t in words}
    for i in range(9):
        pokes[0x1AD09 + i] = bytes([0x7E])
    for t in (0x1AD27, 0x1ACD1, 0x1ACF9, 0x1ACFA, 0x1ACCE, 0x1ACFF):
        pokes[t] = bytes([0x7E])
    diffs, _ = differential(0x14588, {"_pokes": pokes},
                            lambda lib, buf: lib.command_state_reset(buf))
    assert not diffs, report(diffs)


def _obj_test_pokes(mode, count, packed):
    pokes = {0xD8EB: bytes([0x52]), MODE: bytes([mode])}
    a = MODE_TABLE
    for _ in range(mode - 1):
        a = _put_entry(pokes, a, [])
    _put_entry(pokes, a, [0x30, 0x31, 0x32, 0x33, count], packed, bytes(range(16)))
    for i in range(0x96):
        pokes[0x1AB62 + i] = bytes([(i * 7 + 3) & 0xFF])
    for t, v in ((0x1A7CA, 0x11111111), (0x1A762, 0x22222222),
                 (0x1A7CE, 0x33333333), (0x1A7D2, 0x44444444)):
        pokes[t] = v.to_bytes(4, "big")
    for t in (0x1AB4A, 0x1ACD3, 0x1ACEB, 0x1ACE7):
        pokes[t] = bytes([0x99])
    for t in (0x1ABF8, 0x1ACAE, 0x1ACAA):
        pokes[t] = b"\x99\x99"
    return pokes


def test_fun_14496_sin_objetos():
    """count = 0: no toca 0x1ab62 y A5 = fin de la cabecera (== inicio de la lista)."""
    pokes = _obj_test_pokes(1, 0x00, b"")
    a4 = ctypes.c_uint32()
    a5 = ctypes.c_uint32()
    diffs, info = differential(
        0x14496, {"_pokes": pokes},
        lambda lib, buf: lib.scene_mode_load(buf, ctypes.byref(a4), ctypes.byref(a5)))
    assert not diffs, report(diffs)
    assert a4.value == info["regs"]["a4"]
    assert a5.value == info["regs"]["a5"]


def test_fun_14496_con_objetos():
    """count = 2: empaqueta 3+1 words segun flags 0x16/0x01 y limpia 0x1ab62."""
    packed = bytes([
        0x00, 0x16, 0xAA, 0x01, 0xBB, 0x02, 0xCC, 0x03,   # bits 4,2,1 -> +0,+4,+6
        0x00, 0x01, 0xDD, 0x04,                           # bit 0 -> +8
    ])
    pokes = _obj_test_pokes(3, 0x02, packed)
    a4 = ctypes.c_uint32()
    a5 = ctypes.c_uint32()
    diffs, info = differential(
        0x14496, {"_pokes": pokes},
        lambda lib, buf: lib.scene_mode_load(buf, ctypes.byref(a4), ctypes.byref(a5)))
    assert not diffs, report(diffs)
    assert a4.value == info["regs"]["a4"]
    assert a5.value == info["regs"]["a5"]


def test_fun_1443e():
    """Marcadores 0xe5/0xdd/0xde y rama sin marcador, con A6 += 2 y A4/A5 de salida."""
    for mode, ad32 in ((0x03, 0), (0x05, 0), (0x81, 0), (0x82, 0), (0x03, 1)):
        pokes = _table_for(mode, [0x30, 0x31, 0x32, 0x33, 0x00], b"", bytes(range(4)))
        pokes[0xD8EB] = bytes([0x52])
        pokes[MODE] = bytes([0xA5])
        pokes[0x1AD32] = bytes([ad32])
        pokes[0x30000] = bytes([0x00, mode])
        for i in range(8):
            pokes[0x1A695 + i] = bytes([0x5A ^ i])
        a4 = ctypes.c_uint32()
        a5 = ctypes.c_uint32()
        diffs, info = differential(
            0x1443E, {"a6": 0x30000, "_pokes": pokes},
            lambda lib, buf: lib.scene_mode_set(buf, 0x30000, ctypes.byref(a4),
                                             ctypes.byref(a5)))
        assert not diffs, f"mode={mode:#x} ad32={ad32}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], f"mode={mode:#x}"
        assert a4.value == info["regs"]["a4"], f"mode={mode:#x}"
        assert a5.value == info["regs"]["a5"], f"mode={mode:#x}"


def test_fun_1442c():
    """El wrapper restaura 0x1acc7 que 0x14496 acaba de pisar con la cabecera."""
    pokes = _table_for(0x03, [0x30, 0x31, 0x32, 0x33, 0x00], b"", bytes(range(4)))
    pokes[0xD8EB] = bytes([0x52])
    pokes[MODE] = bytes([0xA5])
    pokes[0x1ACC7] = bytes([0x77])
    pokes[0x30000] = bytes([0x00, 0x03])
    diffs, info = differential(
        0x1442C, {"a6": 0x30000, "_pokes": pokes},
        lambda lib, buf: lib.cmd74_wrap(buf, 0x30000))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


# ================= cluster 0x14602/0x1460a/0x146d0 =================

FUN_1460A = 0x1460A
FUN_146D0 = 0x146D0
FUN_14602 = 0x14602
harness._lib.scene_mode_apply.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.scene_mode_apply.restype = ctypes.c_uint32
harness._lib.scene_mode_draw.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.scene_mode_draw.restype = ctypes.c_uint32
harness._lib.scene_mode_draw_wrap.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.scene_mode_draw_wrap.restype = None

VBL_SITE = 0x1359A
VBL_COUNTER = 0x1ACD4


def _vbl_k(n):
    return [{"pc": VBL_SITE, "nth": k, "addr": VBL_COUNTER, "width": 1, "value": k}
            for k in range(1, n + 1)]


def _obj8(x0, x1, y0, y1, flags):
    return bytes([x0, x1, y0, y1, flags, 0, 0, 0])


def _f1460a_pokes(mode, d0f, tails=(), ad25=0, a570=1):
    pokes = _table_for(mode, [0x30, 0x01, 0x32, 0x33, 0x00], b"", b"".join(tails))
    pokes[0xD8EB] = bytes([0x52])
    pokes[0x30000] = bytes([0x00, mode])          # a6[1] = modo
    pokes[0x1A570] = bytes([a570])
    pokes[0x1AD0F] = bytes([d0f])
    pokes[0x1AD25] = bytes([ad25])
    pokes[0x1ACFC] = bytes([0x00])
    pokes[0x1ACC9] = bytes([0x00])
    pokes[0x1AD1D] = bytes([0x00])
    pokes[0x1A45E] = (0x90000).to_bytes(4, "big")
    pokes[0x1A466] = (0x40000).to_bytes(4, "big")
    for i in range(0x100):
        pokes[0x1A776 + i] = bytes([0x00])
    pokes.update(_chain_13f86())
    return pokes


def _chain_13f86():
    pokes = {
        0x24982: bytes([7]),
        0x41FB0: (0x12).to_bytes(2, "big"),
        0x41FB2: bytes([2, 4]),
        0x1ACF1: bytes([5]), 0x1ACED: bytes([0]),
        0x1A462: (0x88000).to_bytes(4, "big"),
        0x1A573: bytes([0]),
    }
    pos = 0x24983
    for b in ((0, 0x10, 3), (0, 0x11, 5)):
        pokes[pos] = bytes(b)
        pos += 3
    for i in range(0x20):
        pokes[0x41FB4 + i] = bytes([(0x10 + i) & 0xFF])
    for i in range(0x8000):
        pokes[0x88000 + i] = bytes([(i * 11 + 7) & 0xFF])
    return pokes


def test_fun_1460a_busqueda():
    tails = (_obj8(0x10, 0x20, 0x30, 0x40, 0x88), _obj8(0x11, 0x21, 0x31, 0x41, 0x8b))
    for a570, d0f in ((1, 0x01), (0, 0x03), (0, 0x11), (0, 0x00)):
        pokes = _f1460a_pokes(0x87, d0f, tails, a570=a570)
        diffs, info = differential(
            FUN_1460A, {"a6": 0x30000, "_pokes": pokes},
            lambda lib, buf: lib.scene_mode_apply(buf, 0x30000))
        assert not diffs, f"a570={a570} d0f={d0f:#x}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"]


def test_fun_1460a_sin_objeto():
    tails = (_obj8(0x10, 0x20, 0x30, 0x40, 0x00),)
    for d0f in (0x03, 0x11):
        pokes = _f1460a_pokes(0x87, d0f, tails, a570=0)
        diffs, info = differential(
            FUN_1460A, {"a6": 0x30000, "_pokes": pokes},
            lambda lib, buf: lib.scene_mode_apply(buf, 0x30000))
        assert not diffs, f"d0f={d0f:#x}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"]


def test_fun_1460a_texto_pendiente():
    tails = (_obj8(0x10, 0x20, 0x30, 0x40, 0x88),)
    pokes = _f1460a_pokes(0x87, 0x01, tails, ad25=1)
    pokes[0x24982] = bytes([4, 0, 1, 0])
    pokes[0x41FB0] = (2).to_bytes(2, "big")
    pokes[0x41FB2] = bytes([2, 2])
    for i in range(0x40):
        pokes[0x41FB4 + i] = bytes([(0x10 + i) & 0xFF])
    pokes[0x90000] = bytes([0x5A]) * 0x10
    diffs, info = differential(
        FUN_1460A, {"a6": 0x30000, "_pokes": pokes},
        lambda lib, buf: lib.scene_mode_apply(buf, 0x30000))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


def test_fun_1460a_formulario():
    """H-M1 (cobertura): (0x1ad0f & 0x38)==8 corre 0x141fa (formulario hacia
    delante) y pone $1a572=1; 0x1ad0f&7=0 no lanza la busqueda de objeto.
    Se compara con la rama (0x38)==0 del mismo fixture."""
    TABLE = 0x1A7B6
    ENTRIES = 0x41FB0
    RECORD = 0x50000
    SCREEN = 0x90000
    SRC = 0x60000
    for d0f, waits in ((0x08, 3), (0x00, 0)):
        pokes = _f1460a_pokes(0x87, d0f, (_obj8(0x10, 0x20, 0x30, 0x40, 0x88),),
                              a570=0)
        pokes[0x1ACC9] = bytes([1])
        pokes[TABLE] = RECORD.to_bytes(4, "big")
        for i in range(6):
            pokes[ENTRIES + i * 6] = (6).to_bytes(2, "big")
            pokes[ENTRIES + i * 6 + 2] = bytes([2])
            pokes[ENTRIES + i * 6 + 3] = bytes([4])
        for i, (idx, x, y) in enumerate(((0, 2, 1), (1, 5, 1), (2, 8, 1))):
            pokes[RECORD + i * 3] = bytes([idx])
            pokes[RECORD + i * 3 + 1] = bytes([x])
            pokes[RECORD + i * 3 + 2] = bytes([y])
        for i in range(0x8000):
            pokes[SCREEN - 0x8000 + i] = bytes([(i * 5 + 1) & 0xFF])
            pokes[SCREEN + i] = bytes([(i * 3 + 1) & 0xFF])
            pokes[SRC + i] = bytes([(i * 7 + 3) & 0xFF])
        diffs, info = differential(
            FUN_1460A, {"a6": 0x30000, "_pokes": pokes},
            lambda lib, buf: lib.scene_mode_apply(buf, 0x30000),
            schedule=_vbl_k(waits) if waits else [],
            wait_sites=[VBL_SITE] if waits else None,
            max_insns=3_000_000)
        assert not diffs, f"d0f={d0f:#x}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"]


def _body_pokes(cmd=0x00, sel=0, ad1d=0, a572=0):
    pokes = {
        0x1A572: bytes([a572]), 0x1A570: bytes([0x00]),
        0x1ACD2: bytes([cmd]), 0x1ACC6: bytes([0x00]),
        0x1ACC9: bytes([sel]), 0x1AD1D: bytes([ad1d]),
        0x1A45E: (0x80000).to_bytes(4, "big"),
        0x1A462: (0x88000).to_bytes(4, "big"),
        0x1A466: (0x40000).to_bytes(4, "big"),
        0x1ACF4: bytes([0x00]),
        0x1A573: bytes([0x00]), 0x1ACD4: bytes([0x00]),
        0x1AB4F: bytes([0x00]),
    }
    for i in range(0x100):
        pokes[0x1A776 + i] = bytes([0x00])
        pokes[0x1A850 + i] = bytes([0x00])
    pokes[0x1A57C] = bytes([0xFF])
    base = 0x55B0A
    for _ in range(3):
        pokes[base] = bytes([0x00, 0x02]); base += 2
    pokes[base] = bytes([0x00, 0x08, 0x03, 0x55, 0x11, 0x22, 0x33, 0x44])
    pokes[0x1A4E8] = (0x55B0A).to_bytes(4, "big")
    for i in range(0x4000):
        pokes[0x80000 + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x88000 + i] = bytes([(i * 11 + 7) & 0xFF])
    return pokes


def test_fun_146d0_cuerpo():
    for cmd, a572 in ((0x00, 0), (0x16, 0), (0x00, 1)):
        pokes = _body_pokes(cmd=cmd, a572=a572)
        diffs, info = differential(
            FUN_146D0, {"a6": 0x30000, "_pokes": pokes},
            lambda lib, buf: lib.scene_mode_draw(buf, 0x30000),
            schedule=_vbl_k(0x35) if a572 == 0 else [],
            wait_sites=[VBL_SITE] if a572 == 0 else None)
        assert not diffs, f"cmd={cmd:#x} a572={a572}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"]


def test_fun_14602_completo():
    tails = (_obj8(0x10, 0x20, 0x30, 0x40, 0x88),)
    pokes = _f1460a_pokes(0x87, 0x01, tails, a570=1)
    pokes.update(_body_pokes(cmd=0x30))
    pokes[0x1A572] = bytes([0x00])
    pokes[0x1A570] = bytes([0x01])
    diffs, _ = differential(
        FUN_14602, {"a6": 0x30000, "_pokes": pokes},
        lambda lib, buf: lib.scene_mode_draw_wrap(buf, 0x30000))
    assert not diffs, report(diffs)
