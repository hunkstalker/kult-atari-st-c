# ESTADO — reimplementación de KULT (Atari ST) en C

Documento de relevo: lo que hay hecho, cómo se ejecuta y qué queda. Para el
método y las convenciones, `docs/PLAN.md`; para el detalle por tandas,
`docs/LEGIBILIDAD.md`.

## Resumen

La reimplementación en C cubre **todo el juego**. El decomp canónico en ASM
(`/app/kult-atari-st-decomp`, byte a byte) es la especificación y **no se toca**;
este repo (`/app/kult-atari-st-c`) es la versión en C, fiel en **comportamiento**
y verificada con el **diferencial** contra el oráculo (Musashi).

- `src/`: **33 módulos**, ~12.460 líneas.
- Nombres: **417 funciones** + **221 globales** en `work/names.txt` (registro único).
- Pruebas: **499** diferenciales en verde, **0 warnings**.
- Legibilidad: **0** identificadores de registro/decompilador en código.
- API pública del core + visibilidad on-target cerradas.

## Cómo ejecutar

```sh
cd /app/kult-atari-st-c
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt   # una vez
make test                      # compila src/*.c y pasa el diferencial
make test PYTEST_ARGS=-n0      # serial (depuración)
make addrs                     # regenera include/addrs.h desde names.txt
make mapa                      # regenera docs/mapa/
make regscan                   # deuda de legibilidad por módulo (debe dar 0)
make core                      # build on-target con la API restringida
```

El arnés necesita `work/disk_a/KULT.PRG` (copia del usuario, gitignored); si
falta, copiarlo desde `/app/kult-port/work/disk_a/KULT.PRG`.

## Estructura de `src/`

Un fichero por subsistema; cada función lleva `/* 0xADDR — qué hace */` y su
nombre es el de `work/names.txt`:

| área | módulos |
|---|---|
| pantalla/vídeo | `screens`, `video`, `palette`, `sprite`, `maskblit`, `line`, `boxdraw`, `scale`, `reveal`, `overlay` |
| texto/UI | `text`, `textblit`, `textbox`, `textform`, `cursor` |
| escena/elementos | `element`, `anim`, `mode`, `icons`, `select` |
| bucle/VM | `loop`, `dispatch`, `vm`, `commands`, `handlers`, `state`, `misc` |
| E/S | `fileio` |
| sonido/música | `sound`, `music` |
| input/stubs | `input`, `delay`, `stubs` |

`include/addrs.h` es **generado** (`tools/gen_addrs.py`); `include/kult_internal.h`
tiene los helpers compartidos; `include/kult_api.h` + `kult_core.map` definen la
API pública. Las fichas por módulo (funciones, globales, pruebas) están en
`docs/mapa/` (generadas por `tools/gen_mapa.py`).

## Verificación

Cada `test/test_<módulo>.py` ejecuta el **original** en el oráculo y el candidato
(`build/libkult.so`) sobre la misma imagen, y compara memoria + registros + el
**ledger** de efectos off-image (paleta, PSG, …). Las esperas (VBL, IKBD) se
liberan con `schedule`/`wait_sites`. `test/test_conventions.py`,
`test/test_readability.py`, `test/test_mapa.py` y `test/test_visibility.py`
fijan las reglas del repositorio.

## Convenciones (resumen; detalle en `docs/PLAN.md` §4.1)

- Símbolo C = nombre de `names.txt`; comentario con la dirección original.
- Estado = imagen plana `uint8_t *img` + `A_*` de `addrs.h` (big-endian).
- Registros de salida devueltos o por out-param; la prueba los compara.
- Una prueba por entrada del decomp.

## Legibilidad

Tandas 1–4 completas (`docs/LEGIBILIDAD.md`): nombres semánticos en todo el
código; `ids_reg` (identificadores tipo registro en código) **8801 → 0**. Los
nombres de registro que quedan están en comentarios, documentando el ASM.

## Visibilidad on-target

`make core` construye `build/libkultcore.so` con `kult_core.map`: exporta **solo**
la API del motor (31 símbolos). El build diferencial (`libkult.so`) no usa el
mapa (el harness necesita `dlsym`). Detalle en `docs/VISIBILIDAD.md`.

## Pendiente / siguiente

1. **Revisión del C — CERRADA (P0 + P1 + P2).** Los 33 módulos están auditados
   contra el ASM con la checklist de `docs/REVISION.md` §3. Registro:
   - **P0** (§7): 9 módulos, 20 hallazgos (17 arreglados, 16 con prueba, 5
     intencionales); 0 ALTA/MEDIA abiertos.
   - **P1** (§9): 11 módulos, 10 hallazgos arreglados (ALTA/MEDIA con prueba) y
     5 intencionales.
   - **P2** (§11): 13 módulos; **H-MB1** (`hint_entry`, truncado `lsl.w`) y
     **H-OV1** (desplazamientos lógicos mod 64 del 68k) arreglados **con
     prueba**; H-SCR1/H-LN1/H-SP1/H-TX1..TX5 *intencionales* justificados
     (palabras altas residuales, `bgt`/V y `dbra` inalcanzables) y H-SCR2/H-SE1
     (BAJA) arreglados. **0 ALTA/MEDIA abiertos.**
   Resultado: **499 pruebas** verdes, `make regscan` `ids_reg=0`, `make core` OK,
   0 warnings.
2. **Siguiente: fase 3 del PLAN (revisión ya cerrada).** Reimplementar sobre
   esta base C limpia las modificaciones del proyecto previo, tomando
   **kult-port** (previsiblemente renombrado `kult-port-legacy`) como
   **referencia** (guardado por ranuras, textos ES, overlay F1, etc.), con plan
   detallado hecho al abrir la fase. Mientras tanto, kult-port queda **fuera de
   alcance** (no se toca).
- Trampas conocidas del oráculo/kit: `docs/REFERENCIA_F4.md` (sección final).
- Al auditar apareció una clase de fallo recurrente del import+renombrado:
  llamada a la **función hermana equivocada** (H-V1: `sound_play_b` por
  `sound_play_a`) y **condiciones invertidas** (H-C2, H-L1, H-L2, H-L3).

## Historial (hitos)

1. Setup del repo + arnés.
2. Piloto de paleta + convenciones (`names.txt` único, `gen_addrs.py`, guardián).
3. Escalado por subsistemas (pantalla, texto, escena, bucle/VM, sonido/música).
4. `docs/mapa/` (fichas) y visibilidad on-target.
5. Limpieza (warnings, helpers duplicados) y **legibilidad** (tandas 1–4).
6. **Revisión P0 del C** vs ASM (`docs/REVISION.md` §7): 20 hallazgos, 17
   arreglados (16 con prueba), 5 intencionales; 478 → 492 pruebas.
7. **Revisión P1 del C** (`docs/REVISION.md` §9): 11 módulos; 10 hallazgos
   arreglados (ALTA/MEDIA con prueba) y 5 intencionales; 492 → 494 pruebas.
8. **Revisión P2 del C** (`docs/REVISION.md` §11): 13 módulos; H-MB1 (truncado
   `lsl.w` de `hint_entry`) y H-OV1 (desplazamientos lógicos mod 64 del 68k)
   arreglados con prueba, 2 BAJA arreglados y 7 intencionales justificados;
   494 → 499 pruebas. **Revisión del C cerrada; se abre la fase 3.**
