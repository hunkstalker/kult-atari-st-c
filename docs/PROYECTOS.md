# PROYECTOS — topología de repos de Kult (Atari ST / Amiga)

Documento **canónico**: cómo se reparte el trabajo de Kult entre repositorios,
qué papel tiene cada uno y qué reglas aplican. Si algo de otro documento
contradice a este, manda este.

## Los cuatro repos

| repo | papel | garantía | regla |
|---|---|---|---|
| `/app/kult-atari-st-decomp` | decomp **ASM** del original Atari ST | **byte a byte** (`sha256 2e85f223…`) | **congelado**; no se toca |
| `/app/kult-atari-st-c` | **reimplementación en C** del ST, legible y renombrada | **mismo comportamiento** (diferencial Musashi) | es la **base**; revisión interna cerrada, **pendiente de validación del usuario** |
| `/app/kult-port-pc-compilation` (a crear) | **la compilación**: une **Atari ST + Amiga**, mejoras y **todos los idiomas** | por capas: core verificado + backends | **nace de `kult-atari-st-c`** |
| `/app/kult-port-legacy` | predecesor de modding | — | **solo referencia**; no se toca |

## Papeles en detalle

### `kult-atari-st-decomp` — especificación Atari ST
El decomp en ensamblador, `matching/` reproduce el `KULT.PRG` original byte a
byte. Es la **verdad/especificación** y está **congelado**: no se modifica.

### `kult-atari-st-c` — el C verificado del ST
La reimplementación en C moderno, legible y documentado, fiel en comportamiento
y verificada con el **diferencial** (imagen + registros + ledger de efectos
off-image) contra el oráculo Musashi. Cobertura: **todo el juego** (33 módulos,
499 pruebas). Su revisión (P0/P1/P2) está **cerrada internamente**; queda
**pendiente la validación del usuario** (preguntas y verificaciones).

**No se congela**: el código probablemente ya no se toque, pero sigue siendo el
repo de trabajo hasta que el usuario valide. Es la **base** de la compilación.

### `kult-port-pc-compilation` — la compilación (proyecto nuevo)
El objetivo de producto: un Kult **nativo en C para PC** que **una las versiones
Atari ST y Amiga**, con mejoras y **todos los idiomas**. **Nace de
`kult-atari-st-c`** (se copian `src/`, `include/`, `test/`, `tools/` al crear el
repo) y toma `kult-port-legacy` como **referencia**. Plan: **`docs/PLAN_COMPILACION.md`**.

### `kult-port-legacy` — referencia (ex `kult-port`)
Predecesor de modding, **renombrado** de `kult-port` a `kult-port-legacy`. Se
conserva **por sus mejoras y arreglos** para la compilación Atari/Amiga (idiomas,
overlay F1, guardado por ranuras, frontend SDL2, filtros…) como **inventario de
comportamiento**; **no** es código a portar. El trabajo nuevo no vive aquí.

## Terminología

- «**la compilación**» = el proyecto `kult-port-pc-compilation`.
- «**el core**» = la lógica del juego en C; hoy el de `kult-atari-st-c` (ST).
- «**perfil**» = preset de comportamiento (Atari ST / Amiga / Custom); ver
  `docs/PLAN_COMPILACION.md` §3.
- «**spec**» = el decomp ASM byte a byte de una versión; la garantía de fidelidad
  sale de tener una spec **por versión** (Atari ST ya; Amiga pendiente).

## Reglas transversales

1. `kult-atari-st-decomp` y `kult-port-legacy` **no se tocan**.
2. `kult-atari-st-c` es la base; cualquier cambio en él debe dejar
   `make test`/`make regscan`/`make core` en verde y su prueba diferencial.
3. La compilación **no copia código** de `kult-port-legacy`: reimplementa sobre
   el core verificado, usando el legacy solo como referencia de features.
4. Todo lo derivado del juego (assets, binarios, `work/`) queda **gitignored**;
   el usuario aporta su copia.
