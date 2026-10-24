# Visibilidad del core on-target

**Decisión:** el core reconstruido expone al motor **solo un conjunto reducido de
símbolos** (la *API del core*), y oculta el resto mediante un **version-script**
del enlazador. Así, al enlazar el core con SDL/otras librerías no colisiona
ninguno de sus ~400 símbolos internos (`copy_screen`, `set_palette`, `attn_*`,
`draw_*`, …), que son nombres genéricos heredados del original.

## Por qué no un prefijo de módulo

Renombrar los símbolos a `kult_*` en `src/` habría roto la correspondencia 1:1
con `symbols/names.txt` (que es el registro de nombres y la fuente de `addrs.h`) y
habría que tocarlos todos otra vez. La visibilidad se resuelve en la **frontera
de enlace**, no en el nombre.

## Mecanismo

- `kult_core.map` — version-script anónimo: `global:` lista la API y
  `local: *;` oculta el resto.
- `include/kult_api.h` — cabecera que ve el motor: declara la API y define
  `KULT_API` (`__attribute__((visibility("default")))`, `__declspec(dllexport)`
  en Windows).
- `make core` — construye `build/libkultcore.so` con
  `-Wl,--version-script=kult_core.map`.
- `test/test_visibility.py` — comprueba con `nm -D --defined-only` que el `.so`
  exporta **exactamente** la lista del mapa (ni uno más, ni uno menos).

Verificado: `build/libkultcore.so` exporta **31** símbolos (los del mapa).

## El build diferencial NO se toca

`build/libkult.so` (el que carga el harness) se construye **sin** el
version-script: `differential()` localiza los símbolos internos por `dlsym`, así
que deben seguir visibles. La restricción es **solo** del build on-target.

## Alternativa (no usada)

Compilar con `-fvisibility=hidden` **más** anotar cada función de la API con
`KULT_API`. Da el mismo resultado pero obliga a incluir `kult_api.h` y prefijar
~31 definiciones. El version-script consigue lo mismo sin tocar `src/`; si algún
día se prefiere `-fvisibility=hidden`, `KULT_API` ya está listo en la cabecera.

## Mantenimiento

Al añadir una entrada pública, añádela a **los dos** sitios: `kult_core.map` y
`include/kult_api.h` (el test falla si se desincronizan). La API es la frontera
pública del core.
