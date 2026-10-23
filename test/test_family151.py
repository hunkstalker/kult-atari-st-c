"""Tests diferenciales de la familia 0x151b0 sin esperas: 0x15052, 0x15382,
0x15428 y 0x15530.

Todas arrancan con el umbral 0x14f24 (que puede saltar al llamante), fijan
`the_command` ($1a50e) y despachan por 0x103ae. Las rutas con 0x1680c esperan clic
(schedule IKBD/VBL como en el menu) y 0x15428 imprime un nombre y espera Cnecin.
"""
import ctypes

import harness
from harness import console_key, differential, report

_u8p = ctypes.POINTER(ctypes.c_uint8)

FUN_15052 = 0x15052
FUN_15382 = 0x15382
FUN_15530 = 0x15530
SCRIPT = 0x52000
VBL_SITE = 0x1359A
VBL_COUNTER = 0x1ACD4
IKBD_SITE = 0x136B0
IKBD_PACKET = 0x1A44F

harness._lib.obj_action_select.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.obj_action_select.restype = ctypes.c_uint32
harness._lib.palette_cmd.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.palette_cmd.restype = ctypes.c_uint32
harness._lib.mode_queue.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.mode_queue.restype = ctypes.c_uint32


def _base_pokes(umbral=0x15, selec=0):
    pokes = {
        0x1AD09: bytes([0x00]),      # sin el modo que corta
        0x1ACC9: bytes([selec]),
        0x1ACEC: bytes([0x10]),
        0x1ACD2: bytes([0x05]),
        0x1ACE9: bytes([0x10]),
        0x1AD20: bytes([umbral]),
        0x1AB52: (0).to_bytes(2, "big"),
        0x1AB58: (0).to_bytes(2, "big"),
        0x1AB5A: (0).to_bytes(2, "big"),
        0x1AB5E: (0).to_bytes(2, "big"),
        0x1ABF8: (0).to_bytes(2, "big"),
        0x1ABFE: (0).to_bytes(2, "big"),
        0x1AC00: (0).to_bytes(2, "big"),
        0x1AC02: (0).to_bytes(2, "big"),
        0x1ACAA: (0).to_bytes(2, "big"),
        0x1ACAE: (0).to_bytes(2, "big"),
        0x1ACF2: bytes([0x07]),
        0x1AD0E: bytes([0x00]),
        0x1A456: bytes([0x00]),
        0x1A44F: bytes([0xFF]),
        0x1ACD4: bytes([0x00]),
        0x1AB46: bytes([0x00]),
        0x1AB47: bytes([0x00]),
        0x1AB40: bytes([0x00]),
        0x1AB41: bytes([0x00]),
        0x1AB42: bytes([0x00]),
        0x1AB43: bytes([0x00]),
        0x1A466: (0).to_bytes(4, "big"),     # sin paleta activa
        0x1A45E: (0x80000).to_bytes(4, "big"),
        0x1A462: (0x88000).to_bytes(4, "big"),
        SCRIPT: bytes([2, 0x00]),
    }
    for i in range(0x70):                # bancos de overlay A/B
        pokes[0x41586 + i * 3] = bytes([3, 0x21 + (i % 0x40), 0x22])
        pokes[0x41B76 + i * 3] = bytes([3, 0x21 + (i % 0x40), 0x22])
    return pokes


def _click_schedule():
    return [
        {"pc": VBL_SITE, "nth": 1, "addr": VBL_COUNTER, "width": 1, "value": 1},
        {"pc": VBL_SITE, "nth": 2, "addr": VBL_COUNTER, "width": 1, "value": 2},
        {"pc": IKBD_SITE, "nth": 1, "addr": IKBD_PACKET, "width": 1, "value": 0x00},
        {"pc": IKBD_SITE, "nth": 2, "addr": IKBD_PACKET, "width": 1, "value": 0x01},
    ]


def test_fun_15052_directo():
    """$1ad09 != 0: despacha 0x1abfe sin pasar por el hit-test (sin esperas)."""
    pokes = _base_pokes()
    pokes[0x1AD09] = bytes([0x01])
    diffs, info = differential(
        FUN_15052, {"a6": SCRIPT, "_pokes": pokes},
        lambda lib, buf: lib.obj_action_select(buf, SCRIPT))
    assert not diffs, report(diffs)
    assert info["ret"] == SCRIPT


def test_fun_15052_tabla():
    """$1acc9 con seleccion: toma el comando de la tabla 0x1ab62 (el bucle 0x1680c
    con clic queda pendiente)."""
    pokes = _base_pokes(selec=1)
    pokes[0x1AD09] = bytes([0x01])
    diffs, info = differential(
        FUN_15052, {"a6": SCRIPT, "_pokes": pokes},
        lambda lib, buf: lib.obj_action_select(buf, SCRIPT))
    assert not diffs, report(diffs)
    assert info["ret"] == SCRIPT


def test_fun_15382_directo():
    pokes = _base_pokes(umbral=0x1A)
    pokes[0x1AD09] = bytes([0x01])
    diffs, info = differential(
        FUN_15382, {"a6": SCRIPT, "_pokes": pokes},
        lambda lib, buf: lib.palette_cmd(buf, SCRIPT))
    assert not diffs, report(diffs)
    assert info["ret"] == SCRIPT


def test_fun_15530_basico():
    pokes = _base_pokes()
    pokes[0x1ACE7] = bytes([0x00])
    pokes[0x1ABF8] = (1).to_bytes(2, "big")
    diffs, info = differential(
        FUN_15530, {"a6": SCRIPT, "_pokes": pokes},
        lambda lib, buf: lib.mode_queue(buf, SCRIPT))
    assert not diffs, report(diffs)
    assert info["ret"] == SCRIPT


# ================= accion de objeto 0x150ae =================

FUN_150AE = 0x150AE
A81A_OBJ = 0x40100
OBJ_LIST = 0x40000

harness._lib.obj_action.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.obj_action.restype = ctypes.c_uint32


def _ae_wait_pokes():
    """Estado de la espera 0x1680c (mismo que test_objwait) mas el umbral."""
    pokes = {
        IKBD_PACKET: bytes([0xFF]),
        0x1A456: bytes([0x00]),
        0x1A570: bytes([0x00]),
        0x1AB4F: bytes([0x00]),      # handle_frame_input no repite la espera de VBL
        VBL_COUNTER: bytes([0x10]),
        0x1AB44: bytes([0x75]), 0x1AB45: bytes([0x75]),
        0x1AB46: bytes([0x64]), 0x1AB47: bytes([0x64]),
        0x1AB48: bytes([0x75]),
        0x1A772: (0xDEADBEEF).to_bytes(4, "big"),
        0x1A7CA: OBJ_LIST.to_bytes(4, "big"),
        0x1A7CE: (OBJ_LIST + 8).to_bytes(4, "big"),
        0x1AD20: bytes([0x15]),
        0x1AD09: bytes([0x00]),
        0x1ACC9: bytes([0x00]),
        0x1ACC7: bytes([0x00]),
        0x1AB5C: bytes([0x00, 0x00]),
    }
    pokes[OBJ_LIST] = bytes(8)       # rect nulo: el cursor nunca acierta
    return pokes


def _wait_schedule():
    return [
        {"pc": IKBD_SITE, "nth": 1, "addr": IKBD_PACKET, "width": 1, "value": 0x01},
        {"pc": IKBD_SITE, "nth": 2, "addr": IKBD_PACKET, "width": 1, "value": 0x01},
        {"pc": VBL_SITE, "nth": 1, "addr": VBL_COUNTER, "width": 1, "value": 0x11},
    ]


def test_fun_150ae_rts_12():
    """$1ad09 == 0x12: sale por rts sin esperas ni despacho."""
    pokes = {0x1AD20: bytes([0x15]), 0x1AD09: bytes([0x12])}
    diffs, info = differential(
        FUN_150AE, {"a6": SCRIPT, "_pokes": pokes},
        lambda lib, buf: lib.obj_action(buf, SCRIPT))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


def test_fun_150ae_sin_seleccion():
    """$1acc9 == 0: comando de la tabla $1ab5c y dispatch (0 = no-op)."""
    diffs, info = differential(
        FUN_150AE, {"a6": SCRIPT, "_pokes": _ae_wait_pokes()},
        lambda lib, buf: lib.obj_action(buf, SCRIPT),
        schedule=_wait_schedule(), wait_sites=[IKBD_SITE, VBL_SITE])
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


def test_fun_150ae_tabla_cero():
    """$1acc9 != 0 con entrada 0: marca el objeto, cae en 0x1513a (0x159e2)."""
    pokes = _ae_wait_pokes()
    pokes[0x1ACC9] = bytes([0x01])
    pokes[0x1AB64] = bytes([0x00, 0x00])     # word de la entrada (a0+2)
    pokes[0x1A772] = (0x40020).to_bytes(4, "big")
    pokes[0x1A81A] = A81A_OBJ.to_bytes(4, "big")
    pokes[A81A_OBJ] = bytes([0x10, 0x00, 0x20])
    for i in range(0x44):                    # tabla 0x23ee8: entradas de 1 byte
        pokes[0x23EE8 + i] = bytes([0x01])
    diffs, info = differential(
        FUN_150AE, {"a6": SCRIPT, "_pokes": pokes},
        lambda lib, buf: lib.obj_action(buf, SCRIPT),
        schedule=_wait_schedule(), wait_sites=[IKBD_SITE, VBL_SITE])
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


def test_fun_150ae_tabla_con_comando():
    """$1acc9 != 0 con comando 0xa001: salta directo a 0x1513a y despacha idx 0."""
    pokes = _ae_wait_pokes()
    pokes[0x1ACC9] = bytes([0x01])
    pokes[0x1AB64] = bytes([0xA0, 0x01])
    pokes[0x1A772] = (0x40020).to_bytes(4, "big")
    pokes[0x1A81A] = A81A_OBJ.to_bytes(4, "big")
    pokes[A81A_OBJ] = bytes([0x10, 0x00, 0x20])
    for i in range(0x44):
        pokes[0x23EE8 + i] = bytes([0x01])
    diffs, info = differential(
        FUN_150AE, {"a6": SCRIPT, "_pokes": pokes},
        lambda lib, buf: lib.obj_action(buf, SCRIPT),
        schedule=_wait_schedule(), wait_sites=[IKBD_SITE, VBL_SITE])
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


TB_SCREEN = 0x80000
TB_TABLE = 0x3EC8A


def _textbox_pokes():
    """Pokes de la caja de texto (como test_textbox) con la entrada 0xaa pedida."""
    pokes = {
        0x1A45E: TB_SCREEN.to_bytes(4, "big"),
        0x1A46A: (0x8000).to_bytes(4, "big"),
        0x1A450: (0x30).to_bytes(2, "big"),
        0x1A452: (0x40).to_bytes(2, "big"),
        0x1ACC6: bytes([0x87]),              # prompt_wait por Cnecin
        0x1A44F: bytes([0x00]),
        0x1A456: bytes([0x00]),
        0x1ACD4: bytes([0x05]),
        0x1A74E: bytes([0x00]),
        TB_SCREEN: bytes((0x5A + i * 3) & 0xFF for i in range(0x4000)),
    }
    for i in range(0xA6):                    # 0xa6 entradas de 1 byte y la pedida
        pokes[TB_TABLE + i] = bytes([0x01])
    pokes[TB_TABLE + 0xA6] = bytes([3, 0x21, 0x22])
    pokes.update(console_key("a"))
    return pokes


def test_fun_150ae_directo_90aa():
    """$1ad09 != 0 (no 0x12): marca el objeto y despacha 0x90aa (caja de texto)."""
    pokes = _textbox_pokes()
    pokes[0x1AD20] = bytes([0x15])
    pokes[0x1AD09] = bytes([0x01])
    pokes[0x1A772] = (0x40020).to_bytes(4, "big")
    diffs, info = differential(
        FUN_150AE, {"a6": SCRIPT, "_pokes": pokes},
        lambda lib, buf: lib.obj_action(buf, SCRIPT))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


# ================= accion con espera de clic 0x152ee =================

FUN_152EE = 0x152EE
harness._lib.cursor_anim_wait.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.cursor_anim_wait.restype = ctypes.c_uint32


def _ee_pokes():
    pokes = _ae_wait_pokes()
    pokes[0x1A450] = (0x40).to_bytes(2, "big")
    pokes[0x1A452] = (0x30).to_bytes(2, "big")
    pokes[0x1AB5A] = bytes([0xA0, 0x35])
    for i in range(0x44):
        pokes[0x23EE8 + i] = bytes([0x01])
    return pokes


def test_fun_152ee_sin_seleccion():
    """$1acc9 == 0: despacha el comando por defecto $1ab5a."""
    diffs, info = differential(
        FUN_152EE, {"a6": SCRIPT, "_pokes": _ee_pokes()},
        lambda lib, buf: lib.cursor_anim_wait(buf, SCRIPT),
        schedule=_wait_schedule(), wait_sites=[IKBD_SITE, VBL_SITE])
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


def test_fun_152ee_tabla():
    """$1acc9 != 0 con comando 0xa003 en 0x1ab68: despacha la seleccion."""
    pokes = _ee_pokes()
    pokes[0x1ACC9] = bytes([0x01])
    pokes[0x1AB68] = bytes([0xA0, 0x35])
    diffs, info = differential(
        FUN_152EE, {"a6": SCRIPT, "_pokes": pokes},
        lambda lib, buf: lib.cursor_anim_wait(buf, SCRIPT),
        schedule=_wait_schedule(), wait_sites=[IKBD_SITE, VBL_SITE])
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


def test_fun_152ee_tabla_cero():
    """$1acc9 != 0 con entrada 0: cae al comando por defecto."""
    pokes = _ee_pokes()
    pokes[0x1ACC9] = bytes([0x01])
    pokes[0x1AB68] = bytes([0x00, 0x00])
    diffs, info = differential(
        FUN_152EE, {"a6": SCRIPT, "_pokes": pokes},
        lambda lib, buf: lib.cursor_anim_wait(buf, SCRIPT),
        schedule=_wait_schedule(), wait_sites=[IKBD_SITE, VBL_SITE])
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


def test_fun_152ee_directo():
    """$1ad09 != 0: despacha 0x1ac00 sin esperas."""
    pokes = {
        0x1AD20: bytes([0x15]),
        0x1AD09: bytes([0x01]),
        0x1AC00: bytes([0xA0, 0x35]),
    }
    diffs, info = differential(
        FUN_152EE, {"a6": SCRIPT, "_pokes": pokes},
        lambda lib, buf: lib.cursor_anim_wait(buf, SCRIPT))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


# ================= nombre + Cnecin 0x15428 =================

FUN_15428 = 0x15428
harness._lib.name_prompt.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.name_prompt.restype = ctypes.c_uint32


def _name_pokes(entries=140):
    pokes = {
        0x1AD20: bytes([0x15]),
        0x1AD09: bytes([0x00]),
        0x1ACC6: bytes([0x87]),              # modo sin overlay -> Cnecin directo
        0x1AB4F: bytes([0x00]),              # Supexec activo
        0x1ACD4: bytes([0x00]),              # contador de VBL
        0x1A45E: (0x80000).to_bytes(4, "big"),
        0x1ACE9: bytes([0x10]),
    }
    for i in range(entries):                 # banco de textos [len=3][c1][c2]
        pokes[0x3C534 + i * 3] = bytes([3, 0x21 + (i % 0x40), 0x22])
    pokes.update(console_key("a"))
    return pokes


def test_fun_15428_nombre():
    """$1ab56 con bit15 limpio y $1acec < 0x3f: imprime y espera Cnecin."""
    pokes = _name_pokes()
    pokes[0x1AB56] = bytes([0x00, 0x05])
    pokes[0x1ACEC] = bytes([0x20])
    diffs, _ = differential(
        FUN_15428, {"a6": SCRIPT, "_pokes": pokes},
        lambda lib, buf: lib.name_prompt(buf, SCRIPT))
    assert not diffs, report(diffs)


def test_fun_15428_nombre_override_113():
    """$1acec >= 0x3f y $1acd2 < 0x16: D4 se fuerza a 0x113 (banco largo)."""
    pokes = _name_pokes(entries=0x112)
    pokes[0x1AB56] = bytes([0x00, 0x05])
    pokes[0x1ACEC] = bytes([0x3F])
    pokes[0x1ACD2] = bytes([0x15])
    diffs, _ = differential(
        FUN_15428, {"a6": SCRIPT, "_pokes": pokes},
        lambda lib, buf: lib.name_prompt(buf, SCRIPT))
    assert not diffs, report(diffs)


def test_fun_15428_nombre_breve():
    """D4 < 4: 0x10fb4 consume A6[1] (el banco arranca en 0x3c534)."""
    pokes = _name_pokes(entries=0x105)
    pokes[0x1AB56] = bytes([0x00, 0x01])
    pokes[0x1ACEC] = bytes([0x20])
    pokes[SCRIPT + 1] = bytes([0x07])        # el byte que se salta 0x10fb4
    diffs, _ = differential(
        FUN_15428, {"a6": SCRIPT, "_pokes": pokes},
        lambda lib, buf: lib.name_prompt(buf, SCRIPT))
    assert not diffs, report(diffs)


def test_fun_15428_comando():
    """$1ac04 con bit15: despacha el comando (0xa035 -> indice 52, rts)."""
    pokes = _name_pokes()
    pokes[0x1AD09] = bytes([0x01])
    pokes[0x1AC04] = bytes([0xA0, 0x35])
    diffs, info = differential(
        FUN_15428, {"a6": SCRIPT, "_pokes": pokes},
        lambda lib, buf: lib.name_prompt(buf, SCRIPT))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


# ================= transicion de sala por filas 0x151b0 =================

FUN_151B0 = 0x151B0
B0_SCREEN = 0x80000
B0_OBJ = 0x40010

harness._lib.row_transition.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.row_transition.restype = ctypes.c_uint32


def _b0_obj(cls=0x10, row=0, flags=0x00):
    b = bytearray(8)
    b[0] = cls
    b[2] = row
    b[4] = flags
    return bytes(b)


def _b0_pokes(objs):
    pokes = {
        0x1AD20: bytes([0x15]),
        0x1AD09: bytes([0x00]),
        0x1ACC7: bytes([0x00]),
        0x1AB40: bytes([0x00]),              # x0
        0x1AB41: bytes([0x08]),              # x1 -> (8>>2)-1 = 1 (2 longs por fila)
        0x1AB42: bytes([0x00]),              # y0
        0x1AB43: bytes([0x02]),              # y1 -> 2 filas (2 esperas de VBL)
        0x1AB54: bytes([0xA0, 0x35]),
        0x1AC08: bytes([0xA0, 0x35]),
        0x1A466: (0x40000).to_bytes(4, "big"),
        0x40002: bytes([0x00, 0x00]),        # paleta inactiva
        0x1A45E: B0_SCREEN.to_bytes(4, "big"),
        0x1AB4F: bytes([0x00]),
        0x1ACD4: bytes([0x10]),
        0x1A7CA: B0_OBJ.to_bytes(4, "big"),
        0x1A7CE: (B0_OBJ + len(objs)).to_bytes(4, "big"),
    }
    pokes[B0_OBJ] = objs
    for i in range(0x4000):
        pokes[B0_SCREEN + i] = bytes([(i * 3 + 1) & 0xFF])
    for i in range(0x44):                    # tabla 0x23ee8 para 0x159e2
        pokes[0x23EE8 + i] = bytes([0x01])
    return pokes


B0_SCHEDULE = [
    {"pc": VBL_SITE, "nth": 1, "addr": VBL_COUNTER, "width": 1, "value": 0x11},
    {"pc": VBL_SITE, "nth": 2, "addr": VBL_COUNTER, "width": 1, "value": 0x12},
]


def test_fun_151b0_sin_objeto():
    """Ningun objeto coincide: las filas se intercambian y despacha $1ab54."""
    diffs, info = differential(
        FUN_151B0, {"a6": SCRIPT, "_pokes": _b0_pokes(_b0_obj(flags=0x00))},
        lambda lib, buf: lib.row_transition(buf, SCRIPT),
        schedule=B0_SCHEDULE, wait_sites=[VBL_SITE])
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


def test_fun_151b0_con_objeto():
    """Un objeto por fila (clase 0x28): se marca, suena y anima en cada pasada."""
    objs = _b0_obj(row=0, flags=0x28) + _b0_obj(row=1, flags=0x28)
    diffs, info = differential(
        FUN_151B0, {"a6": SCRIPT, "_pokes": _b0_pokes(objs)},
        lambda lib, buf: lib.row_transition(buf, SCRIPT),
        schedule=B0_SCHEDULE, wait_sites=[VBL_SITE])
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


def test_fun_151b0_directo():
    """$1ad09 != 0: despacha $1ac08 sin tocar pantalla."""
    pokes = {0x1AD20: bytes([0x15]), 0x1AD09: bytes([0x01]),
             0x1AC08: bytes([0xA0, 0x35])}
    diffs, info = differential(
        FUN_151B0, {"a6": SCRIPT, "_pokes": pokes},
        lambda lib, buf: lib.row_transition(buf, SCRIPT))
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


# ================= copia de pantalla de la familia (0x15934) =================

FUN_15934 = 0x15934
harness._lib.copy_rows_a.argtypes = [_u8p]
harness._lib.copy_rows_a.restype = None


def test_fun_15934_copia_con_espera():
    """Variante de 0x15944 con screen_ptr_a y $1a573=1: una VBL por cada copia."""
    pokes = {
        0x1A45E: (0x80000).to_bytes(4, "big"),
        0x1A573: bytes([0x00]),
        0x1ACD4: bytes([0x00]),
    }
    schedule = [{"pc": VBL_SITE, "nth": k, "addr": VBL_COUNTER, "width": 1, "value": k}
                for k in range(1, 0x36)]
    diffs, _ = differential(
        FUN_15934, {"_pokes": pokes},
        lambda lib, buf: lib.copy_rows_a(buf),
        schedule=schedule, wait_sites=[VBL_SITE])
    assert not diffs, report(diffs)


# ================= anillo de iconos 0x1566a =================

FUN_1566A = 0x1566A
harness._lib.icon_ring.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.icon_ring.restype = ctypes.c_uint32


def test_fun_1566a_anillo():
    """Un bit en la mascara: un rect, hit-test sin acierto y salida por clic."""
    pokes = {
        0x1A45E: (0x90000).to_bytes(4, "big"),
        0x1A450: (0x00).to_bytes(2, "big"),
        0x1A452: (0x00).to_bytes(2, "big"),
        0x1A575: bytes([0xFF]),              # el original lo calcula del cursor
        0x1A576: bytes([0x10]),
        0x1AB45: bytes([0x75]),              # selector A pintado
        0x1AB47: bytes([0x64]),              # sin repintado de overlay
        0x1AB4F: bytes([0x00]),
        0x1ACD4: bytes([0x10]),
        0x1A44F: bytes([0xFF]),
        0x1A456: bytes([0x00]),
        SCRIPT: bytes([0x00, 0x01, 0x75, 0x00, 0x00]),
    }
    for i in range(0x4000):
        pokes[0x90000 + i] = bytes([(i * 3 + 1) & 0xFF])
    schedule = [
        {"pc": IKBD_SITE, "nth": 1, "addr": IKBD_PACKET, "width": 1, "value": 0x01},
        {"pc": IKBD_SITE, "nth": 2, "addr": IKBD_PACKET, "width": 1, "value": 0x01},
        {"pc": VBL_SITE, "nth": 1, "addr": VBL_COUNTER, "width": 1, "value": 0x11},
        {"pc": VBL_SITE, "nth": 2, "addr": VBL_COUNTER, "width": 1, "value": 0x12},
    ]
    diffs, info = differential(
        FUN_1566A, {"a6": SCRIPT, "_pokes": pokes},
        lambda lib, buf: lib.icon_ring(buf, SCRIPT),
        schedule=schedule, wait_sites=[IKBD_SITE, VBL_SITE])
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


# ================= bucle del anillo 0x15888 =================

FUN_15888 = 0x15888
harness._lib.icon_ring_loop.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.icon_ring_loop.restype = ctypes.c_uint32
harness._lib.icon_ring_loop_ex.argtypes = [_u8p, ctypes.c_uint32,
                                           ctypes.POINTER(ctypes.c_int)]
harness._lib.icon_ring_loop_ex.restype = ctypes.c_uint32


def test_fun_15888_sin_acierto():
    """the_command = 0xffff: guarda/restaura el fin de plantilla y retorna."""
    pokes = {
        0x1A45E: (0x90000).to_bytes(4, "big"),
        0x1A450: (0x00).to_bytes(2, "big"),
        0x1A452: (0x00).to_bytes(2, "big"),
        0x1A575: bytes([0xFF]),
        0x1A576: bytes([0x10]),
        0x1AB45: bytes([0x75]),
        0x1AB47: bytes([0x64]),
        0x1AB4F: bytes([0x00]),
        0x1ACD4: bytes([0x10]),
        0x1A44F: bytes([0xFF]),
        0x1A456: bytes([0x00]),
        0x1A4E0: (0x2AAAA).to_bytes(4, "big"),
        SCRIPT: bytes([0x00, 0x01, 0x75, 0x00, 0x00]),
    }
    for i in range(0x4000):
        pokes[0x90000 + i] = bytes([(i * 3 + 1) & 0xFF])
    schedule = [
        {"pc": IKBD_SITE, "nth": 1, "addr": IKBD_PACKET, "width": 1, "value": 0x01},
        {"pc": IKBD_SITE, "nth": 2, "addr": IKBD_PACKET, "width": 1, "value": 0x01},
        {"pc": VBL_SITE, "nth": 1, "addr": VBL_COUNTER, "width": 1, "value": 0x11},
        {"pc": VBL_SITE, "nth": 2, "addr": VBL_COUNTER, "width": 1, "value": 0x12},
    ]
    diffs, info = differential(
        FUN_15888, {"a6": SCRIPT, "_pokes": pokes},
        lambda lib, buf: lib.icon_ring_loop(buf, SCRIPT),
        schedule=schedule, wait_sites=[IKBD_SITE, VBL_SITE])
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]


def test_fun_15888_ex_sin_tail():
    """icon_ring_loop_ex expone el flag de tail-call; en la rama 0xffff no lo
    activa y devuelve el mismo A6 que el wrapper."""
    pokes = {
        0x1A45E: (0x90000).to_bytes(4, "big"),
        0x1A450: (0x00).to_bytes(2, "big"),
        0x1A452: (0x00).to_bytes(2, "big"),
        0x1A575: bytes([0xFF]),
        0x1A576: bytes([0x10]),
        0x1AB45: bytes([0x75]),
        0x1AB47: bytes([0x64]),
        0x1AB4F: bytes([0x00]),
        0x1ACD4: bytes([0x10]),
        0x1A44F: bytes([0xFF]),
        0x1A456: bytes([0x00]),
        0x1A4E0: (0x2AAAA).to_bytes(4, "big"),
        SCRIPT: bytes([0x00, 0x01, 0x75, 0x00, 0x00]),
    }
    for i in range(0x4000):
        pokes[0x90000 + i] = bytes([(i * 3 + 1) & 0xFF])
    schedule = [
        {"pc": IKBD_SITE, "nth": 1, "addr": IKBD_PACKET, "width": 1, "value": 0x01},
        {"pc": IKBD_SITE, "nth": 2, "addr": IKBD_PACKET, "width": 1, "value": 0x01},
        {"pc": VBL_SITE, "nth": 1, "addr": VBL_COUNTER, "width": 1, "value": 0x11},
        {"pc": VBL_SITE, "nth": 2, "addr": VBL_COUNTER, "width": 1, "value": 0x12},
    ]
    tail = ctypes.c_int(-1)

    def glue(lib, buf):
        tail.value = -1
        return lib.icon_ring_loop_ex(buf, SCRIPT, ctypes.byref(tail))

    diffs, info = differential(
        FUN_15888, {"a6": SCRIPT, "_pokes": pokes}, glue,
        schedule=schedule, wait_sites=[IKBD_SITE, VBL_SITE])
    assert not diffs, report(diffs)
    assert info["ret"] == info["regs"]["a6"]
    assert tail.value == 0
