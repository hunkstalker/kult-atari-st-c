# AGENTS.md — relevo de kult-atari-st-c

Documento de arranque para un agente que retoma el trabajo. Es el punto de
entrada; el detalle vive en `docs/`.

## Qué es este repo

Reimplementación en **C legible** de *Kult: The Temple of Flying Saucers*
(Atari ST, 1989), **fiel en comportamiento** al original.

- **Especificación congelada (no se toca):** `../kult-atari-st-decomp`
  (`matching/auto_*.s` por función, `*.s` manuales, `data_*.s`). Ensambla al
  original **byte a byte**.
- **Este repo:** el C, verificado con **diferencial** contra el oráculo (Musashi)
  del kit (`tools/recreate_kit`): compara memoria + registros + ledger de
  efectos off-image (paleta, PSG, IKBD…).
- Este repositorio contiene **solo código**; no produce un ejecutable jugable ni
  incluye material del juego original.

## Estado (a día de hoy)

- **Todo el juego portado:** 33 módulos, ~12.5k líneas, 518 pruebas
  diferenciales verdes, 0 warnings, `make regscan` `ids_reg=0`, `make core` OK.
- **Revisión del C: cerrada y validada.** P0 (§7), P1 (§9) y P2 (§11)
  auditados; 0 ALTA/MEDIA abiertos. Base validada en Windows (2026-10-23) y
  congelada como `core-base-20261023`. Registro completo en `docs/REVISION.md`.
- Detalle de estado y censo: `docs/ESTADO.md`.

## Método por función (resumen)

1. Abrir el `auto_*.s` de la función en
   `../kult-atari-st-decomp/matching/` (si no existe como `auto_`, buscar la
   etiqueta `f_XXXXXX` en `*.s` manuales) y comparar con el cuerpo C
   instrucción a instrucción: anchos `.b/.w/.l`, signo, banderas C/Z/N/V,
   pre/post-incremento, bucles `dbra` (¡do-while!), orden de escritura, colas
   `rts`/fall-through, registros de salida/`movem`, out-params y el ledger.
2. Registrar cada divergencia con la plantilla de `docs/REVISION.md` §5.
3. Si es un fallo real: **arreglarlo con su prueba diferencial**.
4. `make test` verde, `make regscan` `ids_reg=0`, `make core` OK.
5. Commit `revision(<módulo>): …` y `make mapa`.
6. Al cerrar una tanda: registro (§7/§9/§11), `docs/ESTADO.md`, `make mapa`.

Herramientas: `docs/mapa/<módulo>.md` (fichas), `make addrs` (si tocas
`names.txt`), `make mapa` (regenera fichas), `make regscan` (deuda de
legibilidad).

## Comandos

Desde la raíz del repo:

```sh
make test                      # ejecuta el diferencial (518)
make test PYTEST_ARGS=-n0      # serial (depuración)
make regscan                   # debe dar ids_reg=0
make core                      # build on-target con la API restringida
make mapa / make addrs         # regenerar fichas / direcciones
```

## Reglas de trabajo

- **Reproducir/medir antes de tocar.** Una hipótesis, un cambio.
- **Toda corrección deja su prueba diferencial** (salvo divergencia inobservable,
  que se marca *intencional* con justificación en el registro).
- Commit por módulo; no tocar `../kult-atari-st-decomp`.
- `make regscan` debe seguir en `ids_reg=0`. **Todo en español.**

## Documentos clave

| archivo | para qué |
|---|---|
| `docs/PROYECTOS.md` | relación entre este repo y el decomp |
| `docs/ESTADO.md` | estado exacto, censo, cómo continuar |
| `docs/REVISION.md` | plan de revisión y **registro** (§7/§9/§11) |
| `docs/PLAN.md` | plan maestro y convenciones (§4.1) |
| `docs/mapa/<módulo>.md` | ficha por módulo (funciones, globales, pruebas) |
| `docs/LEGIBILIDAD.md` | tandas de legibilidad (histórico) |
| `docs/VISIBILIDAD.md` | API pública on-target (`kult_core.map`) |
