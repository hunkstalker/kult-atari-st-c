# PROYECTOS — repos de Kult (Atari ST)

Documento **canónico**: cómo se relacionan los dos repositorios de Kult para
Atari ST y qué reglas aplican. Si algo de otro documento contradice a este, manda
este.

## Los dos repos

| repo | papel | garantía | regla |
|---|---|---|---|
| [`kult-atari-st-decomp`](https://github.com/hunkstalker/kult-atari-st-decomp) | decomp **ASM** del original Atari ST | **byte a byte** (`sha256 2e85f223…`) | **congelado**; no se toca |
| `kult-atari-st-c` (este) | **reimplementación en C** del ST, legible y renombrada | **mismo comportamiento** (diferencial Musashi) | es la **base congelada** (`core-base-20261023`) |

## Papeles en detalle

### `kult-atari-st-decomp` — especificación Atari ST

El decomp en ensamblador; `matching/` reproduce el original byte a byte. Es la
**verdad/especificación** y está **congelado**: no se modifica.

### `kult-atari-st-c` — el C verificado del ST

La reimplementación en C moderno, legible y documentado, fiel en comportamiento
y verificada con el **diferencial** (imagen + registros + ledger de efectos
off-image) contra el oráculo Musashi. Cobertura: **todo el juego** (33 módulos,
**518 pruebas**). Su revisión (P0/P1/P2) está **cerrada** y la base fue
**validada en Windows (2026-10-23)**.

**Congelada** como `core-base-20261023`. Cambios futuros al core exigen evidencia
ASM + test. Este repositorio contiene solo código y no produce un ejecutable
jugable.

## Reglas transversales

1. `kult-atari-st-decomp` **no se toca**.
2. `kult-atari-st-c` es la base; cualquier cambio en él debe dejar
   `make test`/`make regscan`/`make core` en verde y su prueba diferencial.
3. Este repo no incluye material del juego original.
