# kult-atari-st-c — reimplementación de KULT (Atari ST) en C moderno

Este repositorio es la **reimplementación en C legible y documentado** del juego
*Kult: The Temple of Flying Saucers* (Exxos/ERE, 1989), hecha **a partir del
ensamblador** del decomp canónico.

## Los dos proyectos (no confundir)

| repo | contenido | garantía |
|---|---|---|
| **`/app/kult-atari-st-decomp`** | el decomp **en ensamblador** (ASM) | **byte a byte** idéntico al original |
| **este repo** (`kult-atari-st-c`) | **reimplementación en C** legible | **mismo comportamiento** (verificado en el emulador) |

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
docs/                 plan, prompt de sesión, referencia, mapa/, VISIBILIDAD
```

## Estado

- Setup **HECHO**: arnés funcionando (verificado: 469 pruebas del intento
  anterior pasan con el C viejo). `src/` **vacío**: la reimplementación empieza
  de cero.
- **Primera tarea:** el **piloto de la paleta** (ver `docs/PLAN.md` §6).

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
