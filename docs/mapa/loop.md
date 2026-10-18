# loop.c

loop.c — piezas del bucle principal (seleccion de objeto, lista de objetos).  FUN_0001778e @ 0x1778e: busca en la lista de objetos (0x1a7ca..0x1a7ce, 8 B/entrada) la entrada cuyo byte 4 con mascara 0x3f coincide con D0 (byte). Devuelve D2 = indice+1 y D1 = clase encontrada, o D1 = 0xff si no hay. El resultado en el llamante se lee por el Z (encontrado -> Z=1).  FUN_000177bc @ 0x177bc: indexa la tabla 0x1aa72 por el byte D0, fija 0x1a772, calcula la clase (byte3 & 7 | 0x10) y busca (0x1778e). Si la encuentra fija 0x1a81a, 0x1acc9 y 0x1a81e y deja D0 = 0; si no, sale por 0x10768 dejando D0. 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x17792` | `obj_find_masked` | Devuelve 1 si encontrado. d1o = clase (o 0xff), d2o = indice+1. Conserva los bytes altos de d2_in como el original (`move.b`/`addq.b`). | test_loop |
| `0x1778e` | `obj_find` | FUN_0001778e @ 0x1778e: `move.b #0x3f,d3` y cae en 0x17792; el cuerpo vive alli porque 0x1780c lo llama con su propia mascara D3. | test_loop |
| `0x177e4` | `obj_find_setup` | FUN_000177e4 @ 0x177e4: fija el objeto seleccionado A0 en 0x1a81a, su indice (D2, 1-based) en 0x1acc9 y el puntero de icono 0x1a81e = 0x1a776 + (D2-1)*4. Corazón compartido por 0x177bc (que cae en el) y 0x1780c. | test_loop |
| `0x177bc` | `obj_find_class` | obj find class | test_loop |
| `0x188ce` | `anim_mode` | ---- cluster 0x14602/0x1460a/0x146d0 (modo de escena) ---- 0x1460a: con $1a570 == 0 y ($1ad0f & 0x38) == 8 corre el formulario 0x141fa; guarda $1ad0f & 7 en ... | test_loop |
| `0x17480` | `rot_action` | rot action | test_loop |
| `0x1857c` | `sample_find_body` | sample find body | test_loop |
| `0x18944` | `icon_repeat` | icon repeat | test_loop |
| `0x1857a` | `sample_find_wrap` | sample find wrap | test_loop |
| `0x18538` | `sample_walk` | FUN_00018538 @ 0x18538: camina la entrada 0x7f de 0x27fe2 (walk_entry), guarda D3 en su byte +2 y, si D3 es 0x25/0x3a/0x28, incrementa o decrementa $1ad2c segun D4 == 0x80. | test_loop |
| `0x184cc` | `sample_mark_a` | FUN_000184cc @ 0x184cc (cmd idx 116) y FUN_000184d6 @ 0x184d6 (idx 115). | test_loop |
| `0x184d6` | `sample_mark_b` | sample mark b | test_loop |
| `0x171b4` | `sel_timer_a` | sel timer a | test_loop |
| `0x173ae` | `sel_timer_b` | sel timer b | test_loop |
| `0x17678` | `obj_clear_anim` | obj clear anim | test_loop |
| `0x18128` | `class30_dispatch` | FUN_00018128 @ 0x18128: si la clase de $1a772 lleva 0x30, la limpia, elige la entrada ($1ace6-0xa) de 0x1a9d4 en $1a75e y corre 0x184d6. A6 += 1. | test_loop |
| `0x178cc` | `obj_consume` | obj consume | test_loop |
| `0x179a2` | `class30_apply` | FUN_000179a2 @ 0x179a2: como 0x18128 pero con 0x9148/0x9147/0x9032, sella 0x1ad33 y marca $1ad2e. A6 += 1. | test_loop |
| `0x16dec` | `object_info_card` | FUN_00016dec @ 0x16dec: ficha/descripcion del objeto. Resuelve la clase (0x1285a), toma selector D1=A6[2] (0xff/0xfe -> defaults 0x1ab4c/0x1ab4d) y arranca el sonido (0x18e12); recorre la tabla 0x246e6 con 0x12950 y por cada entrada: compone el bitmap (0x129c8), si el siguiente byte es 0xff salta 3 con 0x129d6, elige el icono (0x120dc) y lo blitea (0x124da). Con $1aa6e != 0: si el byte previo es 0x25 avanza la pista (0x18a30/0x16250 segun $1ad03), si no hace la espera de D1>>4. A6 += 4. | test_loop |
| `0x189aa` | `object_info_wait` | FUN_000189aa @ 0x189aa: ficha de objeto interactiva: retira los iconos 2/1 (0x12368/0x1232e), configura la caja de texto 0x19704 (color segun $1ad20), la dibuja (0x134da), y repite {18f8e(0x1c); 16dec(0x19708); 18f98(0x1c); 16dec(0x19708); 13698} hasta que $1a456 tenga el bit 2 (clic); cierra con 0x12368. A6: entry+1/2/3 por los 12368/1232e y el ultimo 12368. | test_loop |
| `0x17a1a` | `view_change` | view change | test_loop |
| `0x17ca4` | `use_object` | use object | test_loop |
| `0x176c0` | `track_icon_anim` | dibuja los iconos de pista, busca el objeto 0xbe (0x177bc), fija $1ad26 y corre 0x176c0; luego busca el objeto $1aa70, marca la entrada (0x53), activa el bit... | test_loop |
| `0x16a20` | `portrait_intro` | portrait intro | test_loop |
| `0x171fa` | `track_select_a` | FUN_000171fa @ 0x171fa: variante A de la seleccion de pista. Ajusta A1 segun $1aa72/$1ab1c (con $1acfc bit7 y eco en $1aa73), busca $1acc6 en la tabla 0x19cd1..0x19cf7 -> $1acaa/$1acac y escribe la entrada en $1aa72, limpia el bit7 del objeto indexado por $1aa73&0xf, fija $1a806 y busca/anima (0x177bc/0x17856). | test_loop |
| `0x173f2` | `track_select_b` | FUN_000173f2 @ 0x173f2: variante B (temporizador B): busca $1acc6 en 0x19cd1..0x19cf7 -> $1acae/$1acb0 y escribe la entrada en $1aa8b, limpia el bit7 del objeto ($1aa8c&0xf) y busca/anima con 0x177bc(0x19)/0x17856. | test_loop |
| `0x17c42` | `obj_refresh` | obj refresh | test_loop |
| `0x185e6` | `track_name_print` | FUN_000185e6 @ 0x185e6: sonido 0xe1, anima el objeto seleccionado (0x159e2 d1=0x2a con +1/+6), elige el indice de pista d4 = 0x141 + ($1acd7&3) (+4 si $1ab60 >= 0xe10), imprime el nombre (0x10fb4/0x10b6c sobre 0x19970 con color 0xb4/0x94), avanza la tabla 0x2741e (0x18e12 con d1=0x22), anima d1=0x2b, espera tecla (0x1228a/0x1232e), sonido 0xe1 y anima d1=0x2d. D0/D2 se preservan alrededor de los helpers. | test_loop |
| `0x175f6` | `obj_seal` | FUN_000175f6 @ 0x175f6: restaura el icono 1, sella la entrada $1a812 con $1acd2 y los comandos a018/3, limpia $1ace7, busca/anima (0x177bc($1ad00)/0x17856(0x19cf7)), repinta listas y copia pantalla; con $1acff == 5/6 despacha 0xc029/0xc165. | test_loop |
| `0x1817a` | `obj_mark` | obj mark | test_loop |
| `0x1834a` | `icon_scale_loop` | icon scale loop | test_loop |
| `0x18334` | `icon_scale` | FUN_00018334 @ 0x18334: 0x65 pasadas de 0x1834a (A5 = 0) con D5 = 0x64..0. | test_loop |
| `0x181f0` | `room_reset` | room reset | test_loop |
| `0x1842c` | `prepare_room` | prepara sala: fondos, reveal, FOND.PI1, paleta, CLEAR | test_loop |
| `0x1881e` | `copy_rows_vbl` | copy rows vbl | test_loop |
| `0x18cfc` | `obj_dec` | FUN_00018cfc @ 0x18cfc: dibuja los iconos de pista, baja 5 a los bytes 50/51 de la lista de objetos, reconstruye la tabla de iconos (0x16460), la lista (0x16582), copia pantalla y avanza A6. | test_loop |
| `0x186b6` | `sample_find` | FUN_000186b6 @ 0x186b6: busca en 0x1a8f0 (desde la entrada A6[1], A6[2] intentos) la primera cuyo byte 0 sea A6[3]; guarda la entrada en $1a75e y A6 += 4. | test_loop |
| `0x18e12` | `sample_list` | sample list | test_loop |
| `0x18f0c` | `sample_queue` | sample queue | test_loop |
| `0x13f2a` | `text_table_paint` | FUN_00013f2a @ 0x13f2a: imprime la tabla de texto 0x24982 con el indice de $1acf1 (0x13f56, d1=0x5c), suena 0x1d/0x1f y copia pantalla. A6 += 1. | test_loop |
| `0x16250` | `hint_color` | hint color | test_loop |
| `0x1699c` | `obj_wait_cmd_a` | obj wait cmd a | test_loop |
| `0x169a4` | `obj_wait_cmd_b` | obj wait cmd b | test_loop |
| `0x169ac` | `obj_wait_cmd_snd` | obj wait cmd snd | test_loop |
| `0x169b4` | `obj_wait_cmd` | obj wait cmd | test_loop |
| `0x16ab6` | `sample_entry` | FUN_00016ab6 @ 0x16ab6: guarda todos los registros y salta a la cola 0x18f0c con el A3/D7 del llamante (0x16a20 fija $192be = 0x64c8e antes). | test_loop |
| `0x18e06` | `sample_pair_a` | FUN_00018e06 @ 0x18e06: D1 = A6[1], cuerpo 0x18e12 y A6 += 2. | test_loop |
| `0x16902` | `obj_name_track` | obj name track | test_loop |
| `0x1870a` | `track_walk` | track walk | test_loop |
| `0x1851a` | `sample_mark_c` | FUN_0001851a @ 0x1851a (cmd idx 117): fija $1a75e a la entrada (A6[1]-1) de 0x1a8f0, A6 += 1 y cae en 0x184cc (A6 total += 2). | test_loop |
| `0x18888` | `bank_blit_a` | bank blit a | test_loop |
| `0x18890` | `bank_blit_b` | bank blit b | test_loop |
| `0x18a10` | `track_advance` | avanza la pista (0x18a10), pinta el color 7 a 0x777, espera una VBL (0x13594) y lo devuelve a 0. | test_loop |
| `0x18192` | `track5_body` | marca la entrada del objeto (byte 44 = 0x53), limpia $1ad0b/$1abf8 y cae en 0x18192. | test_loop |
| `0x1816e` | `track5_apply` | track5 apply | test_loop |
| `0x1780c` | `obj_find_or_exit` | FUN_0001780c @ 0x1780c: A6[1] es la mascara D3 y A6[2] la clase D0. Con D3 != 0 busca el objeto (0x17792) y con D3 == 0 lo indexa directamente; luego 0x177e4 + 0x14926 y, si no hay clase (0x1ace6 == 0), deja 0x1a772 = 0x1aad6. A6 += 3. El camino "no encontrado" del original salta a 0x1018e (Pterm) sin escribir. | test_loop |
| `0x1103e` | `text_blit_setup` | FUN_0001103e @ 0x1103e: fija 0x1a534 = D2*2 y cae en el blit de texto (0x1104a). | test_loop |
| `0x15ac8` | `anim_bank` | FUN_00015ac8 @ 0x15ac8: carga el banco de animacion en *(0x1a4c6) (0x190 longs a cero), recorre la entrada D0 de la tabla 0x27fe2 (walk), copia sus 2 primeros bytes como cabecera, toma el bitmap de 0x28ad2 (0x10f8e) y blitea cada bloque de 3 bytes [n][b1][b2] con 0x12ad6/0x12b04 (bit7), stride = cabecera[0]*2. | test_loop |
| `0x16636` | `obj_list_iter` | FUN_00016636 @ 0x16636: el objeto A2 (byte0=x, byte2=y) pide la clase D3. Busca la clase en la tabla 0x1aa72 (0x29 entradas de 5 B) por byte1&0xf; con el indice avanza el contador de 0x1a57d, carga el banco de animacion en 0xd8f0 (0x15ac8) y lo blitea en el back buffer ((x,y)-0x8000) con 0x1103e. Devuelve D0=0xff si la clase esta, 0 si no. | test_loop |
| `0x166ec` | `obj_list_b` | obj list b | test_loop |
| `0x166da` | `obj_list_a` | obj list a | test_loop |
| `0x10522` | `wait_frame_cursor` | FUN_00010522 @ 0x10522: espera un frame y cae en el dibujo del cursor (0x10526). | test_loop |
| `0x14886` | `hit_test_objects` | FUN_00014886 @ 0x14886: hit-test de la lista de objetos contra el cursor. Si el cursor cae en un objeto con bit7 de su byte4 y (byte4 & D2) == D3, selecciona el icono/objeto y busca su clase en 0x1aa72. Si no, marca 0x1a48a=1 y el selector por defecto. | test_loop |
| `0x14832` | `hit_test` | FUN_00014832 @ 0x14832: hit-test y eleccion del sprite del cursor segun 0x1a48a y los bits de 0x1ad0f. | test_loop |
| `0x16582` | `update_icon_list` | FUN_00016582 @ 0x16582 (update_icon_list): limpia el bit7 de los objetos de clase 0x10; recorre la tabla 0x1aa72 y, para las entradas activas de la clase actual (0x1acd2), marca el objeto correspondiente (bit7 + selector) y lo anima (0x16636). | test_loop |
| `0x17856` | `obj_anim` | FUN_00017856 @ 0x17856: blitea el icono seleccionado (*(0x1a81e)), y si el byte0 del script (A1, que el original salva y recupera como A6) tiene el bit7, fija el cursor con la posicion del objeto (*(0x1a81a)), dibuja la caja de texto (0x1042c) y prepara el "script" de 3 bytes en 0x55b02 [indice|x|y]. Cierra con el interprete de animacion (0x159d2) sobre A1-1. | test_loop |
| `0x18740` | `anim_step` | FUN_00018740 @ 0x18740: logica de fase por el reloj 0x1ab60. Segun el tramo fija 0x1acfd/0x1ad14/0x1acec, reinicia partes de la tabla 0x1aa72 y encola comandos (0x1ac06). Sin llamadas ni efectos fuera de imagen. | test_loop |
| `0x18dc0` | `sound_path` | ---- bucle principal (game_loop @ 0x101be) ---- | test_loop |
| `0x101be` | `game_loop` | do{ ... } while: update subsistemas + comando pendiente + repintado | test_loop |
| `0x17e3c` | `script_step` | ctx: avanza PTR_DAT_0001a822 y compara codigos de script 0x38/0x0c/0x0a | test_loop |
| `0x16f90` | `hint_table_relocate` | FUN_00016f90 @ 0x16f90: suma 0x101a8 a cada long no nulo de la tabla 0x1a752..0x1a82e (0x37 entradas). | test_loop |
| `0x17094` | `hint_state_a` | hint state a | test_loop |
| `0x172ca` | `hint_state_b` | FUN_000172ca @ 0x172ca: como 0x17094 pero para la clase 0x51 y la tabla 0x19c8b (7 B); fija 0x1acae (0xa01e/0xa01f) y 0x1acb0. | test_loop |
| `0x17930` | `obj_text_box` | FUN_00017930 @ 0x17930: monta la entrada 0x19978 (x=0x1a858, y=0x1a85a+0x46, ancho=a1[3]+0x12), la dibuja (0x10dfa) y termina rellenando con 0x11020; a1 = 0x1a75e o 0x1a8f0+(obj[4]-1)*6 segun A6[1]. A6 += 2. | test_textbox |

## Estado (globales A_* usadas)

- `A_1AAB3`
- `A_1AAB4`
- `A_1AAB6`
- `A_1AAD6`
- `A_1AADB`
- `A_1AAE1`
- `A_1AB44`
- `A_1AB47`
- `A_1AB48`
- `A_ANIM_1A4A0`
- `A_CLASS_IDX`
- `A_COMMAND_HINTS_LIST`
- `A_CURSOR_SPRITE_PTR`
- `A_CURSOR_X`
- `A_CURSOR_Y`
- `A_CUR_CLASS`
- `A_DAT_1A484`
- `A_DAT_1A49E`
- `A_DAT_1A4D2`
- `A_DAT_1A534`
- `A_DAT_1A75E`
- `A_DAT_1AAF9`
- `A_DAT_1AB08`
- `A_DAT_1AB60`
- `A_DAT_1ABF8`
- `A_DAT_1AC06`
- `A_DAT_1ACAA`
- `A_DAT_1ACAC`
- `A_DAT_1ACAE`
- `A_DAT_1ACB0`
- `A_DAT_1ACB2`
- `A_DAT_1ACC7`
- `A_DAT_1ACD2`
- `A_DAT_1ACD3`
- `A_DAT_1ACD4`
- `A_DAT_1ACD7`
- `A_DAT_1ACE7`
- `A_DAT_1ACE8`
- `A_DAT_1ACEC`
- `A_DAT_1ACF1`
- `A_DAT_1ACF2`
- `A_DAT_1ACF6`
- `A_DAT_1ACF7`
- `A_DAT_1ACFC`
- `A_DAT_1ACFD`
- `A_DAT_1ACFF`
- `A_DAT_1AD00`
- `A_DAT_1AD05`
- `A_DAT_1AD06`
- `A_DAT_1AD07`
- `A_DAT_1AD09`
- `A_DAT_1AD0B`
- `A_DAT_1AD0D`
- `A_DAT_1AD14`
- `A_DAT_1AD1D`
- `A_DAT_1AD2C`
- `A_ELEM_CLASS`
- `A_FILE_HANDLE`
- `A_FRAME_1AB4F`
- `A_HIT_MASK`
- `A_HIT_VALUE`
- `A_INPUT_BUTTONS`
- `A_MODE_FLAG`
- `A_OBJECT_LIST_BEGIN`
- `A_OBJECT_LIST_END`
- `A_OBJ_TBL`
- `A_OVL_ENTRY`
- `A_SAVE_1A4A2`
- `A_SAVE_DESC`
- `A_SCREEN_PTR_A`
- `A_SCREEN_PTR_B`
- `A_SCRIPT_PTR`
- `A_SELECTED_ICON_PTR`
- `A_SELECTED_OBJECT`
- `A_SEL_1A7D2`
- `A_SEL_1AB4A`
- `A_SEL_1ACC9`
- `A_TEXT_INDEX`
- `A_TEXT_SEL_A`
- `A_TEXT_SEL_B`
- `A_THE_COMMAND`

