"""Guardia del unwind A7 en dispatch_a6 (0x10390).

La rama fxxx de 0x103ae restaura A7 desde $1a826. Cuando dispatch_a6 se ejecuta
dentro de un boundary de run_command, el rts del sub-VM sale del comando; sin
boundary conserva el retorno anidado. Este test compila un helper C minimo
contra build/libkult.so porque el comportamiento depende de setjmp/longjmp.
"""
import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
LIB = ROOT / "build"
INCLUDE = ROOT / "include"

_HELPER = r"""
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "addrs.h"
#include "run_boundary.h"

uint32_t dispatch_a6(uint8_t *, uint32_t);

static uint8_t img[0x100000];

static void setup_template(void) {
    memset(img, 0, sizeof(img));
    /* entry 0: len 2 */
    img[A_TEMPL_DATA + 0] = 2;
    /* entry 1: script [0x4d f003 0]; dispatch_a6 lee word en +1/+2 */
    img[A_TEMPL_DATA + 2] = 5;
    img[A_TEMPL_DATA + 3] = 0x4d;
    img[A_TEMPL_DATA + 4] = 0x03;   /* low  */
    img[A_TEMPL_DATA + 5] = 0x00;   /* high -> f003, idx 2 */
    /* entry 2: byte 0 detiene el sub-VM */
    img[A_TEMPL_DATA + 7] = 2;
}

int main(void) {
    setup_template();
    kult_run_boundary b;
    b.prev = 0;
    g_kult_run_boundary_top = &b;
    if (setjmp(b.env) == 0) {
        (void)dispatch_a6(img, A_TEMPL_DATA + 3);
        g_kult_run_boundary_top = 0;
        return 1;                   /* no unwind: fallo */
    }
    g_kult_run_boundary_top = 0;
    return 0;
}
"""


@pytest.mark.skipif(shutil.which("gcc") is None, reason="gcc no disponible")
def test_dispatch_a6_unwind_con_boundary(tmp_path):
    src = tmp_path / "probe.c"
    exe = tmp_path / "probe"
    src.write_text(_HELPER, encoding="utf-8")
    subprocess.run(
        ["gcc", "-std=c11", "-O0", "-I", str(INCLUDE), str(src), "-o", str(exe),
         "-L", str(LIB), "-lkult", f"-Wl,-rpath,{LIB}"],
        check=True, capture_output=True, text=True,
    )
    proc = subprocess.run([str(exe)], capture_output=True, text=True)
    assert proc.returncode == 0, (
        "dispatch_a6 no unwindó con un boundary activo "
        f"(rc={proc.returncode}, stderr={proc.stderr!r})"
    )
