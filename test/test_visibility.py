"""Guardián de la API pública del core (visibilidad on-target).

`make core` construye `build/libkultcore.so` con `kult_core.map`, que deja
locales todos los símbolos salvo la API que consume el motor. Esta prueba
comprueba que la tabla dinámica del .so solo exporta esa lista (evita colisiones
al enlazar con SDL/otras librerías).
"""
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAP = ROOT / "kult_core.map"
CORE = ROOT / "build" / "libkultcore.so"


def _api_from_map():
    txt = MAP.read_text(encoding="utf-8")
    body = txt.split("global:")[1].split("local:")[0]
    return set(re.findall(r"^\s*([A-Za-z_]\w*)\s*;", body, re.M))


def test_core_exports_only_api():
    assert CORE.exists(), "falta build/libkultcore.so (ejecuta `make core`)"
    out = subprocess.check_output(["nm", "-D", "--defined-only", str(CORE)], text=True)
    exported = set()
    for ln in out.splitlines():
        parts = ln.split()
        if len(parts) >= 3:
            exported.add(parts[2].split("@")[0])
    api = _api_from_map()
    extra = exported - api
    missing = api - exported
    assert not extra, f"símbolos exportados fuera de la API: {sorted(extra)}"
    assert not missing, f"símbolos de la API no exportados: {sorted(missing)}"
