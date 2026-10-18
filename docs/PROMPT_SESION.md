# PROMPT para retomar en una sesión nueva

Copia y pega esto al empezar la próxima sesión:

---

Trabajamos en la reimplementación de **Kult: The Temple of Flying Saucers**
(Atari ST, 1989) en **C moderno, legible y documentado**, en
`/app/kult-atari-st-c`. La verdad/especificación es el decomp **ASM** congelado en
`/app/kult-atari-st-decomp` (`matching/auto_*.s`, byte a byte al original): **no se
toca**. Hay además `/app/kult-port`, el proyecto predecesor de modding (SDL2,
textos ES, overlay F1…) cuyas mejoras se re-aplicarán en la **fase 3**, solo tras
cerrar la revisión.

## Tarea: cerrar la **revisión P2** (última tanda)

Estado: **P0 y P1 revisados** (ver el registro en `docs/REVISION.md` §7 y §9).
La reimplementación cubre todo el juego: 33 módulos, 494 pruebas diferenciales
verdes, 0 warnings, `regscan ids_reg=0`. Falta **P2** (13 módulos) para poder
abrir la fase 3.

**Antes de tocar nada, lee, por este orden:**
1. `docs/ESTADO.md` — estado exacto, censo, cómo continuar.
2. `docs/REVISION.md` — plan y método. **El plan de P2 está en §10** (orden,
   foco por módulo, riesgos heredados, criterio de cierre). El registro de
   hallazgos de P0/P1 en §7/§9 sirve de ejemplo de la plantilla §5.
3. `docs/PLAN.md` (convenciones §4.1), `docs/REFERENCIA_F4.md` (trampas del
   oráculo/kit), `AGENTS.md` (reglas de trabajo).
4. `README.md`.

**Método por módulo (los 13 de §10, de menor a mayor):** para cada función,
abrir su `auto_*.s` en `/app/kult-atari-st-decomp/matching/` y comparar el cuerpo
C con el ASM instrucción a instrucción (anchos `.b`/`.w`/`.l`, signo, banderas
C/Z/N/V, pre/post-incremento de punteros y del guion `a6`, límites de bucle
`dbra` —ojo do-while—, orden de escritura, colas `rts`/fall-through, registros de
salida/`movem`, out-params y efectos off-image en el ledger). Registrar cada
hallazgo con la plantilla §5; si es un fallo real, arreglarlo **con su prueba
diferencial** y dejar `make test` verde. Un commit por módulo
(`revision(<módulo>): …`). Al terminar P2: rellenar el registro (§11) y
`docs/ESTADO.md`, y `make mapa`.

**Reglas:** reproducir/medir antes de tocar, una hipótesis un cambio, toda
corrección deja test, `make regscan` debe seguir en `ids_reg=0`. Empieza por
`stubs`/`delay`/`input`/`state`, termina por `screens`/`overlay`. Todo en español.
No toques `/app/kult-port` ni el decomp ASM.

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
- Al cerrar P2 se abre la **fase 3** (re-aplicar las mejoras de kult-port sobre
  este C limpio).
