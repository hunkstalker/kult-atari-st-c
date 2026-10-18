# handlers.c

handlers.c — handlers de comando pequenos (0x121xx-0x127xx).  FUN_0001228a @ 0x1228a: prompt_wait y avanza A6 en 1. FUN_000121fa @ 0x121fa: selecciona el icono A6[1] (0x120e4), lo blitea (0x1215e), calcula la direccion de (x,y) del descriptor (0x10fec) y blitea 2x15 en ella; A6 += 2 y limpia 0x1a484. FUN_00012436 @ 0x12436: copia 0x1a49e/0x1a4a0/0x1a4a2 a las mascaras 0x1a538/53a/53c y cae en el cuerpo de 0x12466 con a0=*(0x1a4b2), a1=0xd8f0, d3=*(0x1a4a4). FUN_000127fa @ 0x127fa: guarda una fila enmascarada del buffer A0 en el descriptor 0xd8f0 (previa 0x12698 + 0x123c0), D3 filas con stride 0x40. FUN_000127e0 @ 0x127e0: A0 -= 0x8000, guarda (0x127fa) y reblitea (0x124da). 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x1228a` | `prompt_wait_entry` | — | test_handlers |
| `0x121fa` | `icon_blit_handler` | — | test_handlers |
| `0x12436` | `mask_rows_handler` | — | test_handlers |
| `0x127fa` | `masked_save_row` | — | test_handlers |
| `0x127e0` | `masked_save` | — | test_handlers |
| `0x134e4` | `copy_grid` | FUN_000134e4 @ 0x134e4: copia una rejilla desde A1 a pantalla en (D0,D1)-0x8000: (D3) filas de (D2>>2) celdas de 8 bytes. Depende de 0x10fec (screen_address). | test_handlers |
| `0x13370` | `reveal_diamond` | FUN_00013370 @ 0x13370: revela una caja (D0,D1,D2,D3) con cuatro corridas (0x130a4/0x13162) que van cerrando un rombo; por iteracion actualiza los ocho bordes 0x1a516/518/51a/51c/51e/520/522/524 y espera 0x3e8 ciclos (sin efecto). | test_handlers |
| `0x13474` | `icon_reveal` | FUN_00013474 @ 0x13474: si 0x1a570 != 0 cae en 0x1222e; si no, revela la caja del icono A6[1] (0x120e4 + 0x13370) y A6 += 2. | test_handlers |
| `0x14748` | `elem_mode_blit` | FUN_00014748 @ 0x14748: segun el modo 0x1acc6 lanza 0x12d96 (0x09/0x66) o 0x12dd8 (0x5f/0x67). | test_element |
| `0x12240` | `wait_button_a` | — | test_handlers |
| `0x1224e` | `wait_button_b` | — | test_handlers |
| `0x11f02` | `blit_dispatch_a` | FUN_00011f02 @ 0x11f02: con flag 0x1a570 se corta a 0x134da (128a4+124da); sin el, 128a4 + 126e0 (compone el bitmap en 0xd8f0) y 11ece (d1=alto-1, a1=0xd8f0 + (alto-1)*0x40). | test_handlers |
| `0x11f2e` | `blit_dispatch_b` | FUN_00011f2e @ 0x11f2e: idem pero 11ee8; el segundo 0x10fec usa y = *(0x1a530)+alto-1 y a1 = 0xd8f0. | test_handlers |
| `0x11dee` | `scroll_up` | FUN_00011dee @ 0x11dee / FUN_00011e5e @ 0x11e5e: desplazan el elemento D1+1 pasadas copiando D3 filas con 0x123c0 (hacia arriba/abajo) y rebliteando la fila del buffer trasero; 0x1a53e (long) controla el descenso por pasada. | test_handlers |
| `0x11e5e` | `scroll_down` | — | test_handlers |
| `0x11f64` | `overlay_dispatch_1` | FUN_00011f64 @ 0x11f64: 128a4 + 124da (a1 = bitmap + stride - 2, d2 = 1) + 111ac (d0 = fase, d1 = ancho - 1) sobre a1 - 2. | test_handlers |
| `0x11f98` | `overlay_dispatch_2` | FUN_00011f98 @ 0x11f98: como 0x11f64 pero el segundo blit es 114b0 y usa el borde derecho del elemento (x + ancho - 1) recalculado con 0x10fec. | test_handlers |
| `0x11fd8` | `overlay_dispatch_3` | FUN_00011fd8 @ 0x11fd8: icono A6[1] (0x120e4) + 11e5e + 123c0 sobre a0 - 0xa0. | test_handlers |
| `0x1200a` | `overlay_dispatch_4` | FUN_0001200a @ 0x1200a: icono + 11dee sobre el borde derecho (d1 = icono[10] + alto - 1) recalculado con 0x10fec. | test_handlers |
| `0x12044` | `overlay_dispatch_5` | FUN_00012044 @ 0x12044: icono + 117de (d0 = cookie con d7 en la palabra baja, d1 = d2 = icono[10], d3 = alto) + 1215e. | test_handlers |
| `0x12076` | `overlay_dispatch_6` | FUN_00012076 @ 0x12076: como 0x12044 pero con 11ada. | test_handlers |
| `0x132aa` | `reveal_box_build` | 0x132aa: 128a4 + 127e0 + 13370 + 12466 (la rama 0x1a570!=0 es 0x134da). | test_handlers |
| `0x132ec` | `reveal_box_wait_a` | 0x132ec: como 0x132aa pero con 0x13e14. | test_handlers |
| `0x1332e` | `reveal_box_wait_b` | 0x1332e: como 0x132aa pero con 0x13eae. | test_handlers |
| `0x13490` | `icon_reveal_h` | 0x13490: revela la caja del icono A6[1] con 0x13e14. | — |
| `0x134b2` | `icon_reveal_v` | 0x134b2: revela la caja del icono A6[1] con 0x13eae. | — |
| `0x103a8` | `run_command` | FUN_000103a8 @ 0x103a8: guarda A7 en 0x1a826 y cae en run_command_keep_sp (0x103ae). | test_dispatch, test_handlers |
| `0x1031c` | `handle_frame_input` | FUN_0001031c @ 0x1031c (handle_frame_input): espera un frame; mientras el flag 0x1ab4f este puesto, el cursor este bajo (>=0x96) y el contador de VBL sea impar, repite. Luego restaura el cursor (0x1060a), repinta los marcos de overlay A/B (0x10a54/0x10acc) si cambiaron sus selectores, y dibuja el cursor (0x10526). | test_family151, test_handlers, test_objwait |
| `0x14798` | `rect_hit_test` | FUN_00014798 @ 0x14798: devuelve el C si (d0b,d1b) cae en el rect [a4..a4+3] (bytes). FUN_0001478a @ 0x1478a carga (cursor_x>>2, cursor_y) y cae en el. | — |
| `0x1478a` | `rect_hit_test_a` | — | — |
| `0x147ce` | `cursor_select_b` | — | test_handlers |
| `0x147c2` | `cursor_select_a` | FUN_000147c2 @ 0x147c2: variante de 0x147ce sobre la tabla 0x19b26 con el contador inicial D2 = 0xa. | test_handlers |
| `0x14926` | `class_find` | FUN_00014926 @ 0x14926: busca en la tabla de clases 0x1aa72 (5 B/entrada, 0x29 entradas) la primera cuyo byte 1 & 0xf == 0x1acc9; si la encuentra deja 0x1a772 = entrada y 0x1ace6 = indice (1-based); si no, 0x1ace6 = 0. | test_handlers |
| `0x14966` | `action_bar_hit_test` | FUN_00014966 @ 0x14966: hit-test de las D3 entradas de 10 B de 0x1a6ae contra el cursor. Encontrada: the_command = word[6], 0x1ab46 = byte[4], 0x1a48a = 0, 0x1ace9 = byte[8] y 0x1a75e = 0x1a8f0 + (byte[8]-1)*6. Si no: the_command = 0, 0x1ab46 = 0x64, 0x1a48a = 1. D3 < 2 falla sin probar la segunda entrada. | test_handlers |
| `0x15588` | `mode_flag_wrap` | — | test_dispatch |
| `0x155b8` | `mode_queue_wrap` | — | test_dispatch |
| `0x134da` | `elem_blit_vbl` | FUN_000134da @ 0x134da: rama compartida por los dispatchers 0x11xxx con 0x1a570 != 0: 0x128a4 y rebliteo (0x124da). | test_handlers |
| `0x167be` | `obj_wait_hit` | FUN_000167be @ 0x167be: bucle de espera (hasta clic, bit2 de 0x1a456). Guarda (D2,D3) en 0x1a74e/0x1a74f y hace 0x10522 (espera frame + dibuja cursor) UNA VEZ; luego itera: 0x13698, hit-test de objetos (0x14886), 0x1031c (restaura + repinta cursor). No toca A6; lo comparten 0x167aa, 0x1680c y varios comandos. OJO: 0x10522 dibuja el cursor SIN restaurar; si se llamara dentro del bucle dejaria una replica del cursor por VBL (rastro). | test_objwait |
| `0x167aa` | `obj_wait` | FUN_000167aa @ 0x167aa (cmd idx 77): A6[1] elige origen/cursor (0x14a98), A6[2]/A6[3] son D2/D3, A6 += 4 y cae en el bucle de espera. | test_objwait |
| `0x1680c` | `obj_wait_a0` | FUN_0001680c @ 0x1680c: espera con origen/cursor 6 y (0xb8,0x90); devuelve A0 = puntero de clase 0x1a772 (lo consume 0x169b4). | test_objwait |
| `0x1695a` | `obj_name_table` | FUN_0001695a @ 0x1695a: formatea el nombre del objeto seleccionado en 0x19970. Con clase (a0[0]) < 0x23 usa a0[1] tal cual y marca 0xf4 en 0x19973; si no, a0[1]-0x14 y 0xd4. Fila = a0[2]-0x28 en 0x19972 y a0[1] ajustado en 0x19971. | test_handlers |
| `0x1687e` | `obj_name` | FUN_0001687e @ 0x1687e: nombre del objeto sin sonido ni filtro. | test_nameprint |
| `0x168a0` | `obj_name_snd` | FUN_000168a0 @ 0x168a0: nombre con sonido (0xe6) y espera de tecla. | test_nameprint |
| `0x168d4` | `obj_name_clean` | FUN_000168d4 @ 0x168d4: como 0x168a0 pero sin sonido y con el bit 6 de 0x19973 (el color del nombre) limpio. | test_nameprint |

## Estado (globales A_* usadas)

- `A_1AB44`
- `A_1AB47`
- `A_1AB48A`
- `A_ANIM_1A4A0`
- `A_BLIT_GROUPS`
- `A_BLIT_MASK_FIRST`
- `A_BLIT_MASK_LAST`
- `A_CLASS_IDX`
- `A_CURSOR_X`
- `A_CURSOR_Y`
- `A_DAT_1A484`
- `A_DAT_1A49E`
- `A_DAT_1A4B2`
- `A_DAT_1A534`
- `A_DAT_1A74E`
- `A_DAT_1A74F`
- `A_DAT_1A75E`
- `A_DAT_1ACD4`
- `A_DAT_1ACE9`
- `A_ELEM_1A52C`
- `A_ELEM_1A52E`
- `A_ELEM_1A530`
- `A_ELEM_1A532`
- `A_ELEM_CLASS`
- `A_FRAME_1AB4F`
- `A_INPUT_BUTTONS`
- `A_LIST_1A8F0`
- `A_OBJ_TBL`
- `A_RUN_STACK`
- `A_SAVE_1A4A2`
- `A_SAVE_1A4A4`
- `A_SAVE_DESC`
- `A_SELECTED_OBJECT`
- `A_SEL_1ACC9`
- `A_SKIP_TRANSITION`
- `A_TBL_1A6AE`
- `A_TEXT_SEL_A`
- `A_TEXT_SEL_B`
- `A_THE_COMMAND`

