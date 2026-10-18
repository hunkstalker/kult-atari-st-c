"""Guardian de docs/mapa/: las fichas deben coincidir con tools/gen_mapa.py.

Si tocas src/ (funciones, comentarios o globales) o los tests, regenera:
    python3 tools/gen_mapa.py
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

import gen_mapa  # noqa: E402


def test_mapa_is_generated():
    files = gen_mapa.generate_all()
    stale = [p.name for p, t in files.items()
             if not p.exists() or p.read_text(encoding="utf-8") != t]
    assert not stale, (
        "docs/mapa/ desactualizado (regenera con `python3 tools/gen_mapa.py`): "
        + ", ".join(sorted(stale)))
