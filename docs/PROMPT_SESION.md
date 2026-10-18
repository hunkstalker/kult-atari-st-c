# PROMPT para retomar en una sesión nueva

Copia y pega esto al empezar la próxima sesión:

---

Trabajamos en la reimplementación de **Kult: The Temple of Flying Saucers**
(Atari ST, 1989) en **C moderno, legible y documentado**, en
`/app/kult-atari-st-c`. La verdad/especificación es el decomp **ASM** congelado en
`/app/kult-atari-st-decomp` (`matching/auto_*.s`, byte a byte al original): **no se
toca**. Hay además `/app/kult-port`, el proyecto predecesor de modding (SDL2,
textos ES, overlay F1…) cuyas mejoras se re-aplican ahora en la **fase 3**.

## Tarea: abrir la **fase 3** (re-aplicar kult-port)

Estado: **revisión del C CERRADA** (registro en `docs/REVISION.md` §7 P0, §9 P1
y §11 P2; 0 ALTA/MEDIA abiertos). La reimplementación cubre todo el juego: 33
módulos, **499 pruebas diferenciales** verdes, 0 warnings, `make regscan`
`ids_reg=0`, `make core` OK.

**Antes de tocar nada, lee, por este orden:**
1. `docs/ESTADO.md` — estado exacto, censo, cómo continuar.
2. `docs/REVISION.md` — §11 (cierre de la revisión) y §3–§5 (método y
   plantilla de hallazgo).
3. `docs/PLAN.md` (fase 3 en §7, convenciones §4.1), `AGENTS.md`, `README.md`.
4. `docs/REFERENCIA_F4.md` (trampas del oráculo/kit) para pruebas.

**Primer paso obligatorio:** hacer el **plan detallado de la fase 3** antes de
implementar nada: qué mejoras de `kult-port` se re-aplican (guardado por
ranuras, textos ES, overlay F1, …), en qué orden, qué módulos de este C tocan y
qué pruebas diferenciales/de comportamiento las cubren. Documentarlo y
commitearlo. Solo entonces empezar a implementar.

**Reglas:** `/app/kult-port` es **referencia de solo lectura** (no se toca); el
decomp ASM tampoco. Reproducir/medir antes de tocar, una hipótesis un cambio,
toda corrección deja prueba, `make regscan` debe seguir en `ids_reg=0`. Todo en
español.

Comandos: `make test` (diferencial), `make test PYTEST_ARGS=-n0` (serial),
`make regscan`, `make core`, `make mapa`. El oráculo necesita
`work/disk_a/KULT.PRG` (gitignored; si falta, cópialo de
`/app/kult-port/work/disk_a/KULT.PRG`).

---

## Nota (para quien prepare un repo así)

- El repo se montó copiando el **arnés** (`tools/recreate_kit` + `Makefile` +
  `project.toml` + `include/` + `test/harness.py`) del intento anterior; las
  pruebas viejas quedan en `reference/test_f4/` solo como referencia.
- El `.venv` es un symlink al de kult-port; si no está, crear uno con
  `python3 -m venv .venv && .venv/bin/pip install -r requirements.txt`.
- La revisión del C (P0/P1/P2) está cerrada; la fase 3 re-aplica las mejoras de
  kult-port sobre este C limpio.
