# screens.c

screens.c — buffers de pantalla y direccionamiento planar.  copy_screen       @ 0x121c6 : copia screen_ptr_b -> screen_ptr_a (8000 longs). copy_screen_a6    @ 0x121c4 : `addq.l #1,a6` y cae en copy_screen. swap_screens      @ 0x121de : intercambia los dos buffers long a long. screen_address    @ 0x10fec : dirección planar de (x,y) sobre screen_ptr_a (d0<<2). screen_address_raw@ 0x10fee : idem sin el `lsl.w #2` (d0 ya escalado). screen_offset     @ 0x10ff4 : cuerpo común; suma el offset a A0 (dado por el llamante).  Todo el efecto es la imagen (o el valor devuelto): sin hardware, verificable por el diff de bytes. Las `screen_address*` devuelven A0 (dirección) y entregan D7 (fase de píxel dentro de la palabra) por out-param, según PLAN §4.1 (primario = retorno, resto = out-param). 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x121c6` | `copy_screen` | copia 0x1f40 longs de screen_ptr_b a screen_ptr_a. | test_dispatch, test_screens |
| `0x121c4` | `copy_screen_a6` | `addq.l #1,a6` y cae en copy_screen. Devuelve A6 avanzado. | test_screens |
| `0x121de` | `swap_screens` | intercambia screen_ptr_a y screen_ptr_b long a long. | test_screens |
| `0x10ff4` | `screen_offset` | cuerpo común: A0 += ((d0 & 0xfff0) >> 1) + (d1 & 0xffff)*0xa0; D7 = d0 & 0xf. Devuelve A0 y entrega D7. | test_screens |
| `0x10fee` | `screen_address_raw` | screen_address_raw: A0 = screen_ptr_a y cae en screen_offset. | test_screens |
| `0x10fec` | `screen_address` | screen_address: `lsl.w #2,d0` y cae en screen_address_raw. | test_screens |
| `0x10fec` | `screen_address_off` | screen_address @ 0x10fec: ST planar address of (x, y) on screen_ptr_a. d0 = x pixels, d1 = y rows; returns the byte address in a0 (d7 = pixel-in-word). | — |
| `0x159bc` | `row_palette` | FUN_000159bc @ 0x159bc: copy D3+1 rows of 16 bytes (4 longs) from A1, advancing A1 by 16 a row, into A2 with a 0xa0 destination stride.  Pure image code. | — |
| `0x1350c` | `script_push` | FUN_0001350c @ 0x1350c: salta a la plantilla número (a6[2]<<8 | a6[1]): a6 = TEMPL+d0. El word leído es el destino (no se usa aquí). Devuelve el A6 nuevo por out-param. | test_loader |
| `0x13522` | `script_pop` | FUN_00013522 @ 0x13522: como 0x1350c pero antes apila el A6 de retorno (a6+3) y el fin de script actual ($1a4e0) en la pila de llamadas en 0x1a7d6. | test_loader |
| `0x13552` | `script_call` | FUN_00013552 @ 0x13552: desapila de 0x1a7d6: restaura $1a4e0 y A6 (vuelve de 0x13522). | test_loader |
| `0x13568` | `load_fond` | carga FOND.PI1 (fondo de sala) en screen_ptr_b | test_loader |
| `0x13572` | `load_named_screen` | prepara buffer (len 0x7d22) y llama a load_resource | test_loader |
| `0x121b6` | `copy_screen_rev` | FUN_000121b6 @ 0x121b6: copia 0x1f40 longs de screen_ptr_a a screen_ptr_b (la inversa de copy_screen / 0x121c6). | — |
| `0x12190` | `mask_row_blit` | mask row blit | — |
| `0x16b76` | `row_blit` | FUN_00016b76 @ 0x16b76: copia D3+1 filas de (D2+1)*8 bytes desde A3-0x8000 a A3, con stride de destino 0xa0. | — |
| `0x16b2e` | `or_rows` | FUN_00016b2e @ 0x16b2e: OR de una fila de (D2+1) grupos de 4 words desde el descriptor 0xd8f0+A5 sobre el destino A3: por grupo lee 4 words (movem.w), calcula la mascara ~(d4|d5|d6|d7) y escribe ((dst & mask) | word) en 4 words consecutivos. Destino avanza 0xa0 por fila (D3+1 filas), fuente 0x40. | — |
| `0x15944` | `copy_rows` | mide y dibuja una caja de texto autodimensionada. 15944, walk_entry sobre la tabla 0x24982 (indice 0x1acc7-1), recorre bloques de 3 bytes: los bytes 0x32..0x... | — |
| `0x15934` | `copy_rows_a` | copy rows a | test_family151 |
| `0x18a30` | `screen_clean` | screen clean | test_loop |
| `0x18a98` | `attract` | attract | test_loop |

## Estado (globales A_* usadas)

- `A_CALL_STACK`
- `A_CUR_FILENAME`
- `A_ELEM_1A52C`
- `A_ELEM_1A52E`
- `A_LOAD_BUFFER`
- `A_LOAD_LENGTH`
- `A_PALETTE_PTR`
- `A_SAVE_DESC`
- `A_SCREEN_PTR_A`
- `A_SCREEN_PTR_B`
- `A_TEMPL_DATA`
- `A_TEMPL_PTR`

