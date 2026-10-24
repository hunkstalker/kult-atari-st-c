# kult-atari-st-c — reimplementación de KULT (Atari ST) en C moderno

Este repositorio es la **reimplementación en C legible y documentado** del juego
*Kult: The Temple of Flying Saucers* (Exxos/ERE, 1989), hecha **a partir del
ensamblador** del decomp canónico.

## Relación con el decomp

Este repositorio parte del decomp canónico en ensamblador,
[`kult-atari-st-decomp`](https://github.com/hunkstalker/kult-atari-st-decomp).
El ASM es la **verdad/especificación**. Este C **no** es byte a byte (el original
fue escrito en ensamblador; ningún C da esos bytes), pero **hace lo mismo**, y eso
se comprueba con pruebas.

Este repo contiene **solo el código del core y sus pruebas**; no produce un
ejecutable jugable ni incluye material del juego original.

## Objetivo de este repo

Un **C moderno, legible, escrito a mano**, donde **cada pieza está identificada**
(nombre con sentido + comentario con su dirección original), **bien documentada**,
para que modificar el juego sea localizar la pieza y tocarla.

- Plan y método: **`docs/PLAN.md`**.
- Cómo retomar en una sesión nueva: **`AGENTS.md`**.

## Estructura

```
Makefile              compilación del candidato para pruebas + `make addrs`/`mapa`/`core`
project.toml          datos del proyecto para el arnés (nombres, base)
include/addrs.h       direcciones con nombre de los globales (GENERADO desde names.txt)
include/kult_api.h    API pública del core (frontera con el motor)
kult_core.map         version-script: exporta solo la API en el build on-target
src/                  C de la reimplementación (un módulo por subsistema)
test/                 pruebas diferenciales (harness.py + test_*.py) + convenciones
tools/recreate_kit/   arnés + oráculo (Musashi); GPL-2.0
tools/gen_addrs.py    genera include/addrs.h desde symbols/names.txt
tools/gen_mapa.py     genera docs/mapa/ desde src/, names.txt y test/
tools/regscan.py      mide la deuda de legibilidad por módulo (docs/LEGIBILIDAD.md)
symbols/              names.txt (registro de nombres)
docs/                 PROYECTOS, ESTADO, PLAN, REVISION, LEGIBILIDAD,
                      VISIBILIDAD, mapa/
```

## Estado

**Completado**: la reimplementación cubre **todo el juego**.

- `src/`: **33 módulos** (~12.620 líneas de C), **423 funciones** identificadas
  (0 placeholders) y **221 globales** con nombre.
- `test/`: **518 pruebas diferenciales** verdes contra el oráculo (Musashi),
  **0 warnings** de compilación.
- Convenciones, registro único de nombres (`names.txt`), generación de
  `addrs.h`/`docs/mapa/`, API/visibilidad on-target y **legibilidad** (0
  identificadores de registro/decompilador en código) — todo cerrado.

- Estado detallado y cómo continuar: **`docs/ESTADO.md`**.
- Plan y método: `docs/PLAN.md` · legibilidad: `docs/LEGIBILIDAD.md` ·
  visibilidad on-target: `docs/VISIBILIDAD.md`.
- La revisión del C está cerrada y la base fue **validada en Windows
  (2026-10-23)**; queda **congelada** como `core-base-20261023`
  (`docs/REVISION.md` §7/§9/§11).

## Cómo se verifica

```sh
cd kult-atari-st-c
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt   # una vez
make test            # ejecuta el diferencial contra el oráculo
```

El arnés ejecuta el **original** en el oráculo (Musashi) y compara con tu C
(memoria y registros). Si coincide, la pieza es fiel.

> **Windows:** el arnés y `make test` están pensados para Linux (o WSL). El kit
> usa `.venv/bin/python`; en Windows nativo la ruta sería
> `.venv\Scripts\python.exe` y el flujo no está soportado tal cual. Usa
> **WSL** y crea el venv dentro de WSL.

## Licencia

El código propio de este repositorio se distribuye bajo la licencia **MIT**
(ver [`LICENSE`](LICENSE)). El kit de verificación
[`tools/recreate_kit`](tools/recreate_kit) se mantiene bajo **GPL-2.0**; los
detalles están en [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) y
[`LICENSES/GPL-2.0.txt`](LICENSES/GPL-2.0.txt).

El juego original sigue bajo copyright de sus respectivos titulares. Este
repositorio contiene únicamente código y no incluye material del juego.
