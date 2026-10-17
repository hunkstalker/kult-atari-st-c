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
1. `/app/kult-atari-st-c/docs/PLAN.md` (plan maestro: método, convenciones, fases).
2. `/app/kult-atari-st-c/README.md`.
3. Para el análisis del original: `/app/kult-atari-st-c/docs/REFERENCIA_F4.md`.

**Método:** para cada pieza, mírala en el ASM de
`/app/kult-atari-st-decomp/matching/`, escribe C legible en
`/app/kult-atari-st-c/src/` (nombre descriptivo + comentario con la dirección
original, p. ej. `/* 0x162e2 — ... */`), escribe su **prueba diferencial** en
`/app/kult-atari-st-c/test/`, y deja `make test` en verde.

**Estado:** el arnés ya funciona (verificado: las 469 pruebas del intento previo
pasan con el C viejo). `src/` está **vacío**: empezamos de cero.

**Primera tarea — el PILOTO de la PALETA** (para acordar el estilo):
- `set_palette` (`0x135da`) → llama a `0x16300` (XBIOS **Setpalette**, trap #14
  función 6, con `A_PALETTE_PTR`; efecto de **hardware** → se comprueba por el
  ledger del oráculo).
- `0x162e2`: fija una entrada de paleta (`A_PAL_TABLE + d0*0xa` → 5 words a
  `A_PALETTE_PTR+2`; efecto en RAM/imagen).
- `0x162d8`: `addq.l #1, a6` (avanza el guion).

Entrégalo como `src/palette.c` + `test/test_palette.py` con `make test` verde, y
propón/confirma las convenciones de nombres, comentarios, estado y pruebas.

Reglas de trabajo del proyecto (si las hay en `/app/kult-port/AGENTS.md`,
respetarlas): reproducir/medir antes de tocar, una hipótesis un cambio, toda
corrección deja test, commit al completar una tarea con éxito. Todo en español.

---

## Nota (para quien preparó esto)

- El repo B se montó copiando el **arnés** (`tools/recreate_kit` + `Makefile` +
  `project.toml` + `include/` + `test/harness.py`) desde el intento anterior, y
  dejando `src/` y `test/test_*.py` vacíos. Las pruebas viejas quedan en
  `reference/test_f4/` solo como referencia (usan nombres antiguos).
- `work/disk_a/KULT.PRG` (copia del usuario) debe existir para el oráculo; está
  gitignored. Si falta, copiarlo desde `/app/kult-port/work/disk_a/KULT.PRG`.
- El `.venv` es un symlink al de kult-port (`projects/kult/recreate/.venv`); si no
  está, crear uno con `python3 -m venv .venv && .venv/bin/pip install -r requirements.txt`.
