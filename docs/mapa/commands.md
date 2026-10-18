# commands.c

commands.c — comandos de escena de la familia 0x151b0 (sin esperas).  Todos empiezan con el umbral 0x14f24 (que puede saltarse el resto del llamante: devuelve 2), fijan `the_command` ($1a50e) desde globals y despachan con run_command_keep_sp (0x103ae). 0x15428 ademas imprime el nombre y espera tecla. 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x15052` | `obj_action_select` | FUN_00015052 @ 0x15052: comando de seleccion (tabla 0x1ab62). | test_family151 |
| `0x14f74` | `grid_wait` | grid wait | test_loop |
| `0x14f5e` | `anim_threshold2` | anim threshold2 | test_loop |
| `0x14e66` | `overlay_menu` | overlay menu | test_loop |
| `0x14ee8` | `cmd135` | FUN_00014ee8 @ 0x14ee8 (cmd idx 135): toma el parametro A6[1] de la entrada de objeto seleccionada en la tabla 0x1ab62 (5 words, +2*param) o, si es 0, de la tabla de reserva 0x1ab50, y despacha. | test_loop |
| `0x150ae` | `obj_action` | obj action | test_family151 |
| `0x152ee` | `cursor_anim_wait` | FUN_000152ee @ 0x152ee: accion de objeto con espera de clic (0x167be). Tras la espera repinta (0x164d4), suena 0x19, anima con el cursor (0x159e2 d1=0x27) y despacha la seleccion (entrada 0x1ab62+6) o el comando por defecto 0x1ab5a. Con $1ad09 != 0 despacha 0x1ac00 directamente. | test_family151 |
| `0x15428` | `name_prompt` | FUN_00015428 @ 0x15428: imprime un nombre y espera tecla (Cnecin). D4 sale de $1ac04 o, si $1ad09 == 0, de $1ab56 (override 0x113 cuando $1acec >= 0x3f y $1acd2 < 0x16). Con el bit 15 de D4 es un comando y se despacha; si no, posiciona con el banco 0x3c534, imprime (0x10b6c), espera (0x1228a) y blitea los iconos 0x1232e. | test_family151 |
| `0x151b0` | `row_transition` | FUN_000151b0 @ 0x151b0: transicion de sala por filas: intercambia longs de la pantalla (grupos de 2 longs, 0x10fec) y espera una VBL por fila (0x13594). En cada fila busca el objeto de la clase 0x28 cuya fila coincide con D4, lo marca (bit 7 de +4), suena (0x1b/0x1a) y lo anima (0x159e2 d1=0x26). Al final repinta (0x164f6), para el sonido (0x18fa4), restaura la paleta y despacha $1ab54. Con $1ad09 != 0 solo despacha $1ac08 sin tocar pantalla. | test_family151 |
| `0x15594` | `load_room` | load room | test_fileio |
| `0x155c4` | `save_room` | save room | test_fileio |
| `0x155dc` | `clock` | FUN_000155dc @ 0x155dc (cmd idx 7): reloj. Inicializa el cursor (0x134da con A6 = 0x19710) y repite: cursor de texto a (0x41,0x78) y base de dibujo a 0, espera VBL (0x13594), imprime HH:MM:SS desde $1ab60 con 0x10838 (el separador 0x1a solo si el resto es impar), espera frame (0x13698) y sale con el clic (bit 2 de $1a456); al final restaura los iconos (0x12368). | test_handlers |
| `0x1566a` | `icon_ring` | FUN_0001566a @ 0x1566a: anillo de iconos de accion. A6[1] es una mascara de 8 bits; por cada bit dibuja un rect [x0,x0+2,y0,y0+8] en 0x19ad6 (base $1a575 $1a576 y tabla 0x19af6) y lo blitea (0x11020 con d4=1). Luego repite: espera frame (0x13698), hit-test de los rects (0x1478a), publica el comando de 3 bytes del script en $1ab46/`the_command`, procesa el frame (0x1031c); con clic restaura el cursor (0x1060a), reblitea el icono pulsado (0x18f8e + 3x 0x11020) y cierra con 0x12436. Devuelve A6 avanzado 2 + 3*(nº de bits). | test_family151 |
| `0x15888` | `icon_ring_loop` | FUN_00015888 @ 0x15888 (cmd idx 98): bucle del anillo de 0x1566a. Guarda el fin de plantilla ($1a4e0) y el script (A6), y repite 0x1566a; con `the_command` a 0xffff (sin acierto) restaura el fin de plantilla y sale. Los comandos con nibble 0xa/0xc y el pendiente $1abf8 cortan el bucle y despachan (el original hace `addq.l #4,a7` + `bra 0x103ae`, asi que el despacho sustituye al bucle). Si no, despacha, cuenta $1ace7 y, tras pasar el 0x2f3c de 0x19316, comprueba el umbral $1acf2/$1ace8 (con 0x15192 al agotarse). | test_family151 |
| `0x15382` | `palette_cmd` | FUN_00015382 @ 0x15382: comando de seleccion con paleta 0x11 y modo 0x9000. | test_family151 |
| `0x15530` | `mode_queue` | FUN_00015530 @ 0x15530: comando de modo/temporizador (dos despachos y cola 0x15022). | test_family151 |

## Estado (globales A_* usadas)

- `A_1AB44`
- `A_1AB47`
- `A_1AB48A`
- `A_CURSOR_TEXT_X`
- `A_CURSOR_TEXT_Y`
- `A_CURSOR_X`
- `A_CURSOR_Y`
- `A_DAT_1A534`
- `A_DAT_1AB40`
- `A_DAT_1AB41`
- `A_DAT_1AB42`
- `A_DAT_1AB43`
- `A_DAT_1AB50`
- `A_DAT_1AB54`
- `A_DAT_1AB5A`
- `A_DAT_1AB5C`
- `A_DAT_1AB60`
- `A_DAT_1AB62`
- `A_DAT_1ABF8`
- `A_DAT_1ACD3`
- `A_DAT_1ACE7`
- `A_DAT_1ACE8`
- `A_DAT_1ACF2`
- `A_DAT_1AD09`
- `A_DAT_1AD0F`
- `A_ELEM_CLASS`
- `A_INPUT_BUTTONS`
- `A_OBJECT_LIST_BEGIN`
- `A_OBJECT_LIST_END`
- `A_PALETTE_PTR`
- `A_SCREEN_DRAW_BASE`
- `A_SEL_1ACC9`
- `A_TEMPL_PTR`
- `A_TEXT_SEL_A`
- `A_TEXT_SEL_B`
- `A_THE_COMMAND`

