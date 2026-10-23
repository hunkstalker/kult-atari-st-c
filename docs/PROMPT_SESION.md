# PROMPT para retomar en una sesión nueva

Copia y pega esto al empezar la próxima sesión:

---

Trabajamos en la reimplementación de **Kult: The Temple of Flying Saucers**
(Atari ST, 1989) en **C moderno, legible y documentado**, en
`/app/kult-atari-st-c`. El trabajo se reparte en **cuatro** repos (fuente
canónica: **`docs/PROYECTOS.md`**):

- `/app/kult-atari-st-decomp` — decomp **ASM** del ST, byte a byte; **congelado**.
- **`/app/kult-atari-st-c`** (este) — **reimplementación en C** del ST; la base.
- `/app/kult-port-pc-compilation` — **la compilación** (Atari ST + Amiga,
  idiomas, mejoras); nace de este C. Plan: **`docs/PLAN_COMPILACION.md`**.
- `/app/kult-port-legacy` — predecesor de modding; **solo referencia**.

## Estado y tareas

- Este repo cubre **todo el juego** (33 módulos, **518 pruebas** diferenciales
  verdes, 0 warnings, `make regscan` `ids_reg=0`, `make core` OK). La revisión
  del C (P0/P1/P2) está **cerrada internamente** (registro en
  `docs/REVISION.md` §7/§9/§11).
- **Validación:** completada en Windows (2026-10-23). La base está **congelada**
  como `core-base-20261023`; cambios futuros exigen evidencia ASM + test.
- **Siguiente — la compilación:** el proyecto nuevo (`kult-port-pc-compilation`)
  que une **Atari ST + Amiga**, con mejoras y **todos los idiomas**. Nace de este
  C; `kult-port-legacy` es solo referencia.

**Antes de tocar nada, lee, por este orden:**
1. `docs/PROYECTOS.md` — topología de los 4 repos y reglas.
2. `docs/PLAN_COMPILACION.md` — plan definitivo de la compilación (incluida la
   **determinación sobre Amiga** y su primer hito).
3. `docs/ESTADO.md` — estado exacto, censo, cómo continuar.
4. `docs/REVISION.md` — §11 (cierre de la revisión) y §3–§5 (método).
5. `docs/PLAN.md` (convenciones §4.1), `docs/REFERENCIA_F4.md` (trampas del
   oráculo/kit), `AGENTS.md`, `README.md`.

## Reglas

- **`/app/kult-port-legacy` y `/app/kult-atari-st-decomp` no se tocan.**
- En este repo: reproducir/medir antes de tocar, una hipótesis un cambio, toda
  corrección deja prueba, `make regscan` debe seguir en `ids_reg=0`. Todo en
  español.
- Si la tarea es la compilación, es un **proyecto nuevo**: primero su plan ya
  está en `docs/PLAN_COMPILACION.md`.

Comandos (este repo): `make test`, `make test PYTEST_ARGS=-n0`, `make regscan`,
`make core`, `make mapa`/`make addrs`. El oráculo necesita
`work/disk_a/KULT.PRG` (gitignored; si falta, cópialo de
`/app/kult-port-legacy/work/disk_a/KULT.PRG`).

---

## Nota (para quien prepare un repo así)

- El repo se montó copiando el **arnés** (`tools/recreate_kit` + `Makefile` +
  `project.toml` + `include/` + `test/harness.py`) del intento anterior; las
  pruebas viejas quedan en `reference/test_f4/` solo como referencia.
- El `.venv` es un symlink al del predecesor
  (`/app/kult-port-legacy/projects/kult/recreate/.venv`).
- La revisión del C (P0/P1/P2) está cerrada internamente; la **compilación**
  reimplementa sobre este C limpio y toma `kult-port-legacy` como referencia.
