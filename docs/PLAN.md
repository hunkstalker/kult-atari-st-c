# PLAN — Reimplementación de Kult (Atari ST) en C legible

Documento maestro de **este** repo (`/app/kult-atari-st-c`). Para retomar en una
sesión nueva, ver `docs/PROMPT_SESION.md`.

## 0. Resumen

Cuatro repos, con papeles distintos (fuente canónica: **`docs/PROYECTOS.md`**):

| repo | papel | garantía |
|---|---|---|
| `/app/kult-atari-st-decomp` (A) | decomp canónico **en ensamblador** | **byte a byte** con el original |
| `/app/kult-atari-st-c` (B, este) | **reimplementación en C** legible (base) | **mismo comportamiento** |
| `/app/kult-port-pc-compilation` | **la compilación** (Atari ST + Amiga, idiomas, mejoras) | por capas sobre el core verificado |
| `/app/kult-port-legacy` | predecesor de modding (referencia) | solo referencia |

- **A no se toca**: es la verdad/especificación. Su `matching/` reproduce el PRG
  original byte a byte (sha256 `2e85f223…`).
- **B es el trabajo**: escribir, desde A, un C moderno, legible, documentado y
  con **todo identificado**, fiel en comportamiento. Plan de la compilación que
  nace de B: **`docs/PLAN_COMPILACION.md`**.
- A se identificará **de paso** en B (nombres y documentación viven aquí); A se
  queda como referencia congelada.

## 1. Por qué este esquema (contexto)

- El motor original es **ensamblador a mano**. No existe ningún C que, compilado,
  dé los mismos bytes (a diferencia de SM64, cuyo original era C). Por eso el
  matching byte a byte solo es posible en ASM → vive en A.
- B es una **reimplementación** (port nativo), como hacen con Pokémon Rojo:
  `pret/pokered` es el ASM canónico y `pokeredpc`/`PokeSwift` son
  reimplementaciones en lenguajes modernos que usan el ASM como especificación.
- Verificación de B: **por comportamiento**, con el oráculo (emulador) y pruebas,
  no comparando bytes compilados.

## 2. Materiales

- **Especificación (ASM):** `/app/kult-atari-st-decomp/matching/`
  (`auto_*.s` por función, `*.s` manuales, `data_*.s` de tablas). Ensambla byte a
  byte con `vasm`.
- **Índice y nombres:** `/app/kult-atari-st-decomp/work/decomp.c` (índice de
  funciones) y `work/names.txt` (177 nombres ya puestos).
- **Original:** `work/disk_a/KULT.PRG` (tu copia; gitignored).
- **Análisis de comportamiento:** `docs/REFERENCIA_F4.md` (copiado del intento
  anterior; incluye "trampas" y fórmulas ya deducidas). **Referencia**, no se
  copia código.
- **Verificación:** `tools/recreate_kit/` (arnés diferencial + oráculo Musashi).
- **Pruebas previas:** `reference/test_f4/` (469 pruebas del intento anterior).
  Atadas a los nombres viejos → **referencia de comportamiento**, se reescriben.

## 3. Método (por cada pieza)

1. **Localizar** la pieza en A (`matching/`): dirección y qué hace.
2. **Escribir C legible** en `src/`:
   - nombre descriptivo (p. ej. `palette_set_entry`);
   - comentario de cabecera con la **dirección original** (`/* 0x162e2 — ... */`);
   - tipos y nombres con sentido (nada de `g_fun_xxx` ni offsets crípticos).
3. **Escribir su prueba diferencial** en `test/test_*.py`: ejecuta el original en
   el oráculo y compara con tu C (memoria y registros).
4. **`make test` verde.**
5. **Actualizar el mapa** de módulos (abajo) y la documentación del módulo.

Regla de oro: una pieza está hecha solo si su prueba diferencial pasa.

## 4. Convenciones (a fijar en el piloto)

- **Módulos por subsistema** (un `.c` por área: `palette.c`, `cursor.c`,
  `text.c`, `sound.c`, `rooms.c`, `loop.c`, …).
- **Nombres:** `snake_case` descriptivo; comentario con la dirección original en
  cada función y en cada global.
- **Globals:** `work/names.txt` es la **única fuente**; `include/addrs.h` se
  **genera** con `tools/gen_addrs.py` (`make addrs`). Cada `var 0xADDR nombre`
  produce `#define A_NOMBRE 0xADDR`. `test/test_conventions.py` falla si el
  fichero no coincide (y prohíbe nombres duplicados y funciones sin nombre).
- **Estado del juego:** decidir en el piloto entre (a) **imagen plana + vistas
  tipadas documentadas** (más directo, cercano al original) o (b) **structs
  tipados** (más legible). Propuesta: (a), documentando cada campo.
- **Documentación:** ficha por módulo (qué hace, qué funciones, qué estado toca,
  qué pruebas) + un mapa de módulos en `docs/mapa/`. Se **generan** con
  `tools/gen_mapa.py` (`make mapa`) y `test/test_mapa.py` los mantiene
  sincronizados.
- **Identificación total:** cada parte del juego debe poder localizarse por su
  nombre y su procedencia.
- **Legibilidad (port):** el C se moderniza a estándar de *port* (nombres
  semánticos, control de flujo legible) sin cambiar comportamiento; la traza al
  ASM vive en `names.txt` + el comentario `/* 0xADDR */` + el decomp congelado.
  Plan por módulos y seguimiento: **`docs/LEGIBILIDAD.md`** (`tools/regscan.py`).

### 4.1 Convenciones confirmadas en el piloto de la paleta (2026-10-17)

- **Nombres:** el símbolo C es **exactamente el nombre de `work/names.txt`**
  (p. ej. `set_palette_entry`, `xbios_setpalette`, `set_palette_current`). Así la
  búsqueda por nombre sirve en los dos repos y `names.txt` es la única fuente de
  nombres; el `.c` agrupa por subsistema. (Se descartó el prefijo de módulo
  `palette_set_entry`.) Si una pieza no tiene nombre, se añade a `names.txt`.
- **Comentarios:** cabecera de fichero con el subsistema, el estado que toca y la
  cadena de control real; cada función con `/* 0xDIR — qué hace */`, citando los
  registros que usa.
- **Estado del juego:** (a) **imagen plana** `uint8_t *img` + direcciones con
  nombre en `include/addrs.h` (`A_*`); acceso big-endian con
  `be16/be32/wr16/wr32`. Cada campo se documenta en su módulo y en `addrs.h`.
- **Registros:** los de entrada van en `regs={"d0":…,"a6":…}` (el glue pasa el
  mismo valor al candidato); los de salida que importan **se devuelven** (p. ej.
  el A6 que avanza el guion) y la prueba los compara con `info["regs"]`.
- **Prueba:** `test/test_<módulo>.py`, una prueba por entrada del decomp
  (`/* 0x… */`); se comparan imagen, registros y el ledger de eventos
  off-image (p. ej. `harness.OS_EVENT_SETPALETTE` para XBIOS Setpalette).

## 5. Verificación

- `make test` compila `src/*.c` a un `.so` y lanza las pruebas diferenciales.
- El arnés (kit) ejecuta el **original** en el oráculo Musashi con la misma
  entrada y compara **imagen de memoria + registros**; los efectos de hardware
  (paleta, PSG, …) van a un "ledger" de eventos que también se compara.
- Baseline: las 469 pruebas del intento anterior pasan con el C viejo (verificado
  al montar este repo). La suite nueva tiene **518** pruebas verdes (478 al
  terminar el portado; crecieron con la revisión del C, `docs/REVISION.md`).

## 6. Piloto: PALETA (HECHO)

Objetivo cumplido: **acordar el estilo** (nombres, comentarios, cómo se declara
el estado, cómo se escribe la prueba) antes de escalar. Resultado:
`src/palette.c` + `test/test_palette.py` verdes, y las convenciones fijadas en
§4.1.

Piezas implicadas (en A `matching/`):

- `set_palette` (`0x135da`) → `bsr` a `0x16300`. Punto de entrada público.
- `0x16300`: XBIOS **Setpalette** (trap #14, función 6) con `A_PALETTE_PTR`.
  Efecto de **hardware** → se comprueba por el ledger del oráculo.
- `0x162e2`: fija una **entrada de paleta**: `A_PAL_TABLE + d0*0xa` → 5 words a
  `A_PALETTE_PTR+2`. Efecto **en RAM** (imagen).
- `0x162d8`: `addq.l #1, a6` (avanza el guion).

## 7. Fases

0. **Setup** — HECHO (este repo, arnés, docs).
1. **Piloto paleta** — HECHO (estilo acordado, §4.1).
2. **Escalar por subsistemas** — HECHO: **todo el juego** portado (33 módulos,
   423 símbolos de función en `names.txt`, 422 definiciones en `src/`; hoy 518
   pruebas). El orden tentativo (paleta/cursor → input → vídeo/blit → texto/UI →
   animación → salas/transiciones → VM/guion → bucle → sonido → música) se
   cubrió; el detalle de tandas está en `docs/LEGIBILIDAD.md`.
   - **Revisión del C** — HECHA: `docs/REVISION.md` (§7 P0, §9 P1, §11 P2;
     0 ALTA/MEDIA abiertos).
3. **La compilación** — proyecto `kult-port-pc-compilation`, que **nace de este
   C** y **une Atari ST + Amiga**, con mejoras y **todos los idiomas**.
   `kult-port-legacy` (ex `kult-port`) queda como **referencia** (guardado por
   ranuras, textos ES, overlay F1, etc.). Plan definitivo:
   **`docs/PLAN_COMPILACION.md`**.

## 8. Legal

El juego sigue bajo copyright (Piko Interactive). No se distribuyen assets ni
binarios; el usuario aporta su copia. Lo derivado del juego está gitignored.
