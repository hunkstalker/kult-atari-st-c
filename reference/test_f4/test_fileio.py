"""Differential tests for the resource loader: load_resource @ 0x1076a and resource list @ 0x106f8.

Fopen/Read/Close go through the kit's staged-file model, and Fsetdta/Fsfirst through the new
directory-search model (the size lands at DTA+0x1a). Everything that changes (destination bytes,
file cursor, the stored handle, the DTA entry) is image state, so the byte diff covers it.
"""
import ctypes

import harness
from harness import console_key, differential, report, stage_files

LOAD_RESOURCE = 0x1076a
LOAD_LIST = 0x106f8
FUN_106E2 = 0x106e2
FUN_106EA = 0x106ea
NAME = 0x30000
TABLE = 0x31000
BUF = 0x20000

_u8p = ctypes.POINTER(ctypes.c_uint8)
harness._lib.g_load_resource.argtypes = [_u8p]
harness._lib.g_load_resource.restype = None
harness._lib.g_load_resource_list.argtypes = [_u8p, ctypes.c_uint32]
harness._lib.g_load_resource_list.restype = ctypes.c_int32
harness._lib.g_fun_106e2.argtypes = [_u8p]
harness._lib.g_fun_106e2.restype = ctypes.c_int32
harness._lib.g_fun_106ea.argtypes = [_u8p]
harness._lib.g_fun_106ea.restype = ctypes.c_int32


def _pokes(fname, data, length=None):
    pokes, _handles = stage_files([(fname, data)])
    pokes[NAME] = fname.encode("ascii") + b"\0"
    pokes[0x1a55a] = NAME.to_bytes(4, "big")           # cur_filename
    pokes[0x1a55e] = BUF.to_bytes(4, "big")             # load_buffer
    pokes[0x1a562] = (len(data) if length is None else length).to_bytes(4, "big")
    return pokes


def _run(pokes):
    diffs, _ = differential(LOAD_RESOURCE, {"_pokes": pokes},
                            lambda lib, buf: lib.g_load_resource(buf))
    assert not diffs, report(diffs)


def test_load_resource():
    _run(_pokes("TEST.BIN", bytes(range(256))))


def test_load_resource_partial_read():
    """A short Read leaves the cursor part-way; both sides must agree on it."""
    _run(_pokes("TEST.BIN", bytes(range(64)), length=10))


def test_load_resource_zero_length():
    _run(_pokes("TEST.BIN", bytes(range(32)), length=0))


def test_load_resource_length_past_eof():
    """load_length larger than the file: Read caps at the remaining bytes on both sides."""
    _run(_pokes("TEST.BIN", bytes(range(8)), length=0x1000))


# NOTE: the unstaged-name path is deliberately NOT a differential case — the oracle's os_fopen
# refuses it and emu.run raises rather than fabricate a result (kit TRAP_MODEL.md). The refusal is
# the model's guarantee; there is nothing on the candidate side to compare against.


WRITE = 0x107a2
SAVE_CLEAR = 0x1077e


def test_write_named_file():
    pokes, _ = stage_files([("A:CLEAR.BIN", b"", 0x100)])   # capacity para crecer
    pokes[0x1a439] = b"A:CLEAR.BIN\0"
    pokes[0x1a55a] = (0x1a439).to_bytes(4, "big")           # cur_filename
    pokes[0x1a55e] = (0x30000).to_bytes(4, "big")           # load_buffer
    pokes[0x1a562] = (32).to_bytes(4, "big")                # load_length
    pokes[0x30000] = bytes(range(32))
    diffs, _ = differential(WRITE, {"_pokes": pokes},
                            lambda lib, buf: lib.g_write_named_file(buf))
    assert not diffs, report(diffs)


def test_save_clear_bin():
    pokes, _ = stage_files([("A:CLEAR.BIN", b"", 0x291c)])
    diffs, _ = differential(SAVE_CLEAR, {"_pokes": pokes},
                            lambda lib, buf: lib.g_save_clear_bin(buf))
    assert not diffs, report(diffs)


def _entry(name, buf):
    e = bytearray(14)
    nb = name.encode("ascii") + b"\0"
    assert len(nb) <= 10, "the 10-byte name field must hold the name and its NUL"
    e[:len(nb)] = nb
    e[10:14] = buf.to_bytes(4, "big")
    return bytes(e)


def test_load_resource_list():
    """Two 14-byte entries then '$'; both files must be staged (Fsfirst refuses otherwise)."""
    a, b = bytes(range(20)), bytes(range(50, 80))
    pokes, _h = stage_files([("DIALE.BIN", a), ("DESCE.BIN", b)])
    pokes[TABLE] = (_entry("DIALE.BIN", BUF) + _entry("DESCE.BIN", BUF + 0x4000) + b"$")
    regs = {"a5": TABLE, "_pokes": pokes}
    diffs, info = differential(LOAD_LIST, regs,
                               lambda lib, buf: lib.g_load_resource_list(buf, TABLE))
    assert not diffs, report(diffs)
    # The original returns the status in the low word (0 or 0xffff); the upper word is undefined.
    assert (info["ret"] & 0xffff) == (info["regs"]["d0"] & 0xffff)


def _run_list_wrapper(entry, fn, table_addr):
    a, b = bytes(range(20)), bytes(range(50, 80))
    pokes, _h = stage_files([("DIALE.BIN", a), ("DESCE.BIN", b)])
    pokes[table_addr] = (_entry("DIALE.BIN", BUF) + _entry("DESCE.BIN", BUF + 0x4000) + b"$")
    diffs, info = differential(entry, {"_pokes": pokes},
                               lambda lib, buf: fn(buf))
    assert not diffs, report(diffs)
    assert (info["ret"] & 0xffff) == (info["regs"]["d0"] & 0xffff)


def test_fun_106e2():
    """FUN_000106e2 seeds A5 = 0x1a3aa and falls into load_resource_list."""
    _run_list_wrapper(FUN_106E2, harness._lib.g_fun_106e2, 0x1a3aa)


def test_fun_106ea():
    """FUN_000106ea seeds A5 = 0x1a3e4 and falls into load_resource_list."""
    _run_list_wrapper(FUN_106EA, harness._lib.g_fun_106ea, 0x1a3e4)


# Wrappers GEMDOS sueltos (0x107b6..0x10828).
GEMDOS_FOPEN = 0x107B6
GEMDOS_FCREATE = 0x107D0
GEMDOS_FREAD = 0x107E8
GEMDOS_FWRITE = 0x10808
GEMDOS_FCLOSE = 0x10828
for _n in ("g_gemdos_fopen", "g_gemdos_fcreate", "g_gemdos_fread",
           "g_gemdos_fwrite", "g_gemdos_fclose"):
    getattr(harness._lib, _n).argtypes = [_u8p]
    getattr(harness._lib, _n).restype = None


def test_gemdos_fopen():
    pokes = _pokes("TEST.BIN", bytes(range(16)))
    diffs, _ = differential(GEMDOS_FOPEN, {"_pokes": pokes},
                            lambda lib, buf: lib.g_gemdos_fopen(buf))
    assert not diffs, report(diffs)


def _open_slot_pokes(name, data, capacity, length, buf=0x30000):
    """stage_files + marcar el slot como ABIERTO, que es el estado que fread/fwrite ven
    tras un Fopen/Fcreate (os_fwrite exige OS_FS_OFF_OPEN != 0)."""
    pokes, handles = stage_files([(name, data, capacity)])
    slot = int(handles[name]) - harness.OS_FS_FIRST_HANDLE
    entry_addr = harness.emu.OS_FS_TABLE + slot * harness.OS_FS_ENTRY
    entry = bytearray(pokes[entry_addr])
    entry[harness.OS_FS_OFF_OPEN:harness.OS_FS_OFF_OPEN + 4] = (1).to_bytes(4, "big")
    pokes[entry_addr] = bytes(entry)
    pokes[0x1a47e] = int(handles[name]).to_bytes(2, "big")
    pokes[0x1a55e] = buf.to_bytes(4, "big")
    pokes[0x1a562] = length.to_bytes(4, "big")
    return pokes


def test_gemdos_fcreate():
    pokes, _ = stage_files([("A:CLEAR.BIN", b"", 0x100)])
    pokes[0x1a439] = b"A:CLEAR.BIN\0"
    pokes[0x1a55a] = (0x1a439).to_bytes(4, "big")
    diffs, _ = differential(GEMDOS_FCREATE, {"_pokes": pokes},
                            lambda lib, buf: lib.g_gemdos_fcreate(buf))
    assert not diffs, report(diffs)


def test_gemdos_fwrite():
    pokes = _open_slot_pokes("A:CLEAR.BIN", b"", 0x100, 8)
    pokes[0x30000] = bytes(range(8))
    diffs, _ = differential(GEMDOS_FWRITE, {"_pokes": pokes},
                            lambda lib, buf: lib.g_gemdos_fwrite(buf))
    assert not diffs, report(diffs)


def test_gemdos_fread():
    pokes = _open_slot_pokes("TEST.BIN", bytes(range(16)), 0x10, 0x10)
    diffs, _ = differential(GEMDOS_FREAD, {"_pokes": pokes},
                            lambda lib, buf: lib.g_gemdos_fread(buf))
    assert not diffs, report(diffs)


def test_gemdos_fclose():
    pokes = _open_slot_pokes("TEST.BIN", bytes(range(16)), 0x10, 0x10)
    diffs, _ = differential(GEMDOS_FCLOSE, {"_pokes": pokes},
                            lambda lib, buf: lib.g_gemdos_fclose(buf))
    assert not diffs, report(diffs)


# ================= carga/guardado de sala (0x16ed8 / 0x16fb4) =================

FUN_16ED8 = 0x16ED8
FUN_16FB4 = 0x16FB4
FUN_15594 = 0x15594
FUN_155C4 = 0x155C4
ROOM_SCREEN = 0x80000
ROOM_BACK = 0x78000
ROOM_PAL = 0x30000
ROOM_OBJ = 0x41000
TB_TABLE = 0x3EC8A
ROOM_SIZE = 0x291C + 0x7D00 * 2

for _n, _args in (("g_fun_16ed8", [_u8p]), ("g_fun_16fb4", [_u8p]),
                  ("g_fun_15594", [_u8p]), ("g_fun_155c4", [_u8p])):
    getattr(harness._lib, _n).argtypes = _args
    getattr(harness._lib, _n).restype = ctypes.c_int32


def _room_data():
    """SCENA.BIN sintetico: la zona 0x1a752.. lleva la lista de objetos en 0x1a7ca,
    que 0x16f90 desplaza +0x101a8 (el formato guarda punteros relativos)."""
    d = bytearray((i * 13 + 5) & 0xFF for i in range(ROOM_SIZE))
    d[0x1A7CA - 0x1A752:0x1A7CA - 0x1A752 + 4] = (ROOM_OBJ - 0x101A8).to_bytes(4, "big")
    d[0x1A7CE - 0x1A752:0x1A7CE - 0x1A752 + 4] = (ROOM_OBJ + 8 - 0x101A8).to_bytes(4, "big")
    # $1acc6 (byte 0x2574) tambien cae dentro del fichero: modo sin overlay (Cnecin)
    d[0x1ACC4 - 0x1A752:0x1ACC4 - 0x1A752 + 4] = bytes([0x00, 0x00, 0x87, 0x00])
    return bytes(d)


def _room_base_pokes():
    pokes = {
        0x1A45E: ROOM_SCREEN.to_bytes(4, "big"),
        0x1A462: ROOM_BACK.to_bytes(4, "big"),
        0x1A466: ROOM_PAL.to_bytes(4, "big"),
        0x1A7CA: ROOM_OBJ.to_bytes(4, "big"),
        0x1A7CE: (ROOM_OBJ + 8).to_bytes(4, "big"),
        0x1AD0D: bytes([0x00]),
        0x1A750: bytes([0x00]),
        0x1A752: (0x111A8).to_bytes(4, "big"),        # punteros de hint (save)
        0x1A756: (0x00000).to_bytes(4, "big"),
        0x1A75A: (0x201A8).to_bytes(4, "big"),
        0x1A4E0: (0).to_bytes(4, "big"),
    }
    pokes[ROOM_OBJ] = bytes(8)
    for i in range(0x7D00):
        pokes[ROOM_SCREEN + i] = bytes([(i * 11 + 7) & 0xFF])
        pokes[ROOM_BACK + i] = bytes([(i * 5 + 3) & 0xFF])
    return pokes


def test_fun_16ed8_carga():
    pokes, _h = stage_files([("SCENA.BIN", _room_data(), ROOM_SIZE)])
    pokes.update(_room_base_pokes())
    diffs, info = differential(
        FUN_16ED8, {"_pokes": pokes},
        lambda lib, buf: lib.g_fun_16ed8(buf))
    assert not diffs, report(diffs)
    assert info["ret"] == 0


def test_fun_16ed8_fichero_corto():
    pokes, _h = stage_files([("SCENA.BIN", bytes(range(16)), 16)])
    pokes.update(_room_base_pokes())
    diffs, info = differential(
        FUN_16ED8, {"_pokes": pokes},
        lambda lib, buf: lib.g_fun_16ed8(buf))
    assert not diffs, report(diffs)
    assert info["ret"] == 1


def _blit_desc(dest):
    e = bytearray([2, 3])
    e += dest.to_bytes(4, "big")
    e += (4).to_bytes(2, "big")
    e += (4).to_bytes(4, "big")
    e += bytes(range(0x20, 0x20 + 24))
    return bytes(e)


DESC = 0x38000


def test_fun_16fb4_guardado():
    pokes, _h = stage_files([("SCENA.BIN", b"", ROOM_SIZE)])
    pokes.update(_room_base_pokes())
    pokes[DESC] = _blit_desc(ROOM_SCREEN)
    for i in range(16):
        pokes[0x1A776 + i * 4] = DESC.to_bytes(4, "big")
    diffs, info = differential(
        FUN_16FB4, {"_pokes": pokes},
        lambda lib, buf: lib.g_fun_16fb4(buf))
    assert not diffs, report(diffs)
    assert info["ret"] == 0


def _textbox_pokes(index):
    pokes = {
        0x1A46A: (0x8000).to_bytes(4, "big"),
        0x1A450: (0x30).to_bytes(2, "big"),
        0x1A452: (0x40).to_bytes(2, "big"),
        0x1ACC6: bytes([0x87]),
        0x1A44F: bytes([0x00]),
        0x1A456: bytes([0x00]),
        0x1ACD4: bytes([0x05]),
        0x1A74E: bytes([0x00]),
    }
    for i in range(index - 4):
        pokes[TB_TABLE + i] = bytes([0x01])
    pokes[TB_TABLE + index - 4] = bytes([3, 0x21, 0x22])
    pokes.update(console_key("a"))
    return pokes


def test_fun_15594_carga():
    pokes, _h = stage_files([("SCENA.BIN", _room_data(), ROOM_SIZE)])
    pokes.update(_room_base_pokes())
    pokes.update(_textbox_pokes(0xAA))
    diffs, _ = differential(
        FUN_15594, {"_pokes": pokes},
        lambda lib, buf: lib.g_fun_15594(buf))
    assert not diffs, report(diffs)


def test_fun_15594_error():
    pokes, _h = stage_files([("SCENA.BIN", bytes(range(16)), 16)])
    pokes.update(_room_base_pokes())
    pokes.update(_textbox_pokes(0x18F))
    diffs, _ = differential(
        FUN_15594, {"_pokes": pokes},
        lambda lib, buf: lib.g_fun_15594(buf))
    assert not diffs, report(diffs)


def test_fun_155c4_guardado():
    pokes, _h = stage_files([("SCENA.BIN", b"", ROOM_SIZE)])
    pokes.update(_room_base_pokes())
    pokes[DESC] = _blit_desc(ROOM_SCREEN)
    for i in range(16):
        pokes[0x1A776 + i * 4] = DESC.to_bytes(4, "big")
    pokes.update(_textbox_pokes(0xAA))
    diffs, _ = differential(
        FUN_155C4, {"_pokes": pokes},
        lambda lib, buf: lib.g_fun_155c4(buf))
    assert not diffs, report(diffs)
