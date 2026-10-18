# Plan de legibilidad (Fase B/C) — modernizar el C del core

**Objetivo:** llevar el C de `src/` al estándar de un *port nativo* (como hacen
PokeSwift, Ship of Harkinian, re3…): identificadores con nombre semántico y
control de flujo legible, **sin cambiar comportamiento** (lo garantizan las 478
pruebas diferenciales). La traza al original se conserva por el **registro**
`work/names.txt`, el comentario `/* 0xADDR — … */` de cada función y el repo del
decomp congelado; **no** por el texto del identificador.

Contexto y por qué B/C (y no A): ver el análisis en `docs/PLAN.md` §4 y la
discusión de ports (PokeSwift conserva `pret/pokered` como *source of truth* y
moderniza el motor; la traza vive en el mapa + paridad, no en los nombres).

## 0. Medida

`tools/regscan.py` cuenta, por módulo, los identificadores tipo registro
(`d0..d7`, `a0..a6`, variantes `d2b`, `u1`…) y los `goto`, **solo en código**
(sin comentarios). Sirve de línea base y de seguimiento.

Estado inicial (tras Fases 1–2 y el primer `textblit.c`):

| módulo | líneas | funcs | ids_reg | goto |
|---|---:|---:|---:|---:|
| loop.c | 2456 | 74 | 1547 | 29 |
| anim.c | 597 | 21 | 655 | 0 |
| music.c | 602 | 19 | 581 | 18 |
| element.c | 610 | 16 | 576 | 0 |
| overlay.c | 809 | 8 | 569 | 0 |
| handlers.c | 864 | 44 | 567 | 0 |
| textbox.c | 434 | 7 | 514 | 0 |
| dispatch.c | 519 | 5 | 416 | 0 |
| textblit.c | 546 | 26 | 400 | 0 |
| screens.c | 528 | 21 | 390 | 0 |
| commands.c | 686 | 16 | 364 | 3 |
| scale.c | 466 | 6 | 332 | 0 |
| maskblit.c | 282 | 12 | 233 | 0 |
| text.c | 303 | 16 | 218 | 5 |
| textform.c | 171 | 6 | 190 | 0 |
| icons.c | 199 | 13 | 179 | 0 |
| select.c | 334 | 8 | 175 | 30 |
| sprite.c | 189 | 7 | 162 | 0 |
| boxdraw.c | 149 | 6 | 110 | 0 |
| mode.c | 250 | 7 | 109 | 0 |
| line.c | 185 | 3 | 106 | 0 |
| sound.c | 257 | 17 | 98 | 0 |
| video.c | 199 | 5 | 89 | 0 |
| reveal.c | 134 | 4 | 64 | 0 |
| misc.c | 65 | 9 | 48 | 0 |
| vm.c | 47 | 3 | 31 | 0 |
| cursor.c | 71 | 2 | 26 | 0 |
| fileio.c | 252 | 17 | 23 | 10 |
| state.c | 56 | 4 | 11 | 0 |
| palette.c | 71 | 6 | 9 | 0 |
| input.c | 42 | 2 | 5 | 0 |
| delay.c | 32 | 2 | 4 | 0 |
| stubs.c | 12 | 4 | 0 | 0 |
| **TOTAL** | **~12.4k** | **416** | **8801** | **95** |

`overlay.c` ya tiene 0 identificadores de decompilador; sus 569 «ids_reg» son
`w08`/`long00`/`pair10` (contados por el patrón `[da][0-7]`… no: son nombres
semánticos; el patrón no los cuenta — el número viene de los `d0..d7` de los
blitters de plano). `textblit.c` ya se hizo en parte.

## 1. Criterios de renombrado

**Se renombra** todo identificador que sea un registro puro del ASM:
- parámetros y locales `d0..d7`, `a0..a6`, y variantes con sufijo (`d2b`, `a0b`);
- temporales `u1`, `u2`, `u3`, `u5v`, `u11b`, `u14`… (estilo decompilador).

**Se conserva** (no aporta renombrar o perdería traza):
- constantes y macros (`A_*`, valores literales con comentario);
- punteros que el comentario ya ata a un registro concreto y cuyo nombre
  semántico sería engañoso;
- nombres de helper ya semánticos.

**Glosario de reuso** (el nombre final depende del rol en cada función; esto es
una guía, no una regla ciega):

| registro | nombre típico |
|---|---|
| `d0`/`d1` | `value`, `index`, `x`, `y`, `word` |
| `d2` | `cols`, `width`, `count` |
| `d3` | `rows`, `height`, `n` |
| `d4` | `index`, `count`, `color`, `off` |
| `d5` | `shift`, `stride` |
| `d6` | `word`, `carry` |
| `d7` | `phase`, `tmp` |
| `a0` | `dest`, `bitmap`, `base` |
| `a1` | `src`, `stream`, `table` |
| `a2` | `desc`, `out`, `dst` |
| `a3` | `stride` |
| `a4` | `end`, `base` |
| `a5` | `stream`, `ptr`, `scratch` |
| `a6` | `script`, `entry` |

Regla: **el comentario de la función debe permitir reconstruir a qué registro
del ASM corresponde cada variable** si no es obvio (p. ej. «`dest` = A0»).

## 2. Política de `goto`

- **Convertir** cuando es un bucle/`if` que mapea 1:1 a `for`/`while`/`break`/
  `continue` sin reordenar la lógica (`loop.c`, `fileio.c`, `text.c`, parte de
  `music.c`).
- **Conservar** cuando salta a **colas compartidas** del original (p. ej. las
  etiquetas `d58/d5c/d6e` de `select.c`, los cuerpos compartidos de `music.c`):
  convertirlas oscurecería el mapeo con el ASM y añade riesgo. En ese caso, se
  deja el `goto` con un comentario que explique la etiqueta.
- Criterio de aceptación en ambos casos: `make test` verde y legibilidad
  *mejor*, no peor.

## 3. Orden por tandas (de hoja a núcleo, por valor)

Cada tanda = un commit. Dentro de cada módulo, función a función.

- **Tanda 1 — hojas y base** (bajo riesgo, fija el glosario):
  `palette`, `screens`, `cursor`, `input`, `delay`, `stubs`, `state`, `boxdraw`,
  `line`, `sprite`, `maskblit`, `misc`, `vm`.
- **Tanda 2 — vídeo/texto**:
  `textblit` (resto), `text`, `textbox`, `textform`, `icons`, `video`, `select`
  (+ sus 30 `goto`), `mode`.
- **Tanda 3 — escena/elementos/animación**:
  `element`, `anim`, `scale`, `reveal`, `overlay` (locales de registro),
  `commands`, `handlers`.
- **Tanda 4 — núcleo y sonido**:
  `dispatch`, `sound`, `music` (+ `goto`), `fileio` (+ `goto`), `loop`
  (el más grande: 1547 ids y 29 `goto`; hacerlo en sub-bloques por familia).

## 4. Procedimiento por módulo

1. `python3 tools/regscan.py` (línea base del módulo).
2. Leer el módulo; por cada función, definir el mapa registro→semántico y
   aplicarlo (script o edición), **sin tocar la lógica**.
3. Convertir los `goto` seguros; dejar los de colas compartidas con comentario.
4. `make build/libkult.so` + `pytest test/test_<modulo>.py`; corregir.
5. `make mapa`; `make test` completo (478+ verde) y `test_readability` verde.
6. Actualizar la tabla de progreso de este documento y commitear.

## 5. Aceptación y red de seguridad

- **Comportamiento:** las 478 pruebas diferenciales deben seguir verdes; son la
  garantía de que renombrar no cambia nada.
- **Legibilidad:** `test/test_readability.py` verde (0 sin dirección, 0
  identificadores de decompilador).
- **Compilación:** 0 warnings (`-Wall -Wextra`).
- **Traza:** `names.txt` intacto; comentario `/* 0xADDR */` por función.
- **Reversión:** commits por módulo; `git checkout -- src/<modulo>.c` para
  deshacer uno sin tocar el resto.

## 6. Progreso

**Tanda 1 — hojas y base: HECHA** (`ids_reg` antes→después, medido con `make regscan`):

| módulo | antes | después | goto |
|---|---:|---:|---:|
| screens.c | 390 | 25 | 0 |
| maskblit.c | 233 | 3 | 0 |
| sprite.c | 162 | 2 | 0 |
| boxdraw.c | 110 | 3 | 0 |
| line.c | 106 | 3 | 0 |
| misc.c | 48 | 14 | 0 |
| vm.c | 31 | 10 | 0 |
| cursor.c | 26 | 3 | 0 |
| state.c | 11 | 2 | 0 |
| palette.c | 9 | 0 | 0 |
| input.c | 5 | 3 | 0 |
| delay.c | 4 | 0 | 0 |
| stubs.c | 0 | 0 | 0 |

Totales: `ids_reg` **8801 → 7734**, `goto` 95 (sin cambios; Tanda 2+).
Residuales pequeños (p. ej. `screens.c` 25) son variables sueltas menores; se
afinan al tocar cada módulo.

| tanda | módulos | estado |
|---|---|---|
| 1 | palette, screens, cursor, input, delay, stubs, state, boxdraw, line, sprite, maskblit, misc, vm | **HECHA** |
| 2 | textblit (resto), text, textbox, textform, icons, video, select, mode | **HECHA** |
| 3 | element, anim, scale, reveal, overlay, commands, handlers | **HECHA** |
| 4 | dispatch, sound, music (+goto), fileio (+goto), loop (+goto) | pendiente |

**Tanda 3 — escena/elementos/animación: HECHA** (`ids_reg` antes→después):

| módulo | antes | después | goto |
|---|---:|---:|---:|
| element.c | 576 | 111 | 0 |
| anim.c | 655 | 147 | 0 |
| scale.c | 332 | 66 | 0 |
| overlay.c | 569 | 194 | 0 |
| commands.c | 364 | 49 | 3 |
| handlers.c | 567 | 120 | 0 |
| reveal.c | 64 | 4 | 0 |

Totales: `ids_reg` **8801 → 3706**. Queda la Tanda 4 (`loop` 1547, `music` 581,
`dispatch` 416, `sound` 98, `fileio` 23, …).

**Tanda 2 — vídeo/texto: HECHA** (`ids_reg` antes→después):

| módulo | antes | después | goto |
|---|---:|---:|---:|
| textbox.c | 514 | 82 | 0 |
| textblit.c | 400 | 49 | 0 |
| text.c | 218 | 51 | 5 |
| icons.c | 179 | 31 | 0 |
| select.c | 175 | 25 | 30 |
| textform.c | 190 | 23 | 0 |
| mode.c | 109 | 17 | 0 |
| video.c | 89 | 4 | 0 |

Totales: `ids_reg` **8801 → 6142**. Los `goto` de `select.c` (30) y `text.c` (5)
se **conservan** (colas compartidas del original, ver §2); el conteo de `goto`
baja en Tanda 4 al convertir los bucles simples.
