# kult-atari-st-c — reimplementación de KULT (Atari ST) en C moderno

Este repositorio es la **reimplementación en C legible y documentado** del juego
*Kult: The Temple of Flying Saucers* (Exxos/ERE, 1989), hecha **a partir del
ensamblador** del decomp canónico.

## Los proyectos (no confundir)

El trabajo se reparte en **cuatro** repos; la fuente canónica es
**`docs/PROYECTOS.md`**:

| repo | contenido | garantía |
|---|---|---|
| **`/app/kult-atari-st-decomp`** | decomp **en ensamblador** (ASM) del ST | **byte a byte** idéntico al original |
| **este repo** (`kult-atari-st-c`) | **reimplementación en C** legible del ST | **mismo comportamiento** (verificado en el emulador) |
| **`/app/kult-port-pc-compilation`** | **la compilación**: Atari ST + Amiga, mejoras y todos los idiomas | por capas sobre el core verificado |
| **`/app/kult-port-legacy`** | predecesor de modding (referencia) | solo referencia |

- El ASM del decomp es la **verdad/especificación**. Este C **no** es byte a byte
  (el original fue escrito en ensamblador; ningún C da esos bytes) pero **hace lo
  mismo**, y eso se comprueba con pruebas.
- Modelo imitado: `pret/pokered` (decomp ASM) + `pokeredpc`/`PokeSwift`
  (reimplementación en lenguaje moderno que usa el ASM como especificación).

## Objetivo de este repo

Un **C moderno, legible, escrito a mano**, donde **cada pieza está identificada**
(nombre con sentido + comentario con su dirección original), **bien documentada**,
para que modificar el juego sea localizar la pieza y tocarla.

- Plan y método: **`docs/PLAN.md`**.
- Cómo retomar en una sesión nueva: **`docs/PROMPT_SESION.md`**.
- Análisis de comportamiento del original (de la etapa previa): `docs/REFERENCIA_F4.md`.

## Estructura

```
Makefile              build del candidato + `make test` (diferencial) + `make addrs`/`mapa`/`core`
project.toml          datos del proyecto para el arnés (PRG, nombres, base)
include/addrs.h       direcciones con nombre de los globales (GENERADO desde names.txt)
include/kult_api.h    API pública del core (frontera con el motor)
kult_core.map         version-script: exporta solo la API en el build on-target
src/                  C de la reimplementación (un módulo por subsistema)
test/                 pruebas diferenciales (harness.py + test_*.py) + convenciones
tools/recreate_kit/   arnés + oráculo (Musashi); GPL-2.0
tools/gen_addrs.py    genera include/addrs.h desde work/names.txt
tools/gen_mapa.py     genera docs/mapa/ desde src/, names.txt y test/
tools/regscan.py      mide la deuda de legibilidad por módulo (docs/LEGIBILIDAD.md)
reference/test_f4/    469 pruebas del intento anterior (referencia, no se ejecutan)
work/                 PRG original (gitignored) + names.txt (registro de nombres)
docs/                 PROYECTOS, PLAN_COMPILACION, ESTADO, PLAN, PROMPT_SESION,
                      REVISION, REFERENCIA_F4, LEGIBILIDAD, VISIBILIDAD, mapa/
```

## Estado

**Completado**: la reimplementación cubre **todo el juego**.

- `src/`: **33 módulos** (~12.400 líneas de C), **417 funciones** identificadas
  (0 placeholders) y **221 globales** con nombre.
- `test/`: **499 pruebas diferenciales** verdes contra el oráculo (Musashi),
  **0 warnings** de compilación.
- Convenciones, registro único de nombres (`names.txt`), generación de
  `addrs.h`/`docs/mapa/`, API/visibilidad on-target y **legibilidad** (0
  identificadores de registro/decompilador en código) — todo cerrado.

- Estado detallado y cómo continuar: **`docs/ESTADO.md`**.
- Plan y método: `docs/PLAN.md` · legibilidad: `docs/LEGIBILIDAD.md` ·
  visibilidad on-target: `docs/VISIBILIDAD.md`.
- **Siguiente:** **la compilación** (`kult-port-pc-compilation`), que nace de
  este core y une **Atari ST + Amiga**, con mejoras y todos los idiomas. Plan:
  **`docs/PLAN_COMPILACION.md`**. La revisión del C de este repo está cerrada
  internamente (`docs/REVISION.md` §7/§9/§11), **pendiente de validación del
  usuario**.

## Cómo se verifica

```sh
cd /app/kult-atari-st-c
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt   # una vez
make test            # compila src/*.c y pasa el diferencial contra el oráculo
```

El arnés ejecuta el **original** en el oráculo (Musashi) y compara con tu C
(memoria y registros). Si coincide, la pieza es fiel.

## Legal

El juego sigue bajo copyright (Piko Interactive). **No** se distribuyen assets ni
binarios; el usuario aporta su copia. Todo lo derivado (`work/disk_a`, el PRG)
está en `.gitignore`.
