# F4 — Reconstruir `KULT.PRG` a C verificado byte a byte

**Este es el ÚNICO documento de seguimiento de la tarea F4.** Otros docs
(`PLAN`, `ROADMAP`, `TODO`, `PHASE4_VM`…) contienen teoría o planificación
general; para retomar F4 basta leer este fichero.

- **Objetivo:** sustituir el intérprete Musashi por **C nativo** que reproduzca el
  binario original función a función, **verificado byte a byte** contra él.
- **Alcance:** ~406 funciones del PRG (base de carga `0x10000`).
- **Estado (actualizar aquí):** **F4 completa salvo bloqueos**: `0x14ac6`,
  `0x14bfa` y **`0x16b9c`** verificados (el "bloqueo de 72 teclas" era un
  diagnostico erroneo: son 72 esperas de VBL de 0x16d1e, que caben en
  `OS_SCHED_MAX=512`). También hechos `0x135da` (alias de set_palette) y `0x135e2`
  (crt0_init). El índice de Ghidra (406) queda cubierto salvo las rutinas del FDC
  (fuera de alcance por acuerdo). Los 3 "artefactos" de índice también quedan
  explicados: `0x11e7a` y `0x11ec4` son el cuerpo/cola del bucle de `0x11e5e`
  (su `dbra` salta a 0x11e7a; ya verificado) y `0x135ba` son 4 bytes de datos
  del tramo de `0x13594`. **468 casos de test en verde.**

- **Post-F4 (motor nativo, ver `PLAN_MULTIVERSION.md` Fase 1):** se completó el
  **cableado del `byte_handler`** (tabla `0x19f62`: 65 bytes que no estaban
  conectados, imprescindibles para que el VM recorra los guiones completos) y se
  portó el **arranque `0x10000`–`0x101be`** en `runtime/native/`. El framebuffer
  del motor nativo sale **byte a byte idéntico** al de Musashi.

```sh
cd projects/kult/recreate && make test     # -> 468 passed
# depuración de un caso:  make test PYTEST_ARGS=-n0
```

> **Al cerrar una sesión de F4:** actualiza el % y la frontera AQUÍ y, si cambió
> la tarea en curso, ajusta la sección **"Relevo inmediato" de `AGENTS.md`**
> (es el punto de arranque de la próxima sesión).

---

## Método riguroso por función (no saltarse ningún paso)

1. **Desensamblar** el tramo:
   ```sh
   python3 tools/prg_dis.py work/disk_a/KULT.PRG \
       --start $((ADDR-0x10000+28)) --len N --base 0x10000
   ```
2. **Verificar la FRONTERA real** antes de tocar nada:
   - Localiza el `rts` que cierra la función (y sigue el flujo real: hay
     `fallthrough` y `rts` compartidos).
   - **NO fiarse de los tamaños de `work/decomp.c`** (Ghidra parte por entradas de
     llamada). Ejemplos ya vistos: `0x124f2`→`0x12588` (no 6 B), `0x1258a`→`0x12618`,
     `0x10fec` termina en el `rts` de `0x11014`, y `0x11026` cae en `0x11050`.
   - Si dos entradas comparten cuerpo (p. ej. `0x11016`/`0x11020` solo cambian
     `A4`), reconstruye el cuerpo **una vez** y expón ambas entradas.
3. **Transcribir a C** en `src/*.c`:
   - Firmas `g_<nombre>(uint8_t *img, ...)`; usa `be16/be32/wr16/wr32` (big-endian).
   - Direcciones de globals en `include/addrs.h` (fuente: `work/names.txt`).
   - **Registros que use el llamante**: devuélvelos como valor de retorno o por
     out-param (p. ej. `g_print_word` devuelve el estado y entrega `A5` por
     puntero). Documenta la decisión en el comentario.
   - **Funciones que devuelven por CCR** (terminan en `clr.l d0`/`tst.w d0`): expón
     el estado `D0 = 0/1` y que el llamante bifurque por él (`print_word` es el caso).
   - Efectos fuera de imagen (trap/hardware): van al **ledger de eventos** del kit
     (`os_setpalette`…), no al diff de bytes.
   - Esperas (VBL, IKBD): se liberan con `schedule`/`wait_sites` y el candidato
     sondea con `sched_poll8` (ver `test/test_vbl.py`).
4. **Test diferencial** en `projects/kult/recreate/test/test_*.py`:
   ```python
   diffs, info = differential(ENTRY, {"a5": X, "d0": Y, "_pokes": {...}},
                              lambda lib, buf: lib.g_fun_XXXX(buf, ...))
   assert not diffs, report(diffs)
   assert <out-param> == info["regs"]["<reg>"]   # registros de salida
   ```
   El harness compara **toda la imagen** (y el ledger de eventos); los registros se
   comprueban explícitamente. Fija `schedule=`/`wait_sites=` para esperas.
5. **`make test` en verde.**
6. **Actualizar el % y la frontera en ESTE documento**, y la lista de `TODO.md`.

### Reglas de oro
- **Sin verificación no cuenta**: una función está hecha solo si su test
  diferencial pasa. Si no se puede verificar todavía (p. ej. espera hardware no
  modelada), se deja pendiente y se anota el bloqueo.
- **Nada a medias**: no se mezcla una función a medio portar con las verificadas.
- **Casos degenerados**: documenta los que el original no maneja (p. ej. `0x12ffa`
  con `d3=0` desborda a 65536 iteraciones; en el juego nunca ocurre).
- **Commit por tarea:** una tarea F4 termina con commit si `make test` sale
  **100 % verde**; **no** commitear si queda algo pendiente de verificar por el
  usuario (política en `AGENTS.md` §"Relevo inmediato").

---

## Entorno mínimo

- Kit: `tools/recreate_kit/` (harness + oráculo Musashi + modelo de traps). El
  proyecto vive en `projects/kult/recreate/` (`Makefile` → `kit.mk`, `src/`,
  `include/addrs.h`, `test/`).
- Desensamblador propio: `tools/prg_dis.py`. Decompilación: `work/decomp.c`
  (solo como pista, **no** para fronteras). Nombres: `work/names.txt`.
- Binario: `work/disk_a/KULT.PRG`. Direcciones Ghidra = offset de imagen + `0x10000`.

## Fuente de verdad de lo ya hecho

Cada `test/test_*.py` fija su `ENTRY` y su caso; ahí está la lista real de
funciones verificadas (354 casos / 326 funciones). Resumen por bloque:

- Pantalla: `copy_screen`, `swap_screens`, `screen_address` (`0x10fec`/`0x10fee`),
  blit de sprite (`0x124f8`), copias front/back (`0x121b6`/`0x121c6`), fila
  (`0x16b76`/`0x12190`).
- Texto: `advance_text_entry`, `position_text`, `print_char`, `print_word`,
  `0x10946`/`0x10998`/`0x109bc`/`0x1090e`/`0x10b24`/`0x10b46`, `0x10fb4`,
  `0x10aee`, overlays `0x109fe`/`0x10a88` y marcos `0x10a54`/`0x10acc`,
  `0x10768`, prompt `0x122ee`, overlay `0x11016`/`0x11020`.
- Caja de texto (bloque COMPLETO): líneas `0x12ffa`/`0x13216`/`0x12fee`/`0x1320a`,
  copias `0x130a4`/`0x13162`, guardar/restaurar `0x1258a`/`0x124f2`, cuerpo
  `0x10e66`, orquestador `0x10440`, `prompt_wait` `0x12292`.
- Blit de texto: `0x11196`/`0x11050`/`0x11102`, `0x14138` y satélites, `0x12fb6`.
- Despacho: `run_command_keep_sp` (`0x103ae`) + `FUN_00010630` (`0x10630`),
  `0x10390`, `0x186ee`, con los handlers de byte reconstruidos.
- Input/loader: `ikbd_handler`, `wait_frame` (`0x13698`), GEMDOS
  `fopen/fcreate/fread/fwrite/fclose`, `load_resource`, `load_fond`,
  `load_named_screen`.
- Iconos/estado: `0x16556`/`0x16514`/`0x1652e`/`0x164d4`/`0x164f6`,
  `0x18c8e`/`0x18ce0`/`0x18cb2`/`0x182f0`/`0x18316`, `0x14a98`/`0x14bdc`,
  `0x15192`, pila de llamadas `0x1350c`/`0x13522`/`0x13552`, y stubs `rts`.
- **Elementos (nuevo):** `0x126e0` (rejilla al descriptor `0xd8f0`), `0x1276c`
  (relleno de patrón), `0x11ece`/`0x11ee8` (bucles de filas `0x12466`),
  `0x18d28` (marco del objeto seleccionado), y el subárbol `0x1285a`/`0x1296c`/
  `0x129c8`/`0x128a4` (comando de elemento: clase -> bitmap `0x3b1ae` -> entrada
  de icono `0x120a8`).
- **Handlers de comando (nuevo):** `0x121fa` (icono + trazo 2x15), `0x1228a`
  (`prompt_wait`+A6), `0x12436` (máscaras + cuerpo `0x12466`), `0x127fa`/`0x127e0`
  (save enmascarado a `0xd8f0`), `0x12240`/`0x1224e` (espera con botón),
  `0x12a4a` (bitmap `0x3b1ac` desde `0x23e9c`/`0x41fb0`), `0x12d96`/`0x12dd8`
  (blits de texto), `0x12ea2`/`0x12e08` (revelado por caja -> `0x12e2e`),
  `0x13370` (rombo de revelado con `0x130a4`/`0x13162`), `0x13474` (icono +
  `0x13370`), `0x134e4` (copia de rejilla) y `0x14748` (despacho por `0x1acc6`).
- **Dispatchers `0x11xxx` (nuevo):** `0x11f02` y `0x11f2e` completos. El encadenado
  de la salida de `0x128a4` (`d2`=0x1a52e, `d3`=0x1a52c, `a1`=0x1a528,
  `a0`/`d7` = `screen_address(x,y)`) se reconstruye de los globals, sin tocar
  `0x128a4`.
- **Revelado por caja (nuevo):** `0x13d0e` (trazo Bresenham sobre `0x55b02`),
  `0x13e14`/`0x13eae` (pasadas con VBL), `0x132aa` (128a4+127e0+13370+12466),
  `0x13490` (13e14) y `0x134b2` (13eae).
- **Bucle principal (parcial):** `run_command` (`0x103a8`, guarda A7 + cae en
  `run_command_keep_sp`), `handle_frame_input` (`0x1031c`), y la selección por
  cursor `0x1478a`/`0x14798`/`0x147ce`.
- **Bucle principal (COMPLETO):** `script_step` (`0x17e3c`, salidas tempranas
  verificadas; el resto documentado) y `game_loop` (`0x101be`). Con ellos, el
  resto del árbol: `0x1778e`/`0x177bc` (búsqueda de objeto), `0x15ac8`/`0x1103e`
  (banco de animación/blit), `0x16636`/`0x166da`/`0x166ec` (lista de objetos),
  `0x14886`/`0x14832` (hit-test), `0x16582` (update_icon_list), `0x1042c`
  (draw_text_box), `0x17856` (animación del objeto), `0x18740` y `0x18dc0`.
  **`game_loop` se verifica forzando una iteración y saltando los caminos
  complejos** (`0x1ad0b`/`0x1acd2`/`0x1acd4`/`0x1ab60` a valores que cortan).
- **Bloque `0x14xxx`/`0x17xxx` (nuevo, en curso):** `0x1442c` (cmd idx 74, wrapper
  que preserva `0x1acc7`), `0x1443e` (fija modo + marcador en `0x1a695` y expone
  A4/A5 que usa `0x1460a`), `0x14496` (camina la tabla de modos `0x1ad38` y
  empaqueta los objetos en `0x1ab62`) y `0x14588` (reset de globals de comandos);
  `0x14926` (búsqueda de clase en `0x1aa72`) y `0x14966` (hit-test de la tabla de
  acciones `0x1a6ae`); `0x159e2`, la entrada gemela del intérprete de animación
  (cuerpo común con `0x159d2`, ahora `anim_run`); `0x167be` (bucle de espera con
  hit-test, doble `wait_frame` + VBL) con sus entradas `0x167aa` (cmd idx 77) y
  `0x1680c` (devuelve A0); y `0x1695a` (nombre del objeto en `0x19970`). También
  `0x16460`/`0x164a4` (reconstrucción de la tabla de iconos de pista); el save
  `g_fun_1258a` se partió en `g_fun_12590` (descriptor en A1) porque `0x164a4` lo
  usa con el buffer `0x3a556` (la zona de `init_hardware`). Tests en
  `test/test_mode.py`, `test/test_objwait.py`, `test/test_icons.py` y
  `test/test_handlers.py`.
- **Búsqueda de objeto `0x17792`/`0x177e4`/`0x1780c` (nuevo):** el cuerpo de `0x1778e`
  es `0x17792` con la máscara en D3 (ahora `g_fun_17792`; `g_fun_1778e` delega con
  0x3f); `g_fun_177e4` fija `0x1a81a`/`0x1acc9`/`0x1a81e` (lo comparten `0x177bc` y
  `0x1780c`) y `0x1780c` (D3 = 0 indexa directo; D3 != 0 busca, y sin coincidencia
  salta a `0x1018e`, testeado con `stop_pc`). Tests en `test/test_loop.py`.
- **Transiciones de texto `0x16b2e`/`0x16dbc` y umbral `0x14f24` (nuevo):**
  `0x16b2e` (OR de filas de 4 words desde `0xd8f0+A5` sobre A3) y `0x16dbc`
  (0x10fec + 0x10f8e + 0x126e0; devuelve A0, avanza A6 y deja A5 = 0) en
  `test/test_screens.py`/`test/test_textblit.py`; `0x14f24` gestiona `0x1ad20` y
  dispara `0x159e2` con D0/D1/D2 = 0x4a/0x44/0x47 (test en `test/test_anim.py`).
- **Estado de hints/objetos y wipes de texto (nuevo):** `0x16f90` (suma 0x101a8 a
  `0x1a752..0x1a82e`), `0x17094`/`0x172ca` (tabla `0x1aa72` vs clases 0x50/0x51 y
  entradas `0x19b46`/`0x19c8b`), `0x17930` (entrada `0x19978` + `0x10dfa`/`0x11020`)
  y los wipes `0x16c00` (→ `0x12e2e`) y `0x16c48`/`0x16c6c` (4 esperas de VBL; A6
  ya avanzado por `0x16dbc`). Tests en `test/test_loop.py`, `test/test_textblit.py`
  y `test/test_textbox.py`.
- **Sonido/PSG (nuevo, tras desbloquear el kit):** `0x1929a` (`ym_write`:
  selecciona D0 y escribe D1; el mixer 7 hace RMW leyendo `$ff8802`) y `0x19282`
  (reset: 0 en registros 0xd..0 y 0x18 en el mixer). El kit ahora **sirve la
  lectura de `$ff8802` como bus abierto (`OS_PSG_DATA_READ` = 0xFF)** — el chip
  está inactivo ahí y el STF real devuelve el byte pull-up — sin ledger ni
  rechazo; **modela el bloque PSG completo** `$ff8800..$ff88ff` (espejo cada 4
  bytes en accesos BYTE: `(addr&2)==0` select, `==2` data; lecturas de vuelta solo
  con `(addr&3)==0`) para que `movep.l` del ISR PCM (que escribe
  `$ff8800/$ff8802/$ff8804/$ff8806`) decodifique; y añade `hw_sub8` para el
  `subq.b` sobre `$fffa1f`. También subió `OS_SCHED_MAX` 8→256.
- **Driver de sonido (nuevo):** `0x1909e` (apagado: PSG + Timer A), `0x19084`
  (para Timer A), `0x18fc4`/`0x18fc6` (`sound_timer_start`; cuerpo con D1),
  `0x1906a` (vector `0x190f6`) y los envoltorios Supexec `0x18f7a`/`0x18f84`/
  `0x18f8e`/`0x18f98`/`0x18fa2`/`0x18fa4` (+ `0x18f6e`), todos con la puerta
  `$1ab4f`. ISR del Timer A: `0x190b8` (carga de bloque), `0x1910e` (PCM hacia
  delante) y `0x190f6` (hacia atrás), verificados con `stop_pc=0x19144` (el `rte`).
  Tests en `test/test_sound.py`.
- **Bloque de música `0x3axxx` (COMPLETO, 19/19):** init `0x3a556` (`init_hardware`),
  `0x3a5c2` (Supexec(`mfp_setup`) + `Setexc` de los traps #3/#4), `0x3a604`
  (`mfp_setup`: IERA/IMRA, TACR/TADR, vector `$134`), `0x3a62e` (carga la canción de
  la cabecera `0x3b1ac`), `0x3a734` (comandos de canal; despacho por `0x3a57c` con
  sus codelets), `0x3a6f4`/`0x3a712` (parada del Timer A), el generador de notas
  `0x3a992` y el **tick del tracker** `0x3aab6` (`trap #4`) con sus 5 handlers de
  comando (`0x3ad5a`/`0x3ad6a`/`0x3adb4`/`0x3ae00`/`0x3ae68`). Handlers PSG del
  `trap #3`: `0x3aa7c` (write-verify del chip), `0x3aa38` (registro D2+D7 o par
  2*D7+D2 con bit15; RMW del mixer) y `0x3aa8a` (tabla sombra `$3ae7c`); `0x3a85e`
  silencia los canales 8/9/10. El candidato llama a la reconstrucción del handler
  (no hay traps en C) y el oráculo llega por el vector que pinta `Setexc`.
  **Trampas transcritas:** hay tres máscaras de bit de canal distintas — `t` en
  `0x3a734` bit7, `t<<3` en `0x3ac68`/`0x3ae00` y `(t<<3)|t` en el bit6 y en el
  fin de canción de `0x3aab6`; `0x3ae68` **no** reescribe `channel[8]`; `rol.b #2`
  del comando 3 va a la izquierda. Tests en `test/test_music.py`.
- **Formulario de texto `0x14xxx` (HECHO):** `0x142cc` (lista de blits en `$d8f0` desde
  la tabla `0x1a7b6`, caja `0x1a56c..6f`, stride `0x1a534`), `0x143e2` (blit por
  linea con VBL + blit enmascarado de la caja) y entradas `0x141f4`/`0x141fa` (x4) y
  `0x14246`/`0x1424c` (x2). Trampa: `$d900/$d902/$d904` **son** campos del segundo
  registro de la lista. Tests en `test/test_textform.py`.
- **Nombre del objeto (HECHO):** `0x1687e` (sin sonido), `0x168a0` (sonido 0xe6 +
  espera Cnecin) y `0x168d4` (bit 6 del color limpio); deps `0x1695a`/`0x10fb4`/
  `0x10b6c`/`0x1228a`/`0x1232e`. Los bancos de overlay/texto (`0x41586`/`0x41b76`/
  `0x3c534`) deben estar poblados o el printer no termina.
- **Seleccion de samples y su menu (HECHO):** `0x15492`/`0x15510` (tabla `0x1a8f0`,
  registro en `0x1a6ae`, A2 avanza 0xa por coincidencia) y `0x14db2` (bucle
  IKBD/hit-test hasta clic + Timer A) con entradas `0x14d76`/`0x14d90`/`0x149da`.
  Trampa: el `movem` de `0x15492` restaura D6 (contador = nº de coincidencias) y
  `$1a488` lo fija cada entrada. Tests en `test/test_select.py`.
- **Familia `0x151b0` (en curso):** `0x15052`/`0x15382`/`0x15530` (comandos) y
  `0x15934` (copia con `screen_ptr_a`); `0x14f24` devuelve flag (2 = salta al
  llamante). **HECHOS `0x150ae`** (accion de objeto: tabla `0x1ab62`+2, `0x1ad09 ==
  0x12` sale por `rts`, marca `0x1a772[1]` y despacha `0x90aa`; 5 casos), **`0x152ee`**
  (espera de clic `0x167be` + animacion con el cursor: entrada `0x1ab68`, defecto
  `0x1ab5a`, directo `0x1ac00`), **`0x15428`** (nombre + Cnecin: D4 de `0x1ac04`/
  `0x1ab56`, override `0x113` con `0x1acec >= 0x3f && 0x1acd2 < 0x16`, bit 15 ->
  despacho) y **`0x151b0`** (transicion por filas: swap de longs + VBL `0x13594` por
  fila, busqueda de objeto clase `0x28` por fila y `0x159e2` d1=0x26; el `movem` de
  `0x15228` usa la mascara INVERTIDA del predecremento (`#$3810` = d2-d4/a3) y su
  restore sobreescribe d2/d4 antes del `addq.b` de fila). Tests en
  `test/test_family151.py`.
  **Familia COMPLETA**: `0x155dc` (reloj: `0x134da` + HH:MM:SS con `0x10838` y salida
  por clic; `test/test_handlers.py`), `0x15594`/`0x155c4` (cortan el sonido `0x18fa4`
  y llaman a los cargadores de sala; devuelven por CCR: carry = error → despachan
  `0x918f`/`0x9190` y si no `0x90aa`), `0x1566a` (anillo de iconos: mascara A6[1],
  rects en `0x19ad6`, hit-test `0x1478a`, salida por clic) y `0x15888` (bucle del
  anillo: guarda `$1a4e0`/A6, corta el bucle con nibble 0xa/0xc o `$1abf8`; el test
  cubre la salida con `the_command == 0xffff`).
- **Carga/guardado de sala `0x16ed8`/`0x16fb4` (HECHO):** lee/escribe `SCENA.BIN`
  (`0x1a414`): `0x291c` B en `0x1a752`, `0x7d00` en `screen_ptr_b`, `copy_screen`,
  paleta, otro `0x7d00` en `screen_ptr_b`; en la vuelta, `0x16f90` (hints +`0x101a8`)
  y `0x16460` (tabla de iconos). Guardar hace el camino inverso (`0x16556` +
  resta `0x101a8` + `Fcreate`/`Fwrite`/`Fclose`). Los dos devuelven por **CCR**
  (`ori.b #$1,ccr` en el error → el C devuelve 1/0) y las primitivas GEMDOS
  (`0x107b6..0x10828`) ahora **devuelven el valor del trap en D0**
  (`fileio.c`). Trampa del test: `SCENA.BIN` se lee sobre `0x1a752..0x1d06e`, así
  que pisa `$1acc6` (byte `0x2574` del fichero) y la lista de objetos `0x1a7ca`;
  el fichero sintético los rellena. Tests en `test_fileio.py`.
- **Tanda 2 de cierre (nuevo):** `0x13f2a` (pinta `0x24982`+`0x5c` y copia), `0x129d6`
  (bloques de `0x31072` → `0x3b1b0`, 12ad6/12b04), `0x14e66` (menu de overlay con
  `0x1258a`/`0x11020` y bucle hasta clic), `0x16310` (ciclo de paleta hasta wrap y
  acercamiento al target `0x1a10a[$1acf4]`; busy-wait de 65536 por pasada → el test
  necesita `max_insns=3M`), `0x14f5e` (umbral + ciclo), `0x16abe` (rejilla 16dbc +
  16b2e por bandas de 0x1e filas) y `0x14f74` (rejilla + espera `0x167be` + cola
  `0x15022`; ojo: `$1acd2 == 1` evita el 0x9005). Tests en `test_loop.py`,
  `test_element.py`.
- **Tanda 1 de cierre (nuevo):** `0x169b4` (espera `0x1680c` + comando `0x9183` y
  mapeo del byte 2 de la clase 0x5d/0x85/-0x2a por la tabla de A1) con sus envoltorios
  `0x1699c`/`0x169a4`/`0x169ac` (el ultimo suena 0x16), `0x16250` (pista 0x18a10 +
  `Setcolor(0,0x777)` + VBL + `Setcolor(0,0)`), `0x13686` (copia el paquete IKBD),
  `0x14ee8` (cmd idx 135: parametro de la entrada de objeto o tabla `0x1ab50`),
  y `0x18f0c` (cola de samples de `0x18e12`; los offsets son relativos a **`$192be`**,
  no a 0x55b0a — importante para el uso desde `0x16a20`) con su entrada `0x16ab6`.
- **Sala/`reset_clear` y pistas (nuevo):** `0x181f0` (acumula 0x198c0 en 0x3b1ac
  con topes 0x258/0x190, dibuja 16 segmentos con `0x13d0e` y repite; ojo: el `dbf`
  apunta a `0x18258`, el refresco de D3 es POR PASADA), `0x1842c` (prepara la sala:
  llena/limpia `screen_ptr_b`, `0x181f0`, `copy_screen`, `0x182f0`, `0x18334`,
  `0x159e2`, carga `FOND.PI1` con reintento `0x918e`, paleta, repite y encadena) y
  `0x1682a` (`reset_clear`: carga `A:CLEAR.BIN` reintentando, resetea **A7 = 0x1d1fe**
  —el `bsr 0x16210` deja su retorno en `0x1d1fa`, el C lo escribe—, `$1ad0f = 0xff`,
  `0x16210` y `$1acd8 = 2`; salta a `0x10158`, los tests usan `stop_pc`). Tambien
  `0x171fa`/`0x173f2` (seleccion de pista A/B por la tabla `0x19cd1..0x19cf7` ->
  `$1acaa`/`$1acae`, limpia el bit7 del objeto y `0x177bc`/`0x17856`; **`0x17856`
  deja A6 = A1+3**, o `0x55b05` si corre el bloque del bit 7) y `0x175f6` (sella
  `$1a812` y despacha `0xc029`/`0xc165` con `$1acff` 5/6). Tests en `test_loop.py`.
- **Bloque `0x17xxx`/resto `0x18xxx` (nuevo):** `0x1817a` (marca el objeto y cae en
  `0x18192`), `0x1881e` (copia 0x3c filas de +0x5500 con VBL por fila + `0x164a4`),
  `0x1834a`/`0x18342` y `0x18334` (256 descriptores de `0xd8f0`: mascara 0x7fff7fff,
  escala `0xcffff/(v-0x138)`, ORs 0x8000 por tramos, 0x65 pasadas), `0x171b4`/
  `0x173ae` (seleccion + temporizador A/B), `0x17678` (limpia la entrada `$1a812` y
  anima `0x19cfa`), `0x18128`/`0x179a2` (clase 0x30 -> entrada de `0x1a9d4` + `0x184d6`/
  `0x184cc` y despacho 0x9148/0x9147/0x9032) y `0x178cc` (consume `$1a812[4]` ->
  `0x90aa`/`0x9140`). Tests en `test_loop.py`.
- **Bloque `0x18xxx` (HECHO en su mayoria):** wrappers `0x1857a` (cae en `0x1857c`),
  `0x18888`/`0x18890` (A4 = 0x8000/0, cuerpo `0x18896`: banco `0x15ac8` + `0x11030`),
  `0x1816e` (cae en `0x18192`: pista 5 si `$1acc7==8`, objeto 0xbe y tabla
  `0x19dbd..0x19e19` -> `0x159d2`), `0x18a10` (avanza `0x2741e` y llama al driver),
  `0x184cc`/`0x184d6`/`0x1851a` (marcan la entrada de sample `$1a75e` y animan) con
  `0x18538` (walk de `0x27fe2` + `$1ad2c`), `0x18cfc` (baja 5 a bytes 50/51 del
  objeto y repinta), `0x186b6` (busca en `0x1a8f0`), `0x1870a` (pista por `0x2741e`
  hasta `(byte&7)!=7`), `0x18e12`/`0x18e06` (trio `0x19994` -> 10 pares de pitch/vol
  en `0xd8f0` + lista de samples en `0x192d2` desde `0x55b0a`) y `0x16902`
  (nombre + pista + espera; cierra la dep de `0x16902`). Tests en `test_loop.py`.
- **Cluster de "modo de escena" `0x14602`/`0x1460a`/`0x146d0` (HECHO):** `0x1460a`
  (formulario `0x141fa` con `$1ad0f & 0x38 == 8`, `$1acc8 = $1ad0f & 7`, `0x188ce`,
  `0x1443e` con A4/A5, busqueda del objeto `byte4 == ($1acc8|0x88)` -> `$1acc9`/
  `$1a572 |= indice`, `0x13f86`/`0x13f56`/`0x16460`/`0x17094`/`0x172ca`/`0x17480`/
  `0x16582`); `0x146d0` (`addq.l #1,a6`) y `0x14602` (`1460a` + cuerpo `0x146d2`:
  iconos 1..3, `0x15934`/`copy_screen`, sonido 0x16 -> `0x18f8e(0x83)` + `$1ab4f=1`,
  `0x14748`, paleta, `0x1857c`/`0x18944`, `0x1424c` con `$1acc9`, `0x16556`).
  Sus deps nuevas: `0x188ce` (anima 0x87/0x96 via `0x177bc`+`0x17856`),
  `0x17480` (accion rotatoria `$1ad00` + tabla `0x2741e` -> `$1abf8` a018/a019),
  `0x1857c` (busca `[0x40][$1acd2]` en `0x1a8f0`, blitea y cuenta `0x80` en `$1acf7`)
  y `0x18944` (repeticion con `0x19d21`/`0x19d27` + `0x16582`). Tests en
  `test_mode.py` (cluster) y `test_loop.py` (188ce/17480/1857c/18944).
  **Trampa del test:** `0x1443e` fija `$1acc6 = modo`; `0x13f86` solo termina con
  `$1acc6 == 0x87` (el test usa esa entrada de la tabla `0x1ad38`).
- **BUG corregido en `0x13d0e` (revelado por `0x132ec`/`0x1332e`):** las dos
  correcciones de paridad del paso en x tenian los `beq`/`bne` invertidos (`a0 -= 6`
  cuando impar / `a0 += 6` cuando par). Los tests previos (fase d0&0xf == 0) no lo
  cubrian; ahora `test_line.py`/`test_handlers.py` lo fijan.
- **Envoltorios/entradas pequeñas (nuevo):** `0x134da` (rama compartida 128a4+124da),
  `0x1104a`/`0x127f0` (blit de texto + save), `0x12324` (GEMDOS Cnecin),
  `0x12fea` (cae en `0x1222e`), `0x16512`/`0x16554`/`0x16576` y `0x18d26`
  (avanzan A6 y caen en su cuerpo), `0x17472` (limpia globals).
- **Texto/vídeo `0x10xxx` (nuevo):** `0x10ff4` (offset sobre A0), `0x104f4`
  (comando inline + walk_entry), `0x10682`/`0x1066e`/`0x10668` (parser de argumento
  de texto + wrappers), `0x10dfa` (caja de texto + icono), `0x11030`/`0x110da`
  (cuerpos de blit fwd/rev sin avance) y `0x10b52`/`0x10b6c` (ventana de texto,
  ~650 B). Trampa: `0x10fb4` **consume un byte** y forma el índice con `rol16(d4,8)`
  cuando `d4 < 4`; el bucle de impresión de `0x10b6c` reescribe los cursores tras
  cada pasada antes de testear `0x1a508`.
- **Bloque `0x19xxx` (hardware, parcial):** hechos `0x1929a`/`0x19282` (PSG) y
  `0x1906a`/`0x19084`/`0x1909e` (Timer A), mas los envoltorios Supexec y los ISR.
  Siguen **NO viables** las rutinas del FDC (`0x19312`/`0x19316`/`0x1935a`/
  `0x193e4`/`0x19438`/`0x19462`/`0x19474`: `$ff8604` + Supexec + `$ff8800`); la HAL
  la bypass-ea y el port nativo la omite, asi que el kit no modela el FDC.
- **Blitters de overlay (nuevo):** `0x111b6`/`0x111ac` y `0x114ba`/`0x114b0`
  (1–4 planos → `0xd8f0` + `0x123a0`), `0x117fe`/`0x117de` (dos fuentes A0/A1
  con `d6 = ror32(0x80000000,d0)`) y `0x11afa`/`0x11ada` (variante `roxl` con
  `d6 = ror32(0x80000000, 0x1f/0x0f - q)` y control por `0x1a53e`). Trampas: el
  mapeo del nº de longs por pasada es 1 plano→3, 2→7, 3→0xb, 4→0xf (¡no al
  revés!); el tail de `0x117fe` hace `movem` restore y `andi.w #$f,d0` (la fase
  vuelve a 0–15, no a 16); y el avance persistente de A0/A1/A2 por fila incluye
  los `+8` del bucle interno (stride efectivo), no solo el `adda` de fin de fila.
- **Escalado/revelado `0x12/13xxx` (nuevo):** `0x12f28` (reveal pixelado con paso
  0x11 mod total), `0x13a18`/`0x13bc2` (escaladores 16.16 con `roxl` a 4 planos),
  `0x13702`/`0x13740` (preparaciones + bucle animado), `0x1398e`/`0x138a6` (lista
  de 8 filas + animación con VBL) y `0x15944`. Trampas: el avance 16.16 de
  `0x13a18` **intercambia mitades** (`add.w` + `swap` + `add.w` + `swap`); el
  `add.w` de `0x15944` **no propaga el acarreo** a la palabra alta (A2 vaga en la
  ventana de 64K; modelarlo como `a2 = (a2 & 0xffff0000) | ((a2 + word) & 0xffff)`
  o el candidato escribe fuera del buffer y segfaultea); `0x15944` **recarga A1 en
  cada pasada** (`swap d7` deja su word a 0 tras la primera); en `0x13740` el
  `124da` final usa el A0/D7 **restaurados por `movem`** (los de 128a4, no los de
  `10fee`); `0x1398e` escribe por grupo 1 entrada `[word off][word cnt][long]` + 7
  longs (su `dbf` interno salta a 0x139e8, no al inicio del grupo); `0x138a6`
  compara `cnt >> 2` con la pasada y solo termina cuando una pasada no toca nada.
- Varios: paleta (`0x16300`/`0x162e2`/`0x162d8`), blit de fila enmascarado,
  cursor, `0x13698`.

## Frontera / siguientes pasos (en orden)

1. **Despacho `0x11xxx` (HECHO)**: los 4 blitters (`0x111b6`/`0x111ac`,
   `0x114ba`/`0x114b0`, `0x117fe`/`0x117de`, `0x11afa`/`0x11ada`) y los 6
   dispatchers que los encadenan (`0x11f64`, `0x11f98`, `0x11fd8`, `0x1200a`,
   `0x12044`, `0x12076`) más los scrolls `0x11dee`/`0x11e5e` y los wrappers
   `0x11df8`/`0x11e68` (flag=0, pendientes de test directo). Los handlers ya están
   cableados en la tabla de bytes (`0x19f62`, ver frontera 5).
2. **Revelado por caja grande (HECHO)**: `0x13d0e` (trazo Bresenham sobre
   `0x55b02`), `0x13e14`/`0x13eae` (pasadas con VBL), `0x132aa`, `0x13490`,
   `0x134b2` y **`0x132ec`/`0x1332e`** (69/73 esperas de VBL, con `OS_SCHED_MAX`
   = 256; ahí se destapó el bug de paridad de `0x13d0e`).
3. **Bucle principal (HECHO)**: `script_step` (`0x17e3c`) y `game_loop`
   (`0x101be`), con todas sus dependencias inmediatas. Queda pendiente el
   **camino de sonido de `0x18dc0`** (llama a `0x18f7a`, frontera Sonido/MFP): se
   verifica con `0x1ab4f != 0`, donde el original también se salta el Supexec.
4. **Bloque `0x14xxx`–`0x17xxx` (EN CURSO)**: cerrado el cluster
   `0x1442c`/`0x1443e`/`0x14496`/`0x14588` (modo de escena + lista de objetos),
   más `0x14926`/`0x14966` (clase y tabla de acciones), la entrada gemela `0x159e2`
   del intérprete de animación, `0x167aa`/`0x167be`/`0x1680c`/`0x1695a` (espera
   con hit-test y nombre del objeto), `0x16460`/`0x164a4` (tabla de iconos),
   `0x17792`/`0x177e4`/`0x1780c` (búsqueda de objeto), `0x16b2e`/`0x16dbc`/`0x14f24`
   (transiciones de texto y umbral de animación) y `0x16f90`/`0x17094`/`0x172ca`/
   `0x17930`/`0x16c00`/`0x16c48`/`0x16c6c` (hints y wipes de texto).
   **Pendiente de plumbing A6**: `0x1442c` (cmd 74) lee `A6[1]` y la rama `0xa/b`
   del despacho aún no hila A6 (la firma C ya lo recibe).
   **Hechos los wipes antes bloqueados por `OS_SCHED_MAX`**: `0x16ca6` (34 VBL) y
   `0x16ce2` (68); `0x16d3c`/`0x16d78` (16/15). Sigue bloqueado `0x16b9c` (72 VBL +
   `prompt_wait`, que además necesitaría 72 teclas).
   **Formulario de texto (HECHO)**: `0x142cc` (lista de blits en `$d8f0` desde la
   tabla `0x1a7b6`, caja `0x1a56c..6f` y stride `0x1a534`), `0x143e2` (blit por
   línea con VBL + blit enmascarado de la caja) y las entradas `0x141f4`/`0x141fa`
   (×4 en `$d904`) y `0x14246`/`0x1424c` (×2, corrupción deliberada del registro 1).
   Trampa: `$d900/$d902/$d904` **son** los campos del segundo registro de la lista.
   **El sonido ya NO bloquea el bloque**: los wrappers Supexec/MFP (`0x18f7a`+) y el
   driver del Timer A están reconstruidos, así que `0x149da`/`0x14xxx`, la familia
   `0x151b0`, `0x16a20`/`0x16ab6`, `0x176c0` y `0x17ca4` quedan a tiro de default
   (sus deps restantes son ya funciones pendientes normales).
   **Impresión del nombre del objeto (HECHO)**: `0x1687e` (sin sonido), `0x168a0`
   (sonido 0xe6 + espera) y `0x168d4` (bit 6 del color limpio); deps cerradas
   (`0x1695a`/`0x10fb4`/`0x10b6c`/`0x1228a`/`0x1232e`). `0x16902` sigue esperando
   `0x18e12`. **Selección de samples (HECHO)**: `0x15492` (tabla 0x1a8f0, registro
   en 0x1a6ae con a2 += 0xa por coincidencia y ventana 0x1276c con una espera de
   VBL) y `0x15510`; deps cerradas. **Menú de samples (HECHO)**: `0x14db2` (marco +
   lista + bucle IKBD/hit-test hasta clic + Timer A) y sus entradas `0x14d76`
   (filtro 0x00ff), `0x14d90` (filtro 0x40|$1acd2) y `0x149da` (menú de acciones,
   $1ad0e). Trampas: `$1a488` lo fija cada entrada (`0xf0ff`/`0x00ff`/`0x000f`); en
   `0x149da` el `movem` restaura A5 y lo reescribe en `$1a4e0`. `0x14602`/`0x1460a`/
   `0x146d0` siguen necesitando `0x188ce`/`0x17094`/`0x172ca`/`0x17480`.
   **Gigantes (HECHOS 3 de 4)**: `0x17a1a` (cambio de vista; `0x16210` ahora
   DEVUELVE el byte y se usa tras el `bsr`; `d2/d3` del tramo normal se leen como
   WORD de `0x1a858/0x1a85a`; el descriptor 0x19968 y 11f98+18a30+120e4),
   `0x17ca4` (usar objeto: devuelve entry+1; con `$1ad03 != 0` y `a0[3]==0x10/8`
   usa A1=0x19cfd/0x19d00 como script de 0x17856) y **`0x18a98`** (attract:
   53 entradas de la tabla 0x1971c..0x198c0; gotchas: tras `0x128a4` **A6 =
   retorno (a6+4) = 0x19718** y **D0/D1 = x/y del script**, la PRIMERA entrada se
   salta la relectura de D0/D1 del stream (`bra 0x18aea`) y solo consume `[w][h]`,
   las demas 4 words; `13bc2` recibe (d2=$1a4ac, d3=$1a4aa); el bucle infinito
   0x18c80 (`182f0` + `1834a` con d5=0x64) no se transcribe — los tests cortan con
   `stop_pc=0x18c80`; `test_fun_18a98_first` usa 269 VBLs (el contador es byte, el
   schedule cicla `(k%255)+1`)). Para el kit, **`OS_SCHED_MAX` sube a 512**
   **0x12ce4, `0x14ac6`, `0x14bfa` y `0x16b9c` HECHOS** (ver tests
   `test_fun_12ce4_caso*`, `test_fun_14ac6*`, `test_fun_14bfa`, `test_fun_16b9c`).
   Fixture de `0x16b9c`: `_anim_table_dos()` (tabla 0x41fb0 valida para d4=3 y
   d4=0x77 en la misma pasada; el walk se simula en Python con `getb()` porque los
   pokes son words) + `_wipe_common` + `_vbl_schedule(0x48)` con site 0x16d24.
   **Formulas de registro ya derivadas** (los blitters guardan/restauran
   d0-d3/a0-a1 por pasada con un `dbra d1` final):
   - `111b6`/`111ac`: passes = d1'+1 con d1'=(d1<<2)-1 (el envoltorio fija d2=5,
     d3-=1). Final: D0=entrada; D1=(alta)|0xffff; D2=(alta)|(d2+passes); A0=entrada;
     A1=entrada - 2*#pasadas con (d1&3)==0 (d1'..0).
   - `117fe`/`117de`/`11ada` (flag 0x1a53e=1): cola `addq #1,d0; andi #$f` + `+8` a
     A0/A1 cuando D0 envuelve (cada 16 pasadas), D2-=passes. Con 11afa (flag=1) la
     parte de D0/A0/A1 se SALTA. Verificado con el caso 3: entrada
     D0=0,D2=0x14,A0=0x81448 → bloque tras 117de {D0=4, D2=0x2c, A0=0x81458,
     A1=0x79458} y tras 11ada {D0=0, D2=0x2c, A0/A1=entrada}.
   - Las palabras altas de D0/D2 NO son cero (p. ej. caso 1 en el call a 111ac:
     D0=0x07fd0000, D2=0xffffffff) y vienen de 14496/12a4a: hay que modelarlas o
     medirlas por caso.
   **HECHO**: `g_fun_12ce4` (element.c) con los 4 casos y tests
   (`test_fun_12ce4_caso0/1/2/3`; VBLs 88/82/72/72; stop_pc
   0x12ce2/0x12c06/0x12c6c/0x12d94). Los registros que los casos 1/2 necesitan
   (D0=0x07fd0000, D2=0xffff0001, D6=0x05010506 en el 114b0; D6=0xff34 en el
   114ba) resultaron CONSTANTES frente al contenido de pantalla (probado con 3
   patrones): se midieron del oráculo y van como constantes comentadas. Las
   formulas de los bloques 0x1a82e/0x1a83e estan en `g_fun_12ce4`.
   **Tanda 3 (HECHA)**: `0x18a30` (limpieza Alternar Setscreen 5x + copia 0x200 B;
   10 VBL), `0x16a20` (intro/retrato: 1066e, par a 0xd8f0, 16ab6 con a3=0x1998c y
   d7=2, 22 VBL), `0x176c0` (pista + icono + animacion), `0x185e6` (sonido 0xe1,
   nombre 0x3c534 con d4=0x141+(($1acd7&3)|4), espera Cnecin), `0x17c42` (16556 +
   177bc(0xbe) + 176c0 + marcas en 0x1a7ca/0x1a81a/0x1a772 + 16582) y
   **`0x16dec`/`0x189aa`** (ficha de objeto; 1285a, 18e12, walk 0x246e6, 129c8,
   120dc, 124da; 189aa repite 18f8e/16dec/18f98/16dec/13698 hasta clic).
   Trampa de `0x189aa`: la SEGUNDA llamada a `0x16dec` hereda **A6 = retorno de
   la primera (0x1970c)**, no `0x19708` — su selector sale de `0x1970e` (0x36).
   La espera `d1 = ($1aa6e >> 4)` de `0x16dec` es un `lsr.l #4` UNICO seguido
   del bucle `subq/bne` (no desplazar en cada vuelta); con `d4=0xc0` son ~1.6M
   instrucciones (los tests de la ficha usan `d4=0x10`).
   **Familia 0x151b0 empezada**: `0x15052` (selector 0x1ab62/0x1abfe), `0x15382`
   (paleta 0x11 + 0x9000) y `0x15530` (modo + cola 0x15022); `0x14f24` ahora
   devuelve flag (2 = salta al llamante). **HECHO tambien `0x15934`** (variante de
   `0x15944` con screen_ptr_a y 0x35 VBLs). **HECHOS `0x150ae`** (accion de objeto:
   tabla 0x1ab62+2, salida `rts` con $1ad09==0x12, despacho 0x90aa), **`0x152ee`**
   (espera de clic 0x167be + animacion con el cursor; tabla 0x1ab68, defecto
   0x1ab5a, directo 0x1ac00), **`0x15428`** (nombre + Cnecin; D4 de 0x1ac04/0x1ab56,
   override 0x113, bit 15 -> despacho) y **`0x151b0`** (transicion por filas con VBL
   0x13594 y busqueda de objeto clase 0x28; el `movem` de 0x15228 lleva la mascara
   invertida del predecremento: `#$3810` = d2-d4/a3). **HECHOS `0x15594`/`0x155c4`**
   (cortan el sonido y despachan la carga/guardado de sala) y los cargadores
   **`0x16ed8`/`0x16fb4`** (`SCENA.BIN`; devuelven por CCR, GEMDOS con D0).
   **Familia CERRADA**: `0x155dc` (reloj), `0x1566a` (anillo de iconos) y `0x15888`
   (bucle del anillo) también verificados.
5. **Despacho**: cablear más handlers de `cmd_jump_table` (`0x19eca`, 144) y la
   tabla de bytes (`0x19f62`, 106) conforme se reconstruyan.
6. **Sonido (HECHO)**: `ym_write` (`0x1929a`), `0x19282`, el driver del Timer A
   (`0x18fc4`/`0x18fc6`, `0x1906a`, `0x19084`, `0x1909e`), los envoltorios Supexec
   `0x18f7a`/`0x18f84`/`0x18f8e`/`0x18f98`/`0x18fa2`/`0x18fa4` (+ `0x18f6e`) e ISR
   (`0x190b8`/`0x190f6`/`0x1910e`), con `0x18dc0` llamando al driver real bajo la
   puerta `$1ab4f`. El kit sirve el bloque PSG completo (par + sombras + espejos
   cada 4, para `movep.l`), la lectura de `$ff8802` como bus abierto, `hw_sub8` y
   **BIOS `Setexc`** (escribe el vector en la imagen; lo usa `0x3a5c2`).
   **Lo único que el kit no modela** (fuera de F4): el FDC (`0x19312`+; la HAL
   lo bypass-ea y el port lo omite).
   **Los traps `#3`/`#4` de la música ya NO son frontera**: en el 68000 `TRAP #n`
   vectoriza a `n+32`, así que apuntan a los vectores `0x23`/`0x24`, los que
   `0x3a5c2` instala con `Setexc` (`0x3aa38`/`0x3aab6`); `mfp_setup` escribe
   además el vector `0x134` (`0x3a872`) directamente. Verificado en el oráculo:
   `0x3a85e` (`trap #3`) vuelve por `rts`, y `0x3a872` (`trap #4`, ISR: termina
   en `rte` en `0x3a934`) alcanza el checkpoint con `psg_seed={7:…}`. `0x3a872`
   no está en el índice de 406 (Ghidra solo lo ve como dato del vector `0x134`).
   **Bloque `0x3axxx` COMPLETO** (ver arriba): `init_hardware`, `mfp_setup`,
   `Setexc`, carga de canción, generador de notas y el tick del tracker con sus
   handlers de comando. Siguiente por defecto: el resto de pendientes (bloques
   `0x14xxx`–`0x17xxx`, dispatchers y salas).
7. Integrar los núcleos C en la HAL sustituyendo Musashi.

### Trampas conocidas del kit
- **`Crawio` consume teclas:** cada `0x13698` de una espera hace `Crawio(0xff)` y el
  modelo gasta una tecla staged; un `0x169b4` con prompt Cnecin detras de la espera
  necesita varias claves (el test usa 4).
- **Pila del juego visible:** al resetear A7 a `0x1d1fe` (`0x1682a`), el `bsr`
  siguiente deja su direccion de retorno en `0x1d1fa` (fuera de la banda guarda);
  el C del port la escribe a mano para que el diff cuadre.
- **A6 al volver de `0x17856`:** no es el entrante; vale `A1+3` (o `0x55b05` con el
  bloque del bit 7). Los llamantes que devuelven A6 lo calculan a mano.
- **`0x181f0`:** el `dbf` interno apunta a `0x18258`; el `andi`/16230 del prologo se
  ejecutan UNA vez por pasada, no por iteracion.
- **`0x18334` es lento:** cada pasada de `0x1834a` son ~14k instrucciones; el test
  necesita `max_insns=3_000_000` (el tope por defecto de 200k solo cubre ~13 pasadas
  y parece un cuelgue).
- **Banco de texto 0x3ec8a en tests:** las entradas son secuenciales; si se pide mas
  de un indice hay que construir longitudes acumuladas (una entrada len 3 desplaza
  los `skip` de los indices posteriores).
- **A6 no modelado en el C de algunos helpers:** `0x10b6c` deja **A6 += 4**
  (`0x16902`/`0x1870a` lo cuentan a mano: 0x19974 -> 0x19978), `0x1695a` deja
  **A6 = 0x19970** (nombre), y `0x10fb4`/`position_text` dejan `d4 = 0xffff` (no
  observable salvo por registro).
- **`0x18e12`:** la tabla `0x19994` mapea D1 a (d2,d3,d4); el bucle dedup compara
  contra el primer byte del par ANTERIOR (`-2(a1)`) y **no** decrementa el `dbf`
  en los descartes; cierra con `+4`/`+2` en el ultimo par.
- `print_word` devuelve por **CCR** (ver arriba).
- **`DBcc` decrementa y salta cuando la condición es FALSA** (no verdadera); p.
  ej. `dbcs d3,loop` sigue mientras C=0 y sale cuando C=1. La primera versión de
  `0x147ce` lo tenía al revés (un solo caso se detectó al instante).
- **Convenciones de registro encontradas en el bucle:**
  - `0x17856` recibe el puntero de script en **A1** (no A6): el original lo salva
    con `move.l a1,-(a7)` y lo recupera en A6 tras el blit.
  - `0x1a81e`/`0x1a81a` son punteros **a celdas** que contienen el descriptor/objeto:
    hay que desreferenciar dos veces.
  - `0x16636` preserva D2/D3/D7/A0 alrededor de `0x1103e` (`movem #$3180`), así que
    deja esos registros como los recibió; `0x166ec` los consume.
  - `0x177bc` devuelve el flag por **Z** (encontrado → Z=1); el C lo expone como
    retorno `1/0`. `0x16636` devuelve por Z (0xff encontrado / 0 no).
  - `0x14a98` recibe A0 pero lo ignora (los llamantes no lo fijan).
- `0x10fec` **calcula `d7`** (fase de píxel): los llamantes no lo reciben.
- `0x11026` = setup + `g_text_blit_fwd` (cae en `0x11050`). `0x1104a` es otro
  setup (fija `a3 = *(0x1a534)`) del mismo cuerpo.
- **`0x127fa` no toma `d1/d4/d5` como argumentos**: los almacena a partir de la
  salida de `0x12698` (`d1`→`0x1a49e`, `d5`→`0x1a4a0`, `d4`→`0x1a4a2`); los
  únicos argumentos son `a0,d2,d3,d7`. La firma C refleja esto.
- **`0x127f0` RESUELTO.** El fallo anterior era un poke mal elegido (se poteaba
  el descriptor `0xd8f0` con un rango que pisaba datos del test); con un buffer
  limpio y `0x1a534` fijado pasa. `0x127e0` y `0x127f0` están verificados.
- `0x1285a` termina en `jmp 0x10768` (rts) cuando no encuentra la clase: el
  estado se expone como "encontrado" y el no-encontrado deja `d0/d1/d3` iguales
  (pero `d2` = `clase & 0xf8`, porque `andi.b` solo toca el byte bajo).
- **Salidas tempranas de `0x128a4`** (`0x1294c`): hacen `addq.l #4,a7; rts`
  (el llamante deja un long extra). El harness no modela ese pop: se verifican
  con `stop_pc=0x1294c`.
- **`0x12240`/`0x1224e`**: el `addq.l #1,a6` de salida está en `0x12286`
  (compartido por guardas y fin de bucle); `0x12240` además hace `adda.l #1,a6`
  de entrada (total +2). El bucle interior son 0x33 `wait_frame` por repetición:
  con `OS_SCHED_MAX=8` solo se puede forzar el retorno por botón hasta 8 esperas;
  el conteo completo (0x33×N) queda sin cubrir.
- **BUG corregido en `0x12b04`**: el port enmascaraba `a2` a 16 bits
  (`& 0xffff`) al decrementar; el original usa el registro de dirección de 32
  bits. El test de `0x12b04` lo tapaba porque `differential` **consume**
  `_pokes` del dict y la segunda pasada corría sin imagen; ahora usa un dict
  nuevo por pasada.
- **Degenerado `groups == 1`**: `0x12698` con un ancho pequeño (p. ej. 4 y
  fase 0) calcula `0x1a53c = 1`, y entonces `0x12466` (`subq.w #1,d6` /
  `beq`) gira indefinidamente **también en el original**. El juego no lo usa;
  los tests de `0x11f02`/`0x11f2e` usan ancho 16 (`groups>=2`).
- **`0x13e14` loop2 = `0x1a52c`**: el original hace `subq.w #1,d2` antes del
  `dbf`, así que recorre `0x1a52c` veces; con `0x1a52c = 0` el `dbf` recorre
  65536. Cuidado con el off-by-one: un candidato que haga una espera de más
  cuelga (no hay entrada de `schedule`). Lo mismo en general para los `dbf`.
- **`0x132ec`/`0x1332e` ya no topan con `OS_SCHED_MAX`**: `0x13e14`/`0x13eae`
  con el ancho de un elemento real (`0x1a52e>=3`) necesitan >8 esperas de VBL; el
  kit subió el tope a **256** (os.h), así que quedan pendientes de probar, no
  bloqueados. `0x13490`/`0x134b2` se prueban con d2/d3 pequeños de la tabla del icono.
- **`0x1929a`/`0x19282` y el puerto `$ff8802` (kit)**: una lectura de `$ff8802` es
  **bus abierto**: el chip está inactivo ahí y un STF real devuelve 0xFF (Hatari,
  probado en STF). El kit la sirve como `OS_PSG_DATA_READ` **sin ledger y sin
  rechazo** (shim + os.h + TRAP_MODEL.md + el pin de `test_psg_model.py`). En
  `0x19282` **D1 no se recarga en el bucle**: `ym_write` lo pisa con el resultado
  del RMW del mixer y los registros 6..0 reciben ese valor; el ledger PSG del
  oráculo lo delata si el candidato pasa un 0 constante.
- **`movep.l` del ISR PCM (0x1910e/0x190f6)**: escribe en
  `$ff8800/$ff8802/$ff8804/$ff8806` (movep va de dos en dos), o sea **dos registros
  PSG por long**; el candidato llama `psg_port_write` una vez por par. La tabla de
  pares `[reg1][val1][reg2][val2]` está en `0x19182 + (byte & 0xfc)`: la base del
  `move.l (d16,pc,d0.w)` es la **palabra de extensión** (0x1912e+2+0x52), no la
  instrucción siguiente (0x19184) — un test con la base mal da un ledger PSG
  distinto. Los tres ISR terminan en `rte`, así que se comparan con
  `stop_pc=0x19144`; el epílogo compartido hace `bclr #5,$fa0f` (via `hw_bclr8`).
- **`0x18fc4` usa A2 = `*(0x1a4dc)`** (el fin de la entrada que deja `0x10f8e`) y
  publica `0x192c6`/`0x192c2`; con `$1a57c == 0xff` se salta la carga del recurso y
  conserva el banco de la tabla `0x1a4e8`. El `tst.b $fa19` de `0x19084` lee 0 en
  el modelo (el TACR no es una lectura modelada), igual que el original.
- Tablas fuera del texto cargado (p. ej. overlays `0x41b76`/`0x41586`, la de la
  caja `0x3ec8a`) hay que pokeárlas en el test; con índices grandes (p. ej. el
  cmd `0x916b` usa la entrada `0x16b`) hay que rellenar cientos de entradas.
  Los bancos de samples `0x55b0a`/`0x1a4e8` también están a cero en la imagen base.
- `XBIOS Ikbdws` no-op; el spin de `wait_frame` (`0x13698`) y las esperas de VBL
  se modelan con `schedule`/`wait_sites` + `sched_poll8` en el sitio del compare.
  Para N esperas seguidas (p. ej. `0x11ece`) hay que dar una entrada por
  espera con `nth=k` y un valor distinto cada vez, o el bucle gira.
- La rama `0xf000` del despacho **restaura `A7` desde `$1a826`**: en el test hay
  que pokear `$1a826` con `harness.emu.STACK_TOP` o el `rts` se va a basura.
- **`0x1443e` termina en `bra 0x14588`** (tail call) y comparte sus salidas
  **A4/A5** con `0x14496`: `0x1460a` recorre la lista de objetos `[A5, A4)` con
  `cmpa.l`, así que el C las expone por out-param. `0x14496` **reintenta**
  `walk_entry` mientras `0xd8eb != 0x52`; nada del camino lo cambia, así que en
  los tests se pokea `0xd8eb = 0x52` (con otro valor giran original y candidato).
- **`0x159e2` es entrada gemela de `0x159d2`**: comparten cuerpo (`anim_run`) y el
  prólogo de `0x159d2` solo aporta D1/D0/D2 del script; el D0/D1/D2 del oráculo hay
  que pasarlos en `regs` (por defecto 0 → `walk_entry` recorre 0xffff entradas y el
  run no llega al `rts`).
- **`0x167be` es un bucle con dos esperas distintas**: `wait_frame` (`0x13698`,
  sitio `0x136b0`) se llama **dos veces por iteración** (dentro de `0x10522` y
  directo) y la VBL de `0x1031c` (`0x1359a`). El test da dos entradas `nth=1/2` al
  paquete IKBD con bit0 (→ clic) y una al contador de VBL; `0x1ab4f=0` evita la
  segunda espera de `0x1031c` y los selectores A/B ya pintados evitan los overlays.
  `0x1680c` devuelve **A0** = `*(0x1a772)` (lo usa `0x169b4`).
- **`0x1258a`/`0x12590`**: el mismo cuerpo. `0x1258a` solo hace `a1 = 0xd8f0` y cae
  en `0x12590`, que escribe la cabecera en **A1** y devuelve **A4 = A1 final**
  (`movea.l a4,a1`). `0x164a4` lo llama con A1 = `0x3a556` (¡la zona de
  `init_hardware`!) y `0x16460` hila el A1 devuelto por cada objeto; el `push a2` de
  `0x164a4` existe porque `0x12590` machaca A2 (en C A2 es local del llamante).
- **`0x1780c` camino no-encontrado**: cuando `D3 != 0` y `0x17792` no encuentra el
  objeto, el original salta a `0x1018e` (salida/Pterm) sin escribir; el candidato
  sale sin tocar la imagen y el test lo compara con `stop_pc=0x1018e`.
- **`0x14f24` se salta a su llamante**: la rama de disparo hace `addq.l #4,a7`
  (descarta la dirección de retorno) antes de `bsr 0x159e2`, así que el `rts` final
  vuelve dos niveles arriba; esa rama se verifica con `stop_pc=0x14f5c`.
- **`0x16dbc` avanza A6** (`addq.l #1,a6`) y deja A5 = 0, además de devolver A0:
  los llamantes (`0x16abe`, `0x16d3c`) esperan A5 a 0 y lo usan como offset del
  descriptor `0xd8f0`; `0x16c48` usa el **A6 ya avanzado** en el cuerpo `0x16c6c`,
  por eso la firma C lo recibe in/out. La tabla `0x41fb0` está a cero en la imagen
  base, así que los tests de texto la pokean.
- **`0x17094` usa el A1 entrante** en el corte temprano (`0x1acd2 == 8` y bit7 de
  `0x1acfc`): salta el `movea.l #0x1aa72,a1`, así que limpia `a1`, `a1+170` y
  `a1+175`. El resto del cuerpo sí fija A1 = `0x1aa72`. La firma C recibe A1.
- **`0x16ca6`/`0x16ce2` sin verificar por `OS_SCHED_MAX`**: el `dbf d7` con d7 =
  0x21/0x43 hace 34/68 esperas de VBL; `0x16c6c` (d7 = 3) sí entra (4 esperas).
- **Direcciones fuera de la imagen (barrido guardián).** El oráculo acota TODA
  dirección al bus de la imagen (fuera lee 0 / descarta escrituras); el candidato
  indexa `uint8_t *` crudo. `test_fun_10b52` usaba coordenadas `y` sintéticas (0..2)
  y el borde hacía `y-7` con acarreo, generando una dirección de ~10 MB fuera de la
  imagen: el oráculo leía 0 y el candidato, memoria del heap, lo que producía un
  crash de worker esporádico (ASLR). Corregido el test (`y = 0x10`); con el plugin
  guardián del kit adaptado a Linux la suite entera queda limpia (303/303). El
  proyecto **no** tiene bus helper: asume direcciones dentro de la imagen.
- `0x10440` calcula el alto de la caja como `d5*6+7` (`lsl #2` + 2× `add`), no
  `d5*5+7`.
- El bloque PSG entero (`$ff8800..$ff88ff`) se modela en accesos BYTE (select/data
  por el bit 1; lectura de vuelta por `(addr & 3) == 0`, bus abierto en el resto),
  así que ya no se rechaza ningún acceso byte; siguen rechazados los accesos anchos
  y un select > 0x0f. Un `rts` pelado (p. ej. `0x135e0`, `0x160c0`) es una función
  no-op verificable sin más.
