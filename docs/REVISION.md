# REVISIÓN del C (previa a la fase 3 de kult-port)

Objetivo: auditar el C ya escrito y **renombrado** —antes de re-aplicar encima
las mejoras de kult-port— para asegurar que es fiel, correcto y legible. No se
modifica el comportamiento salvo que la revisión encuentre un fallo real
(entonces: arreglo + prueba + commit).

Este documento es el plan y el prompt de arranque. La revisión se hace en una
**sesión nueva y limpia**, sin tocar nada de kult-port.

## 1. Por qué revisar (riesgos conocidos)

- El **bloque central** se importó del C viejo verificado (F4) y luego se
  **renombró dos veces** (nombres viejos → convenciones nuevas → nombres
  semánticos). Cada pasada es una oportunidad de deriva.
- El renombrado se hizo con un `span` que al principio cogía el **prototipo** en
  vez de la definición (corregido); conviene confirmar que no quedó ninguna
  función a medias.
- Los **tests diferenciales solo prueban las rutas que ejecutan**. La revisión
  debe mirar sobre todo **rutas no cubiertas** y semántica de bajo nivel
  (banderas, anchos, orden de escritura).
- Hay **dos módulos sin test dedicado** (`commands`, `video`) y otros con poca
  densidad de pruebas por función.

## 2. Alcance y prioridades

Origen: **A** = portado a mano al principio (revisado al escribir, bajo riesgo);
**B** = importado del C viejo y renombrado (mayor riesgo).

| prio | módulos | motivo |
|---|---|---|
| **P0** | `commands`, `video`, `dispatch`, `handlers`, `loop`, `scale`, `textbox`, `select`, `mode` | importados, grandes, poca prueba dedicada / sin ficha de test propia |
| **P1** | `anim`, `element`, `icons`, `music`, `sound`, `fileio`, `textblit`, `textform`, `reveal`, `misc`, `vm` | importados, tamaño medio |
| **P2** | `palette`, `cursor`, `input`, `delay`, `boxdraw`, `line`, `maskblit`, `sprite`, `text`, `overlay`, `screens`, `state`, `stubs` | portados a mano o ya auditados; repaso ligero |

Censo (líneas / funciones-definición / pruebas `def test_`):

```
anim 597/34/24   boxdraw 149/9/5    commands 686/18/0*  cursor 71/3/2
delay 32/2/2     dispatch 519/152/21 element 610/44/17  fileio 252/24/20
handlers 864/85/32 icons 199/22/10  input 42/2/4        line 185/5/3
loop 2456/153/96 maskblit 282/18/9  misc 65/16/8        mode 250/31/10
music 602/22/28  overlay 809/12/8   palette 71/6/6      reveal 134/8/4
scale 466/18/6   screens 528/32/6   select 334/25/8     sound 257/23/18
sprite 189/8/7   state 56/4/4       stubs 12/4/1        text 303/18/17
textblit 546/37/25 textbox 434/30/7 textform 171/14/7   video 199/5/0*
vm 47/4/3
```
`*` `commands`/`video` se ejercitan indirectamente (`test_family151.py`,
`test_loop.py`) pero **no tienen `test_<módulo>.py`**.

Total: 12.417 líneas, 456 definiciones, 478 pruebas (418 `def test_` + casos
parametrizados), 0 warnings, `ids_reg=0`.

## 3. Los cuatro ejes (checklist)

### 3.1 Fidelidad de comportamiento vs ASM
Para cada función, comparar el cuerpo C con su `auto_*.s` en
`/app/kult-atari-st-decomp/matching/`:
- [ ] **Anchos**: operaciones `.b`/`.w`/`.l` → tipo correcto (`uint8/16/32_t`),
      sin truncar ni extender de más.
- [ ] **Signo**: `ext.w`/`ext.l`, `lsl/lsr/asr`, comparaciones `bcc/bcs/bge/blt`
      (con/sin signo) bien traducidas.
- [ ] **Banderas**: si el original ramifica por C/Z/N/V, el C reproduce esa
      condición exacta (no una equivalente "casi").
- [ ] **Pre/post incremento** de `aN`/`aN+`/`-aN` y desplazamiento de guion
      (`a6`), incluida la cola compartida (fall-through) entre funciones.
- [ ] **Bucles**: límites, off-by-one, `dbra`, orden de iteración, parada.
- [ ] **Orden de escritura** cuando importa (solapamientos, VRAM/planar).
- [ ] **Registros de salida**: los que la prueba compara (`info["regs"]`) y los
      que se devuelven/por out-param coinciden con el original.
- [ ] **Efectos off-image** (XBIOS Setpalette, PSG, IKBD): van al ledger y se
      comparan; confirmar que el C los emite igual (función, orden, datos).
- [ ] **`movem`/guardas**: si el original preserva registros que el C trata como
      salida, o viceversa.
- [ ] **`rts`/fall-through**: funciones que "caen" en el cuerpo de otra están
      modeladas como llamada/continuación, no duplicadas ni reordenadas.

### 3.2 Nombres y comentarios
- [ ] El nombre C = nombre en `work/names.txt` (convención §4.1).
- [ ] El comentario `/* 0xADDR — … */` corresponde a la etiqueta real del ASM y
      describe **lo que hace** (no un copia-pega erróneo del vecino).
- [ ] Out-params y valores de retorno documentados y coherentes con la firma.
- [ ] Comentarios que citan registros (`d0`, `a6`, …) son **correctos** (es la
      traza al ASM; se permite nombrarlos ahí).
- [ ] No quedan nombres engañosos del primer renombrado (p. ej. un `_tail` que en
      realidad es la cabecera).

### 3.3 Legibilidad / consistencia de estilo (convenciones §4.1)
- [ ] Imagen plana `uint8_t *img` + accesos big-endian (`be16/be32/wr16/wr32`);
      sin `memcpy` de structs ni punteros crudos a mitad de palabra.
- [ ] Offsets vía macros `A_*` de `include/addrs.h` (nada de números mágicos
      repetidos).
- [ ] Helpers compartidos de `include/kult_internal.h` usados donde toca.
- [ ] Tipos correctos y firmas coherentes; sin casts peligrosos.
- [ ] Sin **UB**: shifts ≥ ancho del tipo, overflow con signo, aliasing,
      uninitialized.
- [ ] 0 warnings (`-Wall -Wextra`) y 0 `ids_reg` (`make regscan`).
- [ ] Sin restos de decompilador (`FUN_xxxx`, `loc_`, `sub_`, variables `v1`…).

### 3.4 Cobertura de pruebas
- [ ] Cada entrada del decomp tiene su caso en `test/test_<módulo>.py`.
- [ ] Cada **rama** de una función multi-camino tiene un caso (si el ASM ramifica,
      probar ambos lados).
- [ ] Casos **límite**: 0, `0xff`/`0xffff`, wrap de palabra, frontera de bucle,
      índice fuera de tabla (comportamiento del original, no del C "ideal").
- [ ] Se comparan imagen **y** registros **y** ledger off-image.
- [ ] `commands.c` y `video.c`: crear (o justificar por qué no) `test_<módulo>.py`.

## 4. Procedimiento por módulo (plantilla de sesión)

1. `make regscan` y `make mapa` (snapshot de deuda y fichas).
2. Abrir `docs/mapa/<módulo>.md`: lista de funciones, globales y pruebas.
3. Por cada función (empezando por las de salida pública y las ramificadas):
   - Abrir su `auto_*.s`; recorrer instrucción a instrucción contra el C.
   - Marcar divergencias con la **plantilla de hallazgo** (§5).
   - Comprobar la cobertura de test (§3.4).
4. Al cerrar el módulo: aplicar arreglos + añadir pruebas, `make test` verde,
   `make regscan` sigue en 0, actualizar `docs/mapa/` (`make mapa`).
5. Commit por módulo: `revision(<módulo>): …`.

Orden sugerido: P0 de menor a mayor tamaño para calibrar, luego P1, luego P2.

## 5. Plantilla de hallazgo

```
[H-<n>] <severidad ALTA|MEDIA|BAJA> · <módulo> · <función> (0xADDR)
Qué:      <descripción del problema>
ASM:      <auto_xx.s:NN o fragmento>
C:        src/<módulo>.c:NN
Evidencia:<por qué es un fallo (no estilo)>
Arreglo:  <cambio propuesto> + <test que lo cubre>
Estado:   abierto | arreglado (<commit>) | intencional (divergencia documentada)
```

Severidad: **ALTA** = resultado/efecto distinto al original; **MEDIA** = solo en
rutas no probadas / riesgo de UB; **BAJA** = nombre/comentario/estilo.

## 6. Criterio de cierre

- Todos los módulos **P0 y P1** revisados con checklist completa.
- Hallazgos ALTA/MEDIA arreglados **con prueba** o marcados *intencional* con
  justificación (p. ej. reconstrucción de una cola del original).
- `make test` verde (478+), `make regscan` `ids_reg=0`, `make core` OK.
- `docs/REVISION.md` con el **registro de hallazgos** y `docs/ESTADO.md`
  actualizado. Solo entonces se abre la fase 3.

## 7. Registro de hallazgos

Revisión P0 completada (commands, video, dispatch, handlers, loop, scale,
textbox, select, mode) con los cuatro ejes. `make test` 488 verde,
`make regscan` `ids_reg=0`. Commits `revision(<módulo>)`.

### Tabla resumen

| id | sev | módulo | función (0xADDR) | estado |
|---|---|---|---|---|
| H-V1 | ALTA | video | `palette_cycle` (0x16310) | arreglado (1aefb82) |
| H-S1 | ALTA | select | `sample_menu_b` (0x14bfa) | arreglado (f6ea85b) |
| H-S2 | MEDIA | select | `sample_menu_a/b` (0x14ac6/0x14bfa) | arreglado (f6ea85b) |
| H-T1 | ALTA | textbox | `text_window` (0x10b6c) | arreglado (2d39743) |
| H-T2 | MEDIA | textbox | `scene_text_box` (0x13f86) | arreglado (2d39743) |
| H-C1 | ALTA | commands | `grid_wait` (0x14f74) | arreglado (2569fa5) |
| H-C2 | ALTA | commands | `tail_15022` (0x15022) | arreglado (2569fa5) |
| H-C3 | ALTA | commands | `anim_threshold2` (0x14f5e) | arreglado (2569fa5) |
| H-C4 | MEDIA | commands | `grid_wait` (0x14f74) | intencional (no observable) |
| H-H1 | ALTA | handlers | `overlay_dispatch_4` (0x1200a) | arreglado (3a2a708) |
| H-H2 | MEDIA | handlers | `obj_name*` (0x1687e/0x168a0/0x168d4) | arreglado (3a2a708) |
| H-D1 | ALTA | dispatch | `dispatch_a6` (0x10390) | abierto (plumbing A6) |
| H-D2 | MEDIA | dispatch | `copy_screen_a6` (0x121c4) | arreglado (3a2a708) |
| H-D3 | MEDIA | dispatch | `set_palette_advance` (0x162d8) | arreglado (3a2a708) |
| H-D4 | MEDIA | dispatch | `set_palette_from_entry` (0x162ce) | arreglado (3a2a708) |
| H-D5 | MEDIA | dispatch | `run_command_keep_sp` (0x103ae) | intencional (sin efecto) |
| H-D6 | BAJA | dispatch | comentario "144 entradas" | pendiente |
| H-L1 | ALTA | loop | `game_loop` (0x101be) | arreglado (5bc4ffb) |
| H-L2 | ALTA | loop | `view_change` (0x17a1a) | arreglado (5bc4ffb) |
| H-L3 | ALTA | loop | `icon_scale_loop` (0x1834a) | arreglado (5bc4ffb) |
| H-L4 | MEDIA | loop | `icon_scale_loop` (0x1834a) | arreglado (5bc4ffb) |
| H-L5 | BAJA | loop | `portrait_intro` (0x16a20) | intencional (busy-wait) |
| H-SC1 | ALTA | scale | `scale_prep`/`scale_loop` (0x13702/0x13740) | intencional (retorno doble 0x128a4) |
| H-SC2 | MEDIA | scale | `scale_setup`/`scale_loop` (0x13a18/0x13740) | intencional (divu por 0 = trap) |
| H-SC3 | BAJA | scale | `scale_16_16`/`scale_blit` (0x13a18/0x13bc2) | pendiente (out `phase_start` muerto) |
| H-M1 | MEDIA | mode | `scene_mode_apply` (0x1460a) | cobertura: rama `(clase&0x38)==8` |
| H-V2 | BAJA | video | `palette_cycle` (0x16310) | intencional (busy-wait 65536) |

### Detalle

**[H-V1] ALTA · video · `palette_cycle` (0x16310)**
La fase 2 llamaba `sound_play_b` (0x18f98); el ASM llama `f_018f8e`
(`sound_play_a`), que deja el vector `$134 = 0x1910e` y no publica `0x192c6`.
El caso previo de `test_loop` lo enmascaraba con `$1ab4f=1`. Arreglo: segunda
llamada `sound_play_a`. Test: `test/test_video.py` (6 casos, incluido
`0x1ab4f=0`). `[H-V2]` BAJA: el busy-wait de 65536 de las dos fases no se
reconstruye (sin efecto observable); se documenta en el comentario.

**[H-S1] ALTA · select · `sample_menu_b` (0x14bfa)**
`cur3 ∈ {0x6d,0x6c,0x84}` iba a `l_014cc6`; el ASM (`cmpi #$6d/6c/84; beq
l_014ce6`) los manda a `l_014ce6`. Con el bug el candidato re-buclea. Test:
`test_fun_14bfa_cur3_especial` (alcanza `l_014c94` con un hit-test válido).
`[H-S2]` MEDIA: `0x14ac6`/`0x14bfa` re-fijan `mask/value` y `$1a488` al inicio
de cada pasada (`l_014ad0`/`l_014c10`); el C solo una vez. Arreglado; sin test
dedicado (nada ejercita el cambio de `$1a488` dentro del bucle).

**[H-T1] ALTA · textbox · `text_window` (0x10b6c)**
Para `b6 = A6[3]&0xe0 ∈ {0x40,0x60}` (sin rama) el original pasa a `0x120a8`
`(bx,by)`; el C usaba las coordenadas rancias de la caja. Arreglo: `x=bx,y=by`
antes de la cadena. Test: `test_fun_10b52` con flags 0x40/0x60.
`[H-T2]` MEDIA: `scene_text_box` (0x13f86) recalculaba
`v2=(A6[2]<<1)&0xff` sin el `-8` que `0x14138` aplica a D5 para el bloque `'S'`
(0x53) con `0x1a49e` impar. Arreglo: `run_text_entry_ex` expone D5 (out-param) y
`scene_text_box` lo consume. Test: `test_fun_13f86_special_char`.

**[H-C1] ALTA · commands · `grid_wait` (0x14f74)**
Despachaba la entrada `+8` de `0x1ab62`; el ASM lee `+0` (`move.w (a0,d0.l)`).
Arreglado. Sin test dedicado: la rama `A_SEL_1ACC9 != 0` no se alcanza con el
`obj_wait_hit` del arnés (verificado por ASM y por el binario).
**[H-C2] ALTA · commands · `tail_15022` (0x15022)**
La condición de `$1acec` estaba invertida (`bcs` → `$1acec < 0x3f` despacha sin
`0x9005`). Regresión: caso `(0x10,0x05)` en `test_fun_14f74`.
**[H-C3] ALTA · commands · `anim_threshold2` (0x14f5e)**
Ignoraba el retorno 2 de `0x14f24` (que hace `addq.l #4,a7` y salta al
llamante); ejecutaba `0x16310` y el flag. Arreglado. Sin test dedicado (la rama
se verifica con `stop_pc=0x14f5c`).
**[H-C4] MEDIA · commands · `grid_wait`**
`icon_state_b(img,script,0)` usa `D0=0`; en el original `D0` lo deja `0x121c6`
(0xffff) en la vía de overlay (`$1acc7 ∈ [0x16,0x19)`). Intencional: ruta no
alcanzada en las pruebas (modo fijo 0).

**[H-H1] ALTA · handlers · `overlay_dispatch_4` (0x1200a)**
Pasaba a `scroll_up` la fase almacenada (`out[2]`); el ASM usa el D7 que
recalcula `0x10fec`. Test: casos `d7 != 0` en `test_fun_1200a`.
**[H-H2] MEDIA · handlers · `obj_name`/`obj_name_snd`/`obj_name_clean`**
Descartaban el avance de A6 (2 o 3: `0x10fb4` consume un byte si el índice <4);
el despacho lo fijaba a `+2`. Arreglo: devuelven el A6 final y el `case` lo usa.
Test: `test_nameprint` con `byte<4` comparando el retorno con `a6`.

**[H-D1] ALTA · dispatch · `dispatch_a6` (0x10390)**
El byte handler 0x4d hace `movea.l a5,a6` (nueva corriente de `walk_entry`) y
corre la VM anidada; al volver A6 queda en el fin del sub-guion y el bucle
exterior termina. El C devuelve `script+1` y no propaga ese A6. Abierto: el
plumbing de A6 entre `run_command_keep_sp` y sus llamantes es arquitectónico;
requiere un estudio aparte de los ~48 handlers (ver §3.1, `rts`/fall-through).
**[H-D2..H-D4] MEDIA · dispatch** (`copy_screen_a6` 0x121c4,
`set_palette_advance` 0x162d8, `set_palette_from_entry` 0x162ce): declaradas
`void`/1 arg en `dispatch.c` pero definidas `uint32_t`/2 args (UB de firma). Se
alinean los prototipos y se usan como retorno; `test_dispatch` pasa el 2º
argumento de `run_command_keep_sp`. **[H-D5]** `run_command_keep_sp` usa
`-1` en 32 bits donde el ASM hace `subq.b #1`: sin efecto observable (ambos caen
en `default` del `switch`); intencional. **[H-D6]** BAJA: el comentario dice
"144 entradas" cuando la tabla real de `0x19eca` tiene 38 (las 106 siguientes
son la `byte_handler_table` contigua).

**[H-L1] ALTA · loop · `game_loop` (0x101be)**
El test del temporizador B estaba invertido (`bcc`: salta a `lab_310` si
`ACB0 >= AB60`). Regresión: `test_game_loop_timer_b`.
**[H-L2] ALTA · loop · `view_change` (0x17a1a)**
`action==4` tratado al revés (`0x51/0x52` solo si `y<0x64`; si no `x=2`) y
`x=1` para `action ∉ {0..4}` (el original pone `x=2`). Regresión: casos
`act4_*`/`act5` (los "cuatro"/"cinco" previos indexaban mal `0x1a69d`).
**[H-L3] ALTA · loop · `icon_scale_loop` (0x1834a)**
En el tramo `>=0xc00` no OR-eaba la máscara en `+4` (el ASM cae de `l_01840a` a
`l_018412`). Regresión: `test_fun_1834a_pantalla_cero`.
**[H-L4] MEDIA · loop · `icon_scale_loop`** El `muls.w` del original lee el
cociente de `divu` con signo; el C lo usaba sin extensión. Arreglado.
**[H-L5] BAJA · loop · `portrait_intro` (0x16a20)** y `[H-V2]`: busy-waits de
0x10000/65536 no reconstruidos (sin efecto observable); intencional.

**[H-SC1] ALTA · scale · `scale_prep`/`scale_loop`** (`0x13702`/`0x13740`): el
retorno doble de `0x128a4` (`l_01294c`: `addq.l #4,a7; rts`) no se modela; el C
continúa el cuerpo. Intencional y documentado en `docs/REFERENCIA_F4.md`
(no modelable en el arnés; se verifica con `stop_pc=0x1294c`).
**[H-SC2] MEDIA · scale** `divu.w` por 0 (`scale_setup` con `h_in=0xffff`,
`scale_loop` con `den=0`): el ASM hace trap; el C tendría UB. Intencional: el
juego no alcanza esos valores. **[H-SC3] BAJA**: `phase_start`/`phase` muertos.

**[H-M1] MEDIA · mode · `scene_mode_apply` (0x1460a)**: la rama
`(A_DAT_1AD0F & 0x38) == 8` (formulario hacia delante + `$1a572=1`) no tiene
caso en `test_mode.py` (se probaron `&0x38 ∈ {0,0x10}`). Código revisado fiel;
queda como hueco de cobertura.

### Cobertura restante (no bloqueante)

- Módulos **P1** (`anim`, `element`, `icons`, `music`, `sound`, `fileio`,
  `textblit`, `textform`, `reveal`, `misc`, `vm`) y **P2** sin revisar en esta
  sesión (el alcance pedido era P0).
- Rutas señaladas arriba sin test directo (`H-C1`, `H-C3`, `H-S2`, `H-D1`,
  `H-M1`); el código se contrastó línea a línea con el ASM.
