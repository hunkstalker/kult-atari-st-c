# state.c

state.c — helpers de estado pequeños (limpiezas de flags/globales).  clear_flag         @ 0x172ba : limpia el byte indirecto *(0x1a806) y la word 0x1acaa. clear_object_flags @ 0x15192 : limpia el bit 7 del byte 1 de 41 entradas de 0x1aa72. set_cursor_clip    @ 0x14a98 : fija el clip y el sprite de cursor desde tablas por D0. fix_sample_flag    @ 0x14bdc : cambia 0x81 -> 0x80 en las 0x3f entradas de 0x1a8f0.  Todo el efecto es la imagen. 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x172ba` | `clear_flag` | limpia el byte apuntado por *(0x1a806) y la word 0x1acaa. El oráculo descarta escrituras fuera de la imagen; el candidato indexa crudo, así que acota con el tamaño de imagen (0x100000). | test_state |
| `0x15192` | `clear_object_flags` | limpia el bit 7 del byte 1 de cada una de las 41 entradas de 5 bytes en 0x1aa72, y el flag 0x1acd3. | test_state |
| `0x14a98` | `set_cursor_clip` | fija el origen de recorte (0x19700/0x19702) y el puntero del sprite de cursor (0x199ca) desde la tabla 0x196dc (stride 4) y 0x1949c (stride 64), indexadas por D0. | test_state |
| `0x14bdc` | `fix_sample_flag` | recorre 0x3f entradas de 6 bytes en 0x1a8f0 y cambia 0x81 por 0x80. | test_state |

## Estado (globales A_* usadas)

- `A_CLIP_X`
- `A_CLIP_Y`
- `A_CURSOR_SPRITE_PTR`
- `A_DAT_1ACD3`
- `A_LIST_1A8F0`
- `A_OBJ_TBL`
- `A_PTR_1A806`
- `A_W_1ACAA`

