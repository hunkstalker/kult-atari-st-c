"""Tests diferenciales del despacho de comandos.

run_command_keep_sp @ 0x103ae: despacha `the_command` ($1a50e) por su nibble alto
(0 -> VM de bytes; 0xa/b -> tabla de comandos; 0xf/otro -> walk_entry + VM).
FUN_00010630 @ 0x10630: VM de bytes (tabla 0x19f62) hasta un byte 0/>=0x6b o $1a4e0.

Los comandos cubiertos son los ya reconstruidos (0x11014 rts, 0x121c4 copy_screen).
"""
import ctypes

import harness
from harness import differential, report

RUN = 0x103ae
VM_BYTES = 0x10630
COMMAND = 0x1a50e
SCRIPT_END = 0x1a4e0
TEMPL = 0x1d402
SCRIPT = 0x30000

FRONT = 0x80000
BACK = 0x78000

_u8p = ctypes.POINTER(ctypes.c_uint8)
harness._lib.vm_run.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.vm_run.restype = ctypes.c_uint32
harness._lib.run_command_keep_sp.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.run_command_keep_sp.restype = None


def _screen_pokes():
    return {
        0x1a45e: FRONT.to_bytes(4, "big"),
        0x1a462: BACK.to_bytes(4, "big"),
        BACK: bytes.fromhex("deadbeefcafebabe"),
        BACK + 31992: bytes.fromhex("0102030405060708"),
        FRONT: bytes.fromhex("1112131415161718"),
        FRONT + 31992: bytes.fromhex("2122232425262728"),
    }


def test_vm_bytes_stop_on_zero():
    pokes = {SCRIPT: bytes([0]), SCRIPT_END: (SCRIPT + 100).to_bytes(4, "big")}
    diffs, _ = differential(VM_BYTES, {"a6": SCRIPT, "_pokes": pokes},
                            lambda lib, buf: lib.vm_run(buf, SCRIPT))
    assert not diffs, report(diffs)


def test_vm_bytes_stop_on_end_pointer():
    pokes = {SCRIPT: bytes([1]), SCRIPT_END: SCRIPT.to_bytes(4, "big")}
    diffs, _ = differential(VM_BYTES, {"a6": SCRIPT, "_pokes": pokes},
                            lambda lib, buf: lib.vm_run(buf, SCRIPT))
    assert not diffs, report(diffs)


def test_vm_bytes_copy_screen_handler():
    """Byte 68 -> 0x121c4 (addq.l #1,a6 + copy_screen); luego el 0 lo detiene."""
    pokes = _screen_pokes()
    pokes[SCRIPT] = bytes([68, 0])
    pokes[SCRIPT_END] = (SCRIPT + 100).to_bytes(4, "big")
    diffs, _ = differential(VM_BYTES, {"a6": SCRIPT, "_pokes": pokes},
                            lambda lib, buf: lib.vm_run(buf, SCRIPT))
    assert not diffs, report(diffs)


PAL_BUF = 0x30000
PAL_TABLE = 0x1a10a


def _pal_pokes():
    pokes = {0x1a466: PAL_BUF.to_bytes(4, "big")}
    for i in range(100):
        pokes[PAL_TABLE + i] = bytes([(0x40 + i) & 0xff])
    for i in range(32):
        pokes[PAL_BUF + i] = bytes([0xAA])
    return pokes


def test_vm_bytes_palette_handler():
    """Byte 94 -> 0x162ce (seed de paleta + Setpalette); A6 += 2 y el 0 detiene."""
    pokes = _pal_pokes()
    pokes[SCRIPT] = bytes([94, 5, 0])
    pokes[SCRIPT_END] = (SCRIPT + 100).to_bytes(4, "big")
    diffs, info = differential(VM_BYTES, {"a6": SCRIPT, "_pokes": pokes},
                               lambda lib, buf: lib.vm_run(buf, SCRIPT))
    assert not diffs, report(diffs)
    assert info["regs"]["events"] == [(harness.OS_EVENT_SETPALETTE, PAL_BUF)]


TEXT_TABLE = 0x41FB0
SCREEN = 0x90000


def test_vm_bytes_text_handler():
    """Byte 95 -> 0x141ea (blit de texto sobre la entrada en A6+1); A6 += 4, para el 0."""
    pokes = {
        SCRIPT: bytes([95]), SCRIPT + 1: bytes([0]), SCRIPT + 2: bytes([0x01]),
        SCRIPT + 3: bytes([0x00]), SCRIPT + 4: bytes([0]),
        TEXT_TABLE: (2).to_bytes(2, "big"), TEXT_TABLE + 2: bytes([2]),
        TEXT_TABLE + 3: bytes([2]), 0x1a45e: SCREEN.to_bytes(4, "big"),
        SCRIPT_END: (SCRIPT + 100).to_bytes(4, "big"),
    }
    for i in range(0x40):
        pokes[TEXT_TABLE + 4 + i] = bytes([(0x10 + i) & 0xff])
    for i in range(0x300):
        pokes[SCREEN - 0x8000 + i] = bytes([0x0F if i % 2 else 0xF0])
    diffs, _ = differential(VM_BYTES, {"a6": SCRIPT, "_pokes": pokes},
                            lambda lib, buf: lib.vm_run(buf, SCRIPT))
    assert not diffs, report(diffs)


def test_run_command_zero_nibble():
    """the_command con nibble alto 0 -> a6 = TEMPL + cmd y la VM de bytes."""
    pokes = {TEMPL + 0x10: bytes([0]), SCRIPT_END: (SCRIPT + 100).to_bytes(4, "big")}
    pokes[COMMAND] = (0x0010).to_bytes(2, "big")
    diffs, _ = differential(RUN, {"_pokes": pokes},
                            lambda lib, buf: lib.run_command_keep_sp(buf, 0))
    assert not diffs, report(diffs)


def test_run_command_ab_noop():
    """0xa035 -> índice 52 de cmd_jump_table = 0x11014 (rts)."""
    pokes = {COMMAND: (0xA035).to_bytes(2, "big")}
    diffs, _ = differential(RUN, {"_pokes": pokes},
                            lambda lib, buf: lib.run_command_keep_sp(buf, 0))
    assert not diffs, report(diffs)


def test_run_command_ab_copy_screen():
    """0xa06a -> índice 105 de cmd_jump_table = 0x121c4 (copy_screen).

    (La tabla de comandos 0x19eca no coincide con la de bytes 0x19f62: aquí
    0x121c4 es el índice 105, no el 68.)"""
    pokes = _screen_pokes()
    pokes[COMMAND] = (0xA06A).to_bytes(2, "big")
    diffs, _ = differential(RUN, {"_pokes": pokes},
                            lambda lib, buf: lib.run_command_keep_sp(buf, 0))
    assert not diffs, report(diffs)

# NOTA: la rama 0xf000/otra restaura A7 desde $1a826 (solo válido por run_command
# 0x103a8, que guarda A7). No se prueba por run_command_keep_sp porque el oráculo
# usaría un A7 basura al hacer rts; se cubrirá con run_command cuando se reconstruya.


def test_run_command_text_box():
    """0x9001 -> rama caja de texto (0x10440). En modo $1acc6 == 0x87 el prompt sale
    por Cnecin, así que se stagea una tecla. La tabla de texto (0x3ec8a) es BSB: se pokea."""
    from harness import console_key
    bank = bytes([3, 0x21, 0x22]) * 400     # el cmd 0x916b indexa la entrada 0x16b
    pokes = {
        0x1a45e: (0x80000).to_bytes(4, "big"),
        0x1a46a: (0x8000).to_bytes(4, "big"),
        0x3ec8a: bank,
        0x1a450: (0x30).to_bytes(2, "big"),
        0x1a452: (0x40).to_bytes(2, "big"),
        0x1acc6: bytes([0x87]),
        0x1a44f: bytes([0x00]),
        0x1a456: bytes([0x00]),
        0x1acd4: bytes([0x05]),
        0x1a74e: bytes([0x00]),
        COMMAND: (0x9001).to_bytes(2, "big"),
        0x80000: bytes((0x5A + i * 3) & 0xFF for i in range(0x4000)),
    }
    pokes.update(console_key("a"))
    diffs, _ = differential(RUN, {"_pokes": pokes},
                            lambda lib, buf: lib.run_command_keep_sp(buf, 0))
    assert not diffs, report(diffs)


def _template_chain(n):
    """Cadena de n entradas de plantilla de 2 bytes ([2][0]); el walk_entry de idx la
    recorre y aterriza en un byte 0 que detiene la VM. Devuelve los pokes."""
    pokes = {}
    for i in range(n):
        pokes[TEMPL + i * 2] = bytes([2, 0])
    pokes[TEMPL + n * 2] = bytes([0])       # destino: byte 0 -> la VM para
    pokes[SCRIPT_END] = (SCRIPT + 100).to_bytes(4, "big")
    return pokes


def test_fun_10390():
    """Handler que fija the_command = 0xf000 | word de a6[1..2]. La rama 0xf000 restaura
    A7 desde $1a826, así que se pokea con el A7 de entrada del oráculo (STACK_TOP)."""
    pokes = _template_chain(0x40)
    pokes[0x1a826] = harness.emu.STACK_TOP.to_bytes(4, "big")
    pokes[SCRIPT + 1] = bytes([0x10])
    pokes[SCRIPT + 2] = bytes([0x00])       # word = 0x1000 -> cmd 0xf010 -> idx 0x10-1
    diffs, _ = differential(0x10390, {"a6": SCRIPT, "_pokes": pokes},
                            lambda lib, buf: lib.dispatch_a6(buf, SCRIPT))
    assert not diffs, report(diffs)


def test_fun_186ee():
    """0x186ee: paleta 0x10, comando 0x916b (caja de texto) y paleta actual."""
    from harness import console_key
    bank = bytes([3, 0x21, 0x22]) * 400     # el cmd 0x916b indexa la entrada 0x16b
    pokes = {
        0x1a45e: (0x80000).to_bytes(4, "big"),
        0x1a46a: (0x8000).to_bytes(4, "big"),
        0x3ec8a: bank,
        0x1a450: (0x30).to_bytes(2, "big"),
        0x1a452: (0x40).to_bytes(2, "big"),
        0x1acc6: bytes([0x87]),
        0x1a44f: bytes([0x00]),
        0x1a456: bytes([0x00]),
        0x1acd4: bytes([0x05]),
        0x1a74e: bytes([0x00]),
        0x1a466: (0x30000).to_bytes(4, "big"),
        0x1acf4: bytes([3]),
        0x80000: bytes((0x5A + i * 3) & 0xFF for i in range(0x4000)),
    }
    for i in range(100):
        pokes[0x1a10a + i] = bytes([(0x40 + i) & 0xFF])
    for i in range(64):
        pokes[0x30000 + i] = bytes([0xAA])
    pokes.update(console_key("a"))
    diffs, _ = differential(0x186EE, {"_pokes": pokes},
                            lambda lib, buf: lib.dispatch_cmd(buf))
    assert not diffs, report(diffs)


# ---- cableado de la tabla de bytes (handlers ya reconstruidos) ----

def _vbl_k(n):
    return [{"pc": 0x1359a, "nth": k, "addr": 0x1acd4, "width": 1, "value": k}
            for k in range(1, n + 1)]


def test_vm_byte_1350c_jump():
    """Byte 51 -> 0x1350c: a6 = TEMPL + word(a6[1..2]); un 0 en el destino para la VM."""
    pokes = _screen_pokes()
    pokes[SCRIPT] = bytes([51, 0x00, 0x01, 0])
    pokes[TEMPL + 0x100] = bytes([0])
    pokes[SCRIPT_END] = (SCRIPT + 100).to_bytes(4, "big")
    diffs, _ = differential(VM_BYTES, {"a6": SCRIPT, "_pokes": pokes},
                            lambda lib, buf: lib.vm_run(buf, SCRIPT))
    assert not diffs, report(diffs)


def test_vm_byte_13522_13552():
    """Bytes 52/53 -> call-stack helpers; a6 sale de la pila en 0x13552."""
    pokes = _screen_pokes()
    pokes[0x1a4e0] = (SCRIPT_END).to_bytes(4, "big")
    pokes[0x1a4e4] = (SCRIPT + 4).to_bytes(4, "big")
    pokes[SCRIPT] = bytes([52, 0])
    pokes[SCRIPT_END] = (SCRIPT + 100).to_bytes(4, "big")
    diffs, _ = differential(VM_BYTES, {"a6": SCRIPT, "_pokes": pokes},
                            lambda lib, buf: lib.vm_run(buf, SCRIPT))
    assert not diffs, f"52\n{report(diffs)}"


def test_vm_byte_162d8_palette():
    """Byte 90 -> 0x162d8 (d0 = $1acf4, a6 += 1) y Setpalette."""
    pokes = _pal_pokes()
    pokes[0x1acf4] = bytes([5])
    pokes[SCRIPT] = bytes([90, 0])
    pokes[SCRIPT_END] = (SCRIPT + 100).to_bytes(4, "big")
    diffs, info = differential(VM_BYTES, {"a6": SCRIPT, "_pokes": pokes},
                               lambda lib, buf: lib.vm_run(buf, SCRIPT))
    assert not diffs, report(diffs)
    assert info["regs"]["events"] == [(harness.OS_EVENT_SETPALETTE, PAL_BUF)]


def test_vm_byte_18c8e_clear():
    """Byte 92 -> 0x18c8e: limpia los 0x80 de la lista 0x1a8f0 y a6 += 1."""
    pokes = _screen_pokes()
    pokes[0x1acf7] = bytes([0x55])
    for i in range(0x200):
        pokes[0x1a8f0 + i] = bytes([0x80 if i % 6 == 0 else (i * 5) & 0xFF])
    pokes[SCRIPT] = bytes([92, 0])
    pokes[SCRIPT_END] = (SCRIPT + 100).to_bytes(4, "big")
    diffs, _ = differential(VM_BYTES, {"a6": SCRIPT, "_pokes": pokes},
                            lambda lib, buf: lib.vm_run(buf, SCRIPT))
    assert not diffs, report(diffs)


def _hint_table_pokes():
    pokes = _screen_pokes()
    pokes[0x1a851] = bytes([0x11])
    pokes[0x1ad0a] = bytes([0x00])
    for i in range(10):
        base = 0x1a850 + i * 0x10
        pokes[base] = ((i + 1) & 0xffff).to_bytes(2, "big")
        pokes[base + 2] = (0x90000 + i * 0x10).to_bytes(4, "big")
        for k, v in enumerate((1, 2, 3, 4, 5)):
            pokes[base + 6 + k * 2] = v.to_bytes(2, "big")
    for i in range(0x400):
        pokes[0x88000 + i] = bytes([(i * 3 + 1) & 0xFF])
        pokes[0x90000 + i] = bytes([(i * 11 + 7) & 0xFF])
    return pokes


def test_vm_byte_1222e_icons():
    """Bytes 33/35 -> 0x1222e/0x12fea (icono A6[1] + 0x1215e); A6 += 2."""
    for b in (33, 35):
        pokes = _hint_table_pokes()
        pokes[SCRIPT] = bytes([b, 3, 0])
        pokes[SCRIPT_END] = (SCRIPT + 100).to_bytes(4, "big")
        diffs, _ = differential(VM_BYTES, {"a6": SCRIPT, "_pokes": pokes},
                                lambda lib, buf: lib.vm_run(buf, SCRIPT))
        assert not diffs, f"byte {b}\n{report(diffs)}"


def _elem_pokes(w, h):
    pokes = {
        0x2751e: bytes([5, 0, 0, 0x00, 0x00]),
        0x1a252: bytes([w]), 0x1a253: bytes([h]),
        0x1a254: (0x1111).to_bytes(2, "big"),
        0x1a256: (0x2222).to_bytes(2, "big"),
        0x1a258: (0x3333).to_bytes(2, "big"),
        0x1a25a: (0x4444).to_bytes(2, "big"),
        0x31072: (0x10).to_bytes(2, "big"), 0x31074: bytes([2, 2]),
        0x1a45e: (0x90000).to_bytes(4, "big"),
        0x1a851: bytes([0x22]), 0x1ad0a: bytes([0]),
        0x1acd4: bytes([0x00]),
        0x1a570: bytes([0]),
    }
    for i in range(0x100):
        pokes[0x31076 + i] = bytes([(i * 7 + 3) & 0xFF])
    for i in range(0x2000):
        pokes[0x3b1ae + i] = bytes([(i * 5 + 1) & 0xFF])
        pokes[0xD8F0 + i] = bytes([(i * 11 + 5) & 0xFF])
    for i in range(0x10000):
        pokes[0x90000 + i] = bytes([(i * 3 + 1) & 0xFF])
    for i in range(0x10 * 0x10):
        pokes[0x1a850 + i] = bytes([0])
    return pokes


def test_vm_byte_11f64_dispatch():
    """Byte 5 -> 0x11f64: 128a4 (clase/x/y inline) + 124da + 111ac; A6 sale +4."""
    pokes = _elem_pokes(2, 3)
    pokes[SCRIPT] = bytes([5, 1, 8, 4, 0])
    pokes[SCRIPT_END] = (SCRIPT + 100).to_bytes(4, "big")
    diffs, _ = differential(VM_BYTES, {"a6": SCRIPT, "_pokes": pokes},
                            lambda lib, buf: lib.vm_run(buf, SCRIPT),
                            schedule=_vbl_k(5), wait_sites=[0x1359a])
    assert not diffs, report(diffs)


def test_run_command_ab_wired():
    """Comandos 0xa/b reconstruidos sin A6: 0xa016 -> 0x172ba, 0xa020 -> 0x17472.
    (0xa01c -> 0x186ee lanza la caja de texto y necesita tecla; ya tiene su test.)"""
    pokes = _screen_pokes()
    pokes[0x1a806] = (0x90000).to_bytes(4, "big")
    pokes[0x1acaa] = (0x1234).to_bytes(2, "big")
    pokes[0x90000] = bytes([0xFF])
    for cmd in (0xA016, 0xA020):
        pokes2 = dict(pokes)
        pokes2[COMMAND] = cmd.to_bytes(2, "big")
        diffs, _ = differential(RUN, {"_pokes": pokes2},
                                lambda lib, buf: lib.run_command_keep_sp(buf, 0))
        assert not diffs, f"cmd={cmd:#x}\n{report(diffs)}"


harness._lib.inline_cmd_walk.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.inline_cmd_walk.restype = ctypes.c_uint32


def test_fun_104f4():
    """Byte 18 -> 0x104f4: the_command = word inline, walk_entry(templ, cmd-1)."""
    for word in (1, 2, 5):
        pokes = _template_chain(0x40)
        pokes[SCRIPT] = bytes([18, word & 0xff, word >> 8, 0])
        pokes[COMMAND] = (0).to_bytes(2, "big")
        diffs, info = differential(
            0x104F4, {"a6": SCRIPT, "_pokes": pokes},
            lambda lib, buf: lib.inline_cmd_walk(buf, SCRIPT))
        assert not diffs, f"word={word}\n{report(diffs)}"
        assert info["ret"] == info["regs"]["a6"], "a6"


def test_run_command_ab_c35c():
    """cmd 0xa006 (idx 5) -> 0x15588: the_command = 0xc35c y despacho."""
    pokes = {}
    for i in range(0x35C):
        pokes[0x1D402 + i] = bytes([1])
    pokes[0x1D402 + 0x35B] = bytes([0])
    pokes[0x1D402 + 0x35C] = bytes([0])
    diffs, _ = differential(0x15588, {"_pokes": pokes},
                            lambda lib, buf: lib.mode_flag_wrap(buf))
    assert not diffs, report(diffs)


def test_run_command_ab_c35d():
    """cmd 0xa007 (idx 6) -> 0x155b8: the_command = 0xc35d y despacho."""
    pokes = {}
    for i in range(0x35D):
        pokes[0x1D402 + i] = bytes([1])
    pokes[0x1D402 + 0x35C] = bytes([0])
    pokes[0x1D402 + 0x35D] = bytes([0])
    diffs, _ = differential(0x155B8, {"_pokes": pokes},
                            lambda lib, buf: lib.mode_queue_wrap(buf))
    assert not diffs, report(diffs)
