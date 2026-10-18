# PROMPT para retomar en una sesión nueva

Copia y pega esto al empezar la próxima sesión:

---

Trabajamos en la reimplementación de **Kult: The Temple of Flying Saucers**
(Atari ST, 1989) en **C moderno, legible y documentado**.

**Hay dos repos, no los confundas:**
- `/app/kult-atari-st-decomp` — el **decomp canónico en ensamblador (ASM)**. Es la
  **especificación/verdad**: ensambla al original **byte a byte**
  (sha256 `2e85f223167439425ab2096bd21716de135f9b472bde90f4d40e8b0e03132fe6`). **NO
  se toca.** Su código está en `matching/` (`auto_*.s` por función, `*.s` manuales,
  `data_*.s`).
- `/app/kult-atari-st-c` — **este es donde trabajamos**: la **reimplementación en
  C**, fiel en **comportamiento** (no byte a byte, porque el original era ASM).
  Tiene su propio git.

**Antes de tocar nada, lee:**
1. `/app/kult-atari-st-c/docs/ESTADO.md` (estado exacto, censo, cómo continuar).
2. `/app/kult-atari-st-c/docs/PLAN.md` (plan maestro: método, convenciones, fases).
3. `/app/kult-atari-st-c/README.md`.
4. Para el análisis del original y trampas: `docs/REFERENCIA_F4.md`.

**Estado:** la reimplementación cubre **todo el juego** (33 módulos, 417 funciones,
478 pruebas diferenciales verdes, 0 warnings, 0 identificadores de registro en
código). Convenciones, `docs/mapa/`, visibilidad on-target y legibilidad están
cerradas.

**Método (para cualquier pieza nueva):** mírala en el ASM de
`/app/kult-atari-st-decomp/matching/`, escribe C legible en `src/` (nombre = el de
`work/names.txt` + comentario con la dirección original, p. ej.
`/* 0x162e2 — ... */`), su **prueba diferencial** en `test/`, y deja `make test`
en verde. Si añades un global, edítalo en `work/names.txt` y corre `make addrs`;
si tocas `src/`/`test/`, corre `make mapa`.

**Siguiente:** **revisar el C ya escrito y renombrado** antes de re-aplicar nada
de kult-port. El plan y el prompt de arranque están en **`docs/REVISION.md`**
(cuatro ejes: fidelidad vs ASM, nombres/comentarios, legibilidad, cobertura de
tests; prioridad P0 primero). Solo tras cerrar esa revisión se abre la fase 3
(mejoras de kult-port). No hay deuda conocida de comportamiento ni de
legibilidad.

Reglas de trabajo (ver `/app/kult-port/AGENTS.md`): reproducir/medir antes de
tocar, una hipótesis un cambio, toda corrección deja test, commit al completar
una tarea con éxito. Todo en español.

---

## Nota (para quien prepare un repo así)

- El repo se montó copiando el **arnés** (`tools/recreate_kit` + `Makefile` +
  `project.toml` + `include/` + `test/harness.py`) del intento anterior, dejando
  `src/` y `test/test_*.py` vacíos. Las pruebas viejas quedan en
  `reference/test_f4/` solo como referencia (usan nombres antiguos).
- `work/disk_a/KULT.PRG` (copia del usuario) debe existir para el oráculo; está
  gitignored. Si falta, copiarlo desde `/app/kult-port/work/disk_a/KULT.PRG`.
- El `.venv` es un symlink al de kult-port (`projects/kult/recreate/.venv`); si no
  está, crear uno con `python3 -m venv .venv && .venv/bin/pip install -r requirements.txt`.
