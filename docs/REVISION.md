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

> **Censo previo a la revisión.** Al cerrar P1 el repo quedó en **494 pruebas**
> (478 + las añadidas por P0/P1: cobertura y regresiones). Las cifras de esta
> sección son la foto inicial; el estado vivo está en `docs/ESTADO.md`.

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
- `make test` verde (494+), `make regscan` `ids_reg=0`, `make core` OK.
- `docs/REVISION.md` con el **registro de hallazgos** y `docs/ESTADO.md`
  actualizado. Solo entonces se abre la fase 3.

## 7. Registro de hallazgos

Revisión P0 completada (commands, video, dispatch, handlers, loop, scale,
textbox, select, mode) con los cuatro ejes, incluidos los cierres de H-D1 y los
huecos de cobertura. `make test` 492 verde, `make regscan` `ids_reg=0`. Commits
`revision(<módulo>)` (+ cierre de hallazgos). Ningún ALTA/MEDIA sin resolver o
marcado intencional con justificación.

### Tabla resumen

| id | sev | módulo | función (0xADDR) | estado |
|---|---|---|---|---|
| H-V1 | ALTA | video | `palette_cycle` (0x16310) | arreglado (1aefb82) |
| H-S1 | ALTA | select | `sample_menu_b` (0x14bfa) | arreglado (f6ea85b) |
| H-S2 | MEDIA | select | `sample_menu_a/b` (0x14ac6/0x14bfa) | arreglado (f6ea85b) |
| H-T1 | ALTA | textbox | `text_window` (0x10b6c) | arreglado (2d39743) |
| H-T2 | MEDIA | textbox | `scene_text_box` (0x13f86) | arreglado (2d39743) |
| H-C1 | ALTA | commands | `grid_wait` (0x14f74) | arreglado + test |
| H-C2 | ALTA | commands | `tail_15022` (0x15022) | arreglado (2569fa5) |
| H-C3 | ALTA | commands | `anim_threshold2` (0x14f5e) | arreglado + test |
| H-C4 | MEDIA | commands | `grid_wait` (0x14f74) | intencional (no observable) |
| H-H1 | ALTA | handlers | `overlay_dispatch_4` (0x1200a) | arreglado (3a2a708) |
| H-H2 | MEDIA | handlers | `obj_name*` (0x1687e/0x168a0/0x168d4) | arreglado (3a2a708) |
| H-D1 | ALTA | dispatch | `dispatch_a6` (0x10390) | arreglado + test |
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
| H-M1 | MEDIA | mode | `scene_mode_apply` (0x1460a) | arreglado + test (cobertura) |
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
de cada pasada (`l_014ad0`/`l_014c10`); el C solo una vez. Arreglado
(alineación de fidelidad); ***intencional*** sin test: el único modo de observar
la diferencia es que el comando despachado dentro del bucle (`0xc1bd` → byte de
plantilla `0x1bc`) reescriba `$1a488`, y el camino de byte de la VM no invoca las
entradas de menú de samples (son comandos `0xa0xx`, no bytes). Inobservable en
las rutas ejercitables.

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
Arreglado. Test: `test_fun_14f74_dispatch_sel` (objeto en `0x68000`, que
`grid_bands` no pisa, deja `$1acc9 != 0`; campo `+0`=0 y `+8`=0xa035, que era lo
que leía el C viejo).
**[H-C2] ALTA · commands · `tail_15022` (0x15022)**
La condición de `$1acec` estaba invertida (`bcs` → `$1acec < 0x3f` despacha sin
`0x9005`). Regresión: caso `(0x10,0x05)` en `test_fun_14f74`.
**[H-C3] ALTA · commands · `anim_threshold2` (0x14f5e)**
Ignoraba el retorno 2 de `0x14f24` (que hace `addq.l #4,a7` y salta al
llamante); ejecutaba `0x16310` y el flag. Arreglado. Test:
`test_fun_14f5e_retorno2` (`$1ad20=1`, `stop_pc=0x14f5c`).
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
exterior `f_010630` continúa ahí. El C devolvía `script+1` (el byte bajo del word
inline) y no propagaba ese A6. Arreglo LOCAL (no toca los demás handlers):
`dispatch_a6` hace el `walk_entry + vm_run` de la rama `0xf000` y devuelve el A6
resultante; el `case 77` propaga ese valor. Test:
`test_vm_byte_4d_nested_a6` (guion `[0x4d,0x80,0x00,0]`, plantilla de 0x100
entradas; compara `ret` con `a6` del oráculo).
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
`(A_DAT_1AD0F & 0x38) == 8` (formulario hacia delante + `$1a572=1` + `idx =
&7`) no tenía caso. Cubierta con `test_fun_1460a_formulario` (fixture de mode +
formulario de texto, `$1ad0f=0x08`, 3 VBLs) y contraste con la rama `==0`.

### Cobertura restante (no bloqueante)

- Módulos **P1** (`anim`, `element`, `icons`, `music`, `sound`, `fileio`,
  `textblit`, `textform`, `reveal`, `misc`, `vm`) y **P2** sin revisar en esta
  sesión (el alcance pedido era P0).
- Sin hallazgos ALTA abiertos: H-C1, H-C3, H-D1 y H-M1 quedaron con prueba;
  H-S2 se marca *intencional* (inobservable en las rutas ejercitables).

---

## 8. Plan de revisión P1

Alcance: los 11 módulos importados del C viejo (origen **B**, riesgo medio).
Mismo método y ejes que P0 (§3–§5); un commit por módulo `revision(<módulo>)`.

Censo (líneas / funciones-definición / pruebas `def test_`):

| orden | módulo | líneas | funcs | tests | foco principal |
|---|---|---|---|---|---|
| 1 | `vm` | 47 | 4 | 3 | `walk_entry` 0x12950, `run_text_table` 0x13f56, `text_table_entry` 0x13f4c |
| 2 | `misc` | 65 | 16 | 8 | wrappers (`*_wrap`) y avance +1; dirección de las llamadas |
| 3 | `reveal` | 134 | 8 | 4 | `reveal_line`/`0x12f28` (paso de píxel mod total) |
| 4 | `textform` | 171 | 14 | 7 | 0x142cc/0x143e2/0x141f4..0x1424c; **sound_play_a (0x1d)** y D5/`-8` |
| 5 | `icons` | 199 | 22 | 10 | `icon_table` 0x16460, 0x164a4, `draw_hint_icons` 0x16556, 0x164d4/0x164f6; `pal_next_word` y encadenado de A1 |
| 6 | `fileio` | 252 | 24 | 20 | GEMDOS (D0 de trap), `load_resource`/`load_fond`/`load_named_screen`, `SCENA.BIN` (retorno por CCR) |
| 7 | `sound` | 257 | 23 | 18 | `ym_write`/`ym_reset`, Timer A, wrappers **0x18f7a/8e/98/a2/a4**, ISR `movep` (tabla de pares) |
| 8 | `textblit` | 546 | 37 | 25 | `text_blit_fwd/rev` (planar, fase), `run_text_entry_ex` (nuevo out `yflags`), `mask_restore_rows`, `text_transition` |
| 9 | `anim` | 597 | 34 | 24 | motor, `anim_run`, gemelas 0x159d2/0x159e2, `advance_text_entry`; **sound_play_a/b** |
| 10 | `music` | 602 | 22 | 28 | tracker 0x3aab6, handlers `trap #3/#4`; las **tres máscaras de bit de canal** |
| 11 | `element` | 610 | 44 | 17 | `elem_command` 0x128a4 (retorno doble `l_01294c`), blit de elemento, `class_find` |

Orden: de menor a mayor para calibrar (igual que P0). `element` va último por el
`elem_command`/double-pop, que arrastra a `scale` (H-SC1) y a `handlers`.

### 8.1 Riesgos heredados de P0 (buscar explícitamente)

- **Función hermana equivocada**: `sound_play_a` 0x18f8e vs `sound_play_b`
  0x18f98 (H-V1); wrappers `0x162d8/0x162ce/0x121c4/0x18f7a/8e/98/a2/a4`.
- **Condiciones invertidas** (`bcs`/`bcc`, `bge`/`blt`): H-C2, H-L1, H-L2, H-L3.
- **Campo/offset de tabla equivocado** (H-C1: +0 vs +8).
- **A6 no propagado** (H-D1, H-H2): handlers `void` con avance fijo `+1/+2`.
- **Firmas desalineadas** `void`/aridad vs definición (H-D2..H-D4).
- **Anchos/signo**: `lsl.b` vs `lsl.w`, `muls.w` con signo, `subq.b` vs `-1`.
- **Colas compartidas / `rts` / fall-through** y **efectos off-image** (paleta,
  PSG, IKBD) al ledger.

### 8.2 Procedimiento por módulo

1. `make regscan` y `make mapa`; abrir `docs/mapa/<módulo>.md`.
2. Por cada función definida: abrir su `auto_*.s` y comparar instrucción a
   instrucción (anchos, signo, banderas, pre/post-inc, bucles, orden, A6, rts).
3. Registrar divergencias con la plantilla §5.
4. Arreglar ALTA/MEDIA **con prueba diferencial**; si no es construible,
   justificar *intencional* (como H-S2/H-SC1).
5. `make test` verde, `make regscan` `ids_reg=0`, `make core` OK.
6. Commit `revision(<módulo>): …` y `make mapa`.

Herramienta: agentes de exploración (solo lectura) para una primera pasada por
módulo; **cada hallazgo se verifica a mano** contra el `auto_*.s` antes de tocar
nada (una hipótesis, un cambio), igual que en P0.

### 8.3 Criterio de cierre P1

- Los 11 módulos revisados con la checklist completa (§3).
- ALTA/MEDIA arreglados **con prueba** o *intencional* justificado.
- `make test` verde (492+), `make regscan` `ids_reg=0`, `make core` OK.
- Registro §7 ampliado y `docs/ESTADO.md` actualizado.

Solo al cerrar P1 (y P2) se abre la **fase 3** (re-aplicar kult-port).

---

## 9. Registro de hallazgos P1

Revisión P1 completada (los 11 módulos) con los cuatro ejes. `make test` 494
verde, `make regscan` `ids_reg=0`, `make core` OK. Commits `revision(<módulo>)`.

### Tabla resumen

| id | sev | módulo | función (0xADDR) | estado |
|---|---|---|---|---|
| H-mus1 | ALTA | music | `music_cmd` (0x3a734) | arreglado + test |
| H-mus2 | MEDIA | music | `music_install` (0x3a5c2) | arreglado + test |
| H-mus3 | MEDIA | music | `music_tick` (0x3aab6) | arreglado |
| H-mus4 | MEDIA | music | `music_tick` (0x3aab6) | intencional (índices 5–7) |
| H-mus5 | MEDIA | music | `music_load` (0x3a62e) | intencional (índice signado) |
| H-tb1 | MEDIA | textblit | `bit_reverse_word` (0x11196) | arreglado + test |
| H-tb2 | MEDIA | textblit | `run_text_entry_ex` (0x14138) | arreglado + test |
| H-tb3 | BAJA | textblit | comentarios | arreglado |
| H-an1 | MEDIA | anim | `anim_counter` (0x15cd8) | arreglado + test |
| H-an2 | MEDIA | anim | `anim_engine` (0x15bde) | arreglado (do-while) |
| H-an3 | BAJA | anim | `frame_gate_delay`/`KULT_ANIM_GATE` | intencional (hook) |
| H-an4 | BAJA | anim | comentarios | arreglado |
| H-el1 | ALTA | element | `elem_command` (0x128a4) | intencional (H-SC1) |
| H-el2 | MEDIA | element | `walk_entry` decl | arreglado |
| H-el3 | BAJA | element | `text_block_build` (0x129d6) | arreglado |
| H-rv1 | BAJA | reveal | `cpu_delay`/`KULT_REVEAL_GATE` | intencional (hook) |

Módulos P1 sin hallazgo de comportamiento: `vm`, `misc`, `textform`, `icons`,
`fileio`, `sound` (solo mejoras de comentario/legibilidad o cobertura).

### Detalle

- **[H-mus1] ALTA · `music_cmd` (0x3a734)**: el bloque bit8 hace
  `move.b $1d(a6),d0`, que pisa el byte bajo del acumulador D0 (el mínimo
  devuelto); el C no lo tocaba. Arreglado (`minv = (minv & 0xffffff00) |
  img[state+0x1d]`); test con `0x0100`/`0x0110`.
- **[H-mus2] MEDIA · `music_install` (0x3a5c2)**: el `movem.l (a7)+,d0-...` final
  restaura D0, así que devuelve el D0 de entrada, no el handler anterior de
  Setexc. Firma cambiada a `(img, d0_in)`; test con `d0_in ∈ {0,0x1234,0xdead}`.
- **[H-mus3] MEDIA · `music_tick` (0x3aab6)**: en `l_03ab3a` el original NO
  escribe `channel[8]` (solo lo hace en `l_03abe2`, byte != 0xff); el C lo
  escribía también en la ruta 0xff. Corregido.
- **[H-mus4] MEDIA · `music_tick`**: el salto calculado `tabla[b&7]` con
  índices 5–7 lee fuera de la tabla de 5 entradas (datos en 0x3aec0). El C
  retorna; intencional (comando malformado, no generado por el juego).
- **[H-mus5] MEDIA · `music_load` (0x3a62e)**: el índice `(a0,d0.w)` se
  sign-extiende en el original; el único llamante pasa selector 0. Intencional.
- **[H-tb1] MEDIA · `bit_reverse_word` (0x11196)**: las ops `.b`/`.w` y el
  `move.w d1,d0` preservan la palabra alta de D0/D1; el C la descartaba. Test con
  palabra alta != 0.
- **[H-tb2] MEDIA · `run_text_entry_ex` (0x14138)**: D5 sale de `move.w d1,d5`,
  que conserva la palabra alta del stride (0xffff con flags bit7). Out-param
  `yflags` arreglado; `scene_text_box` (0x13f86) ahora enmascara al byte bajo
  (`cmp.b`). Tests en `test_textblit` y `test_textbox`.
- **[H-an1] MEDIA · `anim_counter` (0x15cd8)**: compara el byte completo
  (`cmpi.b #7`), no `&7`; con bits altos ejecuta la rama de recorte. Test con
  `fl ∈ {0x0f,0x17,0xff}`.
- **[H-an2] MEDIA · `anim_engine` (0x15bde)**: los bucles de bloques/elementos
  son `dbra` do-while (con 0 ejecutan 65536 iteraciones); convertidos.
- **[H-el1] ALTA · `elem_command` (0x128a4)**: el retorno doble de `l_01294c`
  (`addq.l #4,a7; rts`) no se propaga a los 12 llamantes (`blit_dispatch_a/b`,
  `overlay_dispatch_1/2`, `reveal_pixel`, `reveal_box_*`, `elem_blit_vbl`,
  `scale_prep/loop`, `attract`). Intencional (ver H-SC1 y `REFERENCIA_F4`).
- **[H-el2] MEDIA · `walk_entry`**: `element.c` la declaraba `uint32_t`; la
  definición (`vm.c`) es `void` (UB de declaraciones). Alineado.
- **[H-el3] BAJA · `text_block_build` (0x129d6)**: producto `mulu.w` (palabras
  bajas) en vez de 32×32.

Los hooks `cpu_delay`/`KULT_REVEAL_GATE` (reveal) y `frame_gate_delay`/
`KULT_ANIM_GATE` (anim) son instrumentación del port, no-op por defecto
(`include/kult_port.h`): no alteran la imagen y están documentados.

### Cierre

- P1: 11/11 módulos con checklist completa; ALTA/MEDIA resueltos o *intencional*
  con justificación.
- `make test` 494 verde, `make regscan` `ids_reg=0`, `make core` OK.
- Falta **P2** (`palette`, `cursor`, `input`, `delay`, `boxdraw`, `line`,
  `maskblit`, `sprite`, `text`, `overlay`, `screens`, `state`, `stubs`) para
  cerrar la revisión y abrir la fase 3.

---

## 10. Plan de revisión P2

Alcance: los 13 módulos restantes (origen mixto: portados a mano o ya auditados;
repaso ligero). Mismo método y ejes que P0/P1 (§3–§5); un commit por módulo
`revision(<módulo>)`.

Censo (líneas / funciones-definición / pruebas `def test_`):

| orden | módulo | líneas | funcs | tests | foco principal |
|---|---|---|---|---|---|
| 1 | `stubs` | 12 | 4 | 1 | `rts` pelados (0x135e0, 0x160c0, …) |
| 2 | `delay` | 32 | 2 | 2 | espera/`vsync` |
| 3 | `input` | 42 | 2 | 4 | `ikbd_handler` 0x13686, paquete IKBD (0x1a459) |
| 4 | `state` | 56 | 4 | 4 | estado/parámetros globales |
| 5 | `palette` | 71 | 6 | 6 | piloto ya auditado: `set_palette_entry` 0x162e2, `xbios_setpalette` 0x16300 |
| 6 | `cursor` | 71 | 3 | 2 | cursor (`cursor_draw`, clip, `cursor_restore`) |
| 7 | `boxdraw` | 149 | 9 | 5 | marcos/rects (`box_hline/vline`, 0x12ffa/0x13216) |
| 8 | `line` | 185 | 5 | 3 | `0x13d0e` (Bresenham, **ya tuvo un bug de paridad corregido**) |
| 9 | `sprite` | 189 | 8 | 7 | `blit_sprite` 0x124f8 |
| 10 | `maskblit` | 282 | 18 | 9 | `mask_restore_rows` 0x1215e, `blit_mask_rows` 0x12466 (degenerado `groups==1`) |
| 11 | `text` | 303 | 18 | 17 | `advance_text_entry` 0x10f8e, `print_char`/`print_word`, `measure_word` |
| 12 | `screens` | 528 | 32 | 6 | `screen_address` 0x10fec/0x10fee (fase d7), `copy_screen`, `swap_screens`, `row_blit` 0x16b76 |
| 13 | `overlay` | 809 | 12 | 8 | blitters de overlay (0x111b6/0x114ba/0x117fe/0x11afa): mapeo de longs por plano 1→3/2→7/3→0xb/4→0xf, `roxl`, avance de A0/A1/A2 |

Orden: de menor a mayor. `overlay` va último (blitters con muchas trampas).

### 10.1 Riesgos heredados de P0/P1 (buscar explícitamente)

- **Función hermana equivocada** y wrappers (H-V1, H-mus1).
- **Condiciones invertidas** (`bcs`/`bcc`, `bge`/`blt`): H-C2, H-L1..L3.
- **Campo/offset de tabla equivocado**: H-C1.
- **A6 no propagado** en handlers `void`: H-D1, H-H2.
- **Firmas/declaraciones desalineadas** (`void`/aridad): H-D2..D4, H-el2.
- **Anchos/signo**: `.b`/`.w`, `lsl.b` vs `lsl.w`, `muls.w`/`mulu.w`, `subq.b` vs `-1`.
- **Bucles `dbra` do-while** (0 ⇒ 65536) y **retornos dobles** (`addq.l #4,a7`): H-an2, H-SC1.
- **Palabra alta** en out-params (`move.w` preserva la alta): H-tb1, H-tb2.
- **Registros de salida / `movem`** que restaura D0: H-mus2.
- **Colas compartidas `rts`/fall-through** y **ledger off-image** (paleta, PSG, IKBD, `Setcolor`, `Vsync`).

### 10.2 Trampas del kit ya conocidas (para P2)

- `0x13d0e` (line): el `beq`/`bne` de paridad ya se corrigió una vez; cubrir
  `d0&0xf == 0`.
- Blitters de overlay: el mapeo de longs por pasada es **1 plano→3, 2→7, 3→0xb,
  4→0xf** (no al revés); el tail de `0x117fe` hace `andi.w #$f,d0` (fase 0–15);
  el avance persistente de A0/A1/A2 incluye los `+8` internos.
- `0x128a4`/`0x134da` y `0x15944` A2 fuera de ventana, etc. (ver
  `docs/REFERENCIA_F4.md`, sección final).

### 10.3 Criterio de cierre P2 (y de la revisión)

- Los 13 módulos con checklist completa (§3).
- ALTA/MEDIA arreglados **con prueba** o *intencional* justificado.
- `make test` verde (494+), `make regscan` `ids_reg=0`, `make core` OK.
- Registro §7/§9/§11 ampliado y `docs/ESTADO.md` actualizado.
- **Entonces** se abre la **fase 3** (re-aplicar kult-port).

---

## 11. Registro de hallazgos P2 (cierre de la revisión)

Revisión P2 completada (los 13 módulos) con los cuatro ejes. `make test` **499**
verde, `make regscan` `ids_reg=0`, `make core` OK. Commits
`revision(<módulo>)` (uno por módulo, incluidos los que solo aportan comentario o
cobertura).

Módulos: `stubs`, `delay`, `input`, `state`, `palette`, `cursor`, `boxdraw`,
`line`, `sprite`, `maskblit`, `text`, `screens`, `overlay`.

### Tabla resumen

| id | sev | módulo | función (0xADDR) | estado |
|---|---|---|---|---|
| H-MB1 | MEDIA | maskblit | `hint_select_on/off` (0x120dc/0x120e4) | arreglado (7cfbdac) + test |
| H-OV1 | MEDIA | overlay | `overlay_compose_*` (0x111b6/0x114ba/0x117fe/0x11afa) | arreglado (963789b) + test |
| H-SCR1 | MEDIA | screens | `screen_offset/address*` (0x10ff4/0x10fec/0x10fee) | intencional (palabra alta de D7) |
| H-SCR2 | BAJA | screens | `copy_rows` (0x15944) / `screen_address_off` | arreglado (978df7a) |
| H-LN1 | MEDIA | line | `reveal_line` (0x13d0e) | intencional (bgt/V inalcanzable) |
| H-SP1 | BAJA | sprite | `blit_sprite` (0x124f8) | intencional (outer/inner = 0) |
| H-TX1 | MEDIA | text | `measure_words` (0x10998) | intencional (palabra alta D5/D6) |
| H-TX2 | MEDIA | text | `wrap_line` (0x109bc) | intencional (palabra alta D5/D6) |
| H-TX3 | MEDIA | text | `print_words_fit` (0x1090e) | intencional (palabra alta D6) |
| H-TX4 | BAJA | text | `print_word` (0x108a2) | intencional (palabra alta D0) |
| H-TX5 | BAJA | text | `overlay_a_draw/b_draw` (0x109fe/0x10a88) | intencional (A6 no consumido) |
| H-SE1 | BAJA | state | `clear_object_flags`/`set_cursor_clip`/`fix_sample_flag` | arreglado (2678d1a) |

Módulos **sin hallazgo de comportamiento**: `stubs`, `delay`, `input`, `palette`,
`cursor`, `boxdraw` (solo mejoras de comentario y cobertura; ver detalle).

### Detalle

**[H-MB1] MEDIA · maskblit · `hint_select_on/off` (0x120dc/0x120e4)**
La entrada se calcula con `subq.w #1,d0; lsl.w #4,d0; andi.l #$ffff,d0`: el
desplazamiento es de **palabra** y se trunca a 16 bits. El C multiplicaba en 32
bits, así que con `index == 0` leía `0x11a840` (fuera de la imagen) en vez de
`0x2a840`, y con `index == 0x1001` leía `0x2a850` en vez de `0x1a850`. Arreglo:
`hint_entry()` con `((((index-1)&0xffff)<<4)&0xffff)`. Test:
`test_hint_select_offset_truncado` (index 0 y 0x1001, con la entrada y la de
`0x2a840` pokeadas con valores distintos).

**[H-OV1] MEDIA · overlay · blitters de planos**
`lsr.l`/`asl.l` del 68000 toman el count **mod 64** y dan 0 para 32..63; el C
usaba el desplazamiento nativo (mod 32 en x86 y UB en C para counts ≥ 32). Con
`d0 = 32/33/40` el oráculo y el candidato diferían en `0xd8f2..` y en pantalla.
Arreglo: helpers `lsr_l`/`lsl_l` (count mod 64, 0 si ≥ 32) en los 8 sitios de
las cuatro familias. Test: `test_fun_phase_ge_32` (d0 = 32, 33, 40, 63 en
`planes`/`planes32`/`masked`/`shift`).

**[H-SCR1] MEDIA · screens · `screen_offset` (0x10ff4) — *intencional***
`move.w d0,d7` + `andi.w #$f,d7` solo escriben la palabra baja de D7: el
original conserva `d7[31:16]` de entrada. El C devuelve solo el nibble. Es un
residuo del registro del llamante, no un valor de la función; ningún llamante
lee la mitad alta (todos usan `move.w`/`andi.w`/`ror.l`) y modelarla obligaría a
leer locales `phase` sin inicializar en ~40 llamantes. Inobservable en el juego.

**[H-SCR2] BAJA · screens (arreglado)**
El comentario de `copy_rows` (0x15944) describía una caja de texto
(copia-pega de `textbox.c`); es la variante de `screen_copies` sobre
`screen_ptr_b`. Se corrige y se elimina `screen_address_off` (código muerto
duplicado de 0x10fec). Se añade cobertura directa de `copy_screen_rev` 0x121b6
(A→B) con `poison`.

**[H-LN1] MEDIA · line · `reveal_line` (0x13d0e) — *intencional***
El `bgt` tras `add.w d5,d7` usa el flag V (comparación con signo real); el C
prueba `(int16_t)err > 0`, que solo mira N. Solo divergen si el `add.w` de 16
bits desborda con signo, lo que exige `|dy| >= 0x4000` (la recta saldría de la
imagen: el candidato indexaría fuera del buffer y el oráculo descartaría las
escrituras). No es construible como caso diferencial y no es alcanzable con los
datos del juego (altos de caja de decenas de píxeles). El resto del Bresenham
(paridad `beq`/`bne`, acarreo de `rol/ror`, `+/-6`) es fiel y queda cubierto.

**[H-SP1] BAJA · sprite · `blit_sprite` (0x124f8) — *intencional***
`d2/d3` se cargan de bytes y el original hace `subq.w #1` + `dbra`: con
`outer == 0` o `inner == 0` ejecuta 65536 iteraciones. El C usa `for`. Con
stride `0xa0` eso escribiría muy fuera de la imagen; los descriptores reales
(`save_rect`/sprites) tienen ambos ≥ 1 y el caso no es construible en el arnés.
`blit_rect` (misma entrada 0x12510) sí usa el do-while fiel.

**[H-TX1..TX4] MEDIA/BAJA · text — *intencional***
`measure_words` (0x10998), `wrap_line` (0x109bc) y `print_words_fit` (0x1090e)
hacen `clr.w`/`move.w`/`addq.w` sobre D5/D6, y `print_word` (0x108a2) `move.w
#1,d0`: solo sustituyen la palabra baja, conservando la alta del llamante. El C
devuelve el valor de 16 bits significativo. La mitad alta es un residuo, ningún
llamante la lee (todos usan `cmp.w`/`addq.w`/`tst.w`) y modelarla exigiría pasar
ese residuo como entrada o leer locales sin inicializar. Inobservable en el juego.

**[H-TX5] BAJA · text · overlays (0x109fe/0x10a88) — *intencional***
`text_index_arg` avanza A6 cuando el selector es < 4, pero `overlay_a_draw`/
`overlay_b_draw` descartan ese A6 (no lo devuelven). Ningún llamante lo consume
después; los que pasan `script = 0` lo hacen con selectores ≥ 4 (p. ej.
`game_loop` fija 0x75/0x64) y los que pueden usar < 4 pasan el guion real
(`scene_text_box`). Se añade cobertura del selector 0 y de d4 = 1..3 en
`text_index_arg`.

**[H-SE1] BAJA · state (arreglado)**
`clear_object_flags`, `set_cursor_clip` y `fix_sample_flag` usaban literales
para globales que ya tienen macro (`A_OBJ_TBL`, `A_DAT_1ACD3`, `A_CLIP_X/Y`,
`A_CURSOR_SPRITE_PTR`, `A_LIST_1A8F0`). Se alinean con la convención §4.1; sin
cambio de comportamiento (mismas direcciones).

**[stubs/delay/input/palette/cursor/boxdraw] Sin hallazgos de comportamiento**
Los cuatro `rts` de `stubs` (y 0x11014 en `textblit.c`), las esperas de VBL
`do-while`, `wait_frame` (IKBD + Crawio), la paleta (incluido `mulu.w` de la
palabra baja), el cursor (clip/rotación/guardado 16×16) y las primitivas de
caja (planos, wrap de grupo, copias) coinciden instrucción a instrucción. Se
añadieron casos de cobertura (fases, clip, índices con palabra alta, fase 15,
`copy_screen_rev`) y comentarios de contrato (D0 residual de las esperas).

### Cierre de la revisión (P0 + P1 + P2)

- **33/33 módulos** auditados con la checklist de §3.
- **499 pruebas** diferenciales verdes, `make regscan` `ids_reg=0`, `make core`
  OK, 0 warnings.
- Hallazgos P2: **2 ALTA/MEDIA arreglados con prueba** (H-MB1, H-OV1); el resto
  ALTA/MEDIA se justifica *intencional* por ser inobservable/inalcanzable o no
  construible en el arnés (H-SCR1, H-LN1, H-TX1..TX3). No queda ningún ALTA/MEDIA
  abierto. Los BAJA se arreglan o se documentan.
- La revisión del C queda **cerrada**: se abre la **fase 3** (re-aplicar las
  mejoras de `kult-port` sobre esta base, con plan detallado hecho entonces).
