# PLAN — Compilación PC (`kult-port-pc-compilation`)

Plan **definitivo** del proyecto de compilación: un Kult **nativo en C para PC**
que **une las versiones Atari ST y Amiga**, con mejoras y **todos los idiomas**.

Sustituye como hoja de ruta al `PLAN_MULTIVERSION.md` del predecesor
(`kult-port-legacy`), que queda como **referencia/inventario**.

## 0. Premisa y principios

- **Nace de `kult-atari-st-c`.** Al crear el repo se copian `src/`, `include/`,
  `test/` y `tools/` de `kult-atari-st-c` (el C verificado del ST). **No** se
  parte del C viejo de `kult-port-legacy`, que queda **superado**.
- **No se reescribe el core verificado**: se envuelve. La lógica del juego es el
  core ST (4 planos / 16 colores); Amiga aporta su propia spec y su capa.
- **El core no conoce SDL ni hardware**: input, tiempo, pantalla, audio y ficheros
  se inyectan (ABI `os_*`/`hw_*`/`psg_*`/`sched_*`).
- **Recursos por ID lógico**, no por nombre físico; cada versión resuelve el suyo.
- **Verificación**: el harness diferencial (Musashi) se conserva como red de
  seguridad del **perfil vanilla ST**; Amiga y lo nuevo llevan tests propios.
- **Licencia**: no se distribuyen assets ni binarios; el usuario aporta su copia.

## 1. Objetivo y alcance

- Juego nativo (Linux y Windows, SDL2) **sin Musashi** en release (el oráculo se
  queda para test/CI).
- **Dos experiencias nativas**: **Atari ST** y **Amiga**.
- Perfil **CUSTOM** libre y persistido: arte (ST/Amiga) × audio (ST/Amiga) +
  ritmos, cursor e interpolación.
- **Ingesta automática** de los originales del usuario (zip/imágenes de disquete)
  → `data/`, sin pasos manuales.
- **Todos los idiomas** (incluida la traducción ES ya trabajada en el predecesor).
- Base de código organizada y documentada.

## 2. Arquitectura de capas

```
frontend/   SDL2 · overlay F1 · config · presentación (escalado, HD, cursor)
platform/   HAL ST (traps/hw/vídeo/audio) + ABI del C (os_*/hw_*/sched_*/psg_*)
            + planificador (reloj de sim, esperas cooperativas)
            HAL Amiga (chipset/Paula/timing)                  [workstream Amiga]
core/       lógica del juego en C (src/ de kult-atari-st-c, 4 planos/16 colores)
            core Amiga: lógica compartida + capa Amiga        [workstream Amiga]
assets/     ingesta (zip/stx/adf) + proveedores por versión (ST/Amiga)
render/     planar→RGBA (parametrizable), paletas
audio/      backend YM2149 (ST) · backend Paula/samples (Amiga)
```

## 3. Perfiles

Un perfil es un conjunto de parámetros; `ATARI ST` y `AMIGA` son **presets
fijos** (experiencia nativa, no editables); `CUSTOM` expone los ejes y se
**persiste**.

| Eje | ATARI ST | AMIGA | CUSTOM |
|---|---|---|---|
| tasa de simulación | 15 Hz | nativa Amiga (a medir) | libre |
| presentación | 15 Hz (=tick) | nativa | libre (60/120…) |
| cursor | 15 Hz (del juego) | nativa | 15 Hz / refresh / HD |
| interpolación traslaciones | no | según Amiga | on/off |
| delays por CPU (`dbf`) | emulados | emulados | modernizados |
| arte | ST | Amiga | ST/Amiga |
| audio | YM2149 | Paula/samples | ST/Amiga |

Si solo hay una versión disponible, su perfil es el de defecto y el otro se
oculta.

## 4. Fases

### F0 — Ingesta de datos
Doble clic al ejecutable y que funcione con los ficheros del usuario. Detección de
`*.zip`, `*.stx`/`*.st`, `*.adf`/`*.ipf`/`*.msa`; extracción a `data/`. Lectores
en C: ZIP (miniz), **STX/Pasti + FAT12**, **ADF/OFS-FFS** (nuevo). Manifiesto de
disponibilidad por versión.

### F1 — Motor C nativo sin Musashi
Glue del ABI del kit sobre la HAL; bucle core-driven (el core manda; las esperas
de VBL/input presentan frame y sondean SDL); `_start`/crt0 → `game_loop` C.
El arranque y las primitivas del predecesor (`runtime/native/*`) son **referencia
de comportamiento**, no código a copiar.

### F2 — Reloj, ritmos y perfiles
`kult_profile` + `kult.ini` + opción en overlay; acumulador fixed-step; rates por
elemento. **Director de animaciones** central (transiciones, traslaciones, frames,
esperas de guion) en vez de los trucos dispersos del original (VBL, `dbf`,
retardos). El perfil **ST** deriva los valores de los factores **medidos del
oráculo**; **CUSTOM** los sobreescribe.

### F3 — Arte multi-versión + paletas
Cargadores ST (`.PI1`) y Amiga (`*.BIN`, 4 planos contiguos → deintercalar).
**Paletas Amiga**: localizar en el ejecutable/volcar del emulador; fallback =
paletas ST.

### F4 — Audio multi-versión
Backend YM2149 (reconstruido en el ST) y backend **Paula/samples Amiga**. Datos
Amiga (`0–7`, `A`, `B`, `BRUIT`, `SOURI`, `MUSIQ`); estrategia: convertir a un
formato interno y validar A/B contra emulador.

### F5 — Cursor y suavizado
Desacoplar el cursor del tick (dibujo/restauración a refresh, backup propio);
interpolación continua de traslaciones manteniendo la duración total; **frames**
(poses) intactos. Delays CPU emulados en ST/AMIGA, modernizados en CUSTOM.

### F6 — Amiga: spec + core (ver §5)
Confirmar el grado de divergencia del binario Amiga respecto al ST, hacer la
**spec Amiga** (decomp) y reconstruir el **core/capa Amiga**; verificar contra el
binario Amiga. Hito previo: comparación por rutina Amiga↔ST.

### F7 — Organización y release
Reorganización de carpetas (producto vs dev/CI), índice de docs, icono,
metadatos, partida completa de validación (cambio de disco incluido), empaquetado
con ingesta.

## 5. Amiga: determinación

**Sí: la versión Amiga debe tener su propia spec (decomp) y su reconstrucción C.**
La compilación promete reproducir **fielmente ambas** versiones, y la garantía de
este proyecto es «**spec byte-exacta por versión → verificación diferencial**».
Sin spec Amiga no hay oráculo ni forma de verificar el modo Amiga; quedaría en
aproximación. Además, **no existe trabajo previo de decomp Amiga** (el predecesor
solo extrajo arte/texto: pantallas Amiga = bitmap ST *desintercalado*; `PUZZL.BIN`
y bancos de texto idénticos), pero el método ya está probado en
`kult-atari-st-c` y es reutilizable.

**Alcance (lo que abarata):** al ser el mismo motor portado ST↔Amiga, buena parte
de la lógica será **equivalente** a la del ST. La vía Amiga se plantea como:

1. **Identificar lo compartido** y **reutilizar** el C de `kult-atari-st-c`.
2. Decompilar/reconstruir solo lo **específico de Amiga**: vídeo bitplane
   desintercalado, audio Paula, carga ADF/OFS-FFS, timing/presentación, paletas.
3. **Core Amiga** = lógica compartida + capa de plataforma Amiga, verificado
   contra el binario Amiga (hunk).

**Primer hito (dimensiona el trabajo):** comparar por rutina el ejecutable Amiga
con el decomp Atari (qué coincide, qué se reubica, qué es nuevo) y fijar el censo
real de la spec Amiga. Los assets/audio/textos Amiga no dependen de esto y pueden
avanzar en paralelo.

## 6. Verificación

- **Vanilla ST**: cubierto por el harness diferencial (imagen + registros +
  ledger) de `kult-atari-st-c`.
- **Amiga**: harness diferencial contra un oráculo del binario Amiga (a montar
  en el workstream Amiga, análogo al de ST).
- **Mods** (perfiles, arte, audio, cursor, ingesta): tests nuevos — conversores
  (round-trip arte/paleta), audio (golden/A-B), perfiles (persistencia), ingesta
  (fixtures zip/stx/adf).
- Mantener siempre un **modo vanilla** para regresión.

## 7. Origen del repo y dependencias

- **Origen**: `kult-atari-st-c` (copia de `src/`, `include/`, `test/`, `tools/`).
- **`kult-atari-st-decomp`**: spec Atari ST (solo lectura).
- **`kult-port-legacy`**: referencia de features/comportamiento (solo lectura).

## 8. Criterios de aceptación (por fase)

- F0: arranca con solo el zip de ST, solo con el de Amiga, y con ambos ofrece los
  dos perfiles; sin ninguno, mensaje claro.
- F1: partida completa con el C; harness ST verde; comparación por muestreo
  build-C vs emulado.
- F2: ST idéntico al vanilla; CUSTOM persiste; AMIGA/ST/CUSTOM según
  disponibilidad.
- F3/F4: arte/audio intercambiables correctos con su paleta/sonido.
- F5: criterios de suavizado documentados y verificados en ST/AMIGA/CUSTOM.
- F6: modo Amiga verificado contra el binario Amiga.
- F7: partida completa de validación y paquete con ingesta.

## 9. Preguntas abiertas

- Censo real de la spec Amiga (lo fija el primer hito de §5).
- Paleta Amiga: método de extracción definitivo.
- Tasa nativa Amiga y si la lógica Amiga difiere de la ST (afecta a F2/F6).
- Reorganización de carpetas (producto vs dev/CI) y empaquetado.
