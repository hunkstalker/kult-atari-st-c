# element.c

element.c — the element blit helpers around the hint-icon machinery.  FUN_000126e0 @ 0x126e0: compone una rejilla de words en el descriptor 0xd8f0. Borra los 0x627 longs del descriptor, precalcula las mascaras (0x12698) y luego, por cada columna (D2) y fila (D3), lee una palabra del origen A1 (avanzando por el stride 0x1a534), la rota por la fase D7 y la despliega en cuatro words que se OR-ean en el descriptor. La fase avanza de 4 en 4 (nibble); al envolver, la base del descriptor avanza 8 bytes. La funcion restaura A0/A1/D3/D7, asi que solo el descriptor (y los globals de 0x12698) son efectos observables.  FUN_0001276c @ 0x1276c: relleno de patron. Como el anterior pero el word origen es D6 (constante) y escribe directamente en A0 con and/or (no acumula con OR). Avanza 0xa0 por fila; A1 no se usa. Cierra con una espera de VBL (0x13594).  FUN_00011ece @ 0x11ece y FUN_00011ee8 @ 0x11ee8: repiten D1+1 veces el blit de filas 0x12466 incrementando cada vez el numero de filas D3 (empieza en 1) y retrasando A1 0x40 (la primera) o A0 0xa0 (la segunda) por iteracion. 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x126e0` | `grid_fill` | — | test_element |
| `0x1276c` | `pattern_fill` | — | test_element |
| `0x11ece` | `row_blit_a` | — | test_element |
| `0x11ee8` | `row_blit_b` | — | test_element |
| `0x1285a` | `elem_class_find` | FUN_0001285a @ 0x1285a: busca en la tabla de 6 bytes 0x19e19 la entrada cuya clase (byte0) coincide con (*0x1a772)[3] & 0xf8. Si la encuentra copia los bytes 4/5 a 0x1ab4c/0x1ab4d, devuelve por out d0=byte2, d1=byte1, d2=0xff, d3=byte3, y marca encontrado (el llamante 0x128a4 bifurca por el Z del `bsr`: encontrado -> Z=0). Si no, cae en 0x10768 (rts) dejando los registros de entrada intactos. | test_element |
| `0x1296c` | `elem_bitmap_build` | FUN_0001296c @ 0x1296c: construye un bitmap en 0x3b1ae a partir de la entrada (*A5)*10 de la tabla 0x1a252. Cabecera [alto][ancho], luego una fila superior de `ancho` words, (alto-2) filas [izq][medio x(ancho-2)][der] y una fila inferior de `ancho` words. Fija el stride 0x1a534 = ancho*2. A5 avanza 1 (el indice). | test_element |
| `0x129c8` | `elem_list_build` | FUN_000129c8 @ 0x129c8: recorre la lista 0x2751e (via walk_entry con D1) y, por cada bloque de 3 bytes [n][b1][b2] tras el indice, compone el bitmap 0x3b1ae con la fuente de texto desplazada D4*(n) entradas (0x10f8e) y lo blitea en 0x3b1b0+b0*2 con 0x12ad6 o 0x12b04 segun b2 (bit7: stride negativo; bit6: variante invertida). Devuelve A1 = 0x3b1ac (constante que el llamante usa para leer la cabecera). | test_element |
| `0x128a4` | `elem_command` | Exposicion con las salidas de registro (A6/D0/D1) que 0x18a98 necesita: el original deja D0/D1 = x/y del script (o los de 0x1285a en la rama 0xff) y A6 avanzado. El envoltorio clasico devuelve solo A6. | — |
| `0x128a4` | `elem_command_wrap` | — | test_element |
| `0x12a4a` | `elem_bitmap_build2` | FUN_00012a4a @ 0x12a4a: construye un bitmap en 0x3b1ac desde la lista 0x23e9c (walk_entry D1) y la tabla de texto 0x41fb0, bliteando cada bloque de 3 bytes [n][b1][b2] en 0x3b1ac+b0*2 con 0x12ad6/0x12b04 (bit7) y stride fijo 0x28. Fija 0x1a534=0x28, 0x1a530=0x20, 0x1a52e=0x14, 0x1a52c=0x3b y devuelve A1=0x3b1ac. | test_element |
| `0x12d96` | `elem_blit_pair` | FUN_00012d96 @ 0x12d96: dos blits de texto (0x12a4a + 0x10fec + 0x12510) a y=0x20, en x=0x10 (a1+0x12) y x=0x2d, con 11 columnas y stride 0x28. | test_element |
| `0x12dd8` | `elem_blit_pair_vbl` | FUN_00012dd8 @ 0x12dd8: como 0x12d96 pero con 0x124da (que espera VBL) y x=0x10/0x24, 0x14 columnas. | test_element |
| `0x12ea2` | `reveal_box_b` | FUN_00012ea2 @ 0x12ea2: lee la caja 0x1ab40..0x1ab43, fija 0x1a56a=1, 0x1a577=0x11, A6 += 1 y cae en 0x12e2e. | test_element |
| `0x12e08` | `reveal_box_a` | FUN_00012e08 @ 0x12e08: si 0x1a570 != 0 cae en 0x1222e; si no, selecciona el icono A6[1] (0x120e4), fija 0x1a577=0x11, A6 += 2 y 0x1a56a=0x64 y cae en 0x12e2e. | test_element |
| `0x129d6` | `text_block_build` | — | test_element |
| `0x12ce4` | `view_mode_cycle` | — | test_loop |

## Estado (globales A_* usadas)

- `A_ANIM_1A56A`
- `A_ANIM_1A577`
- `A_DAT_1A534`
- `A_DAT_1AB40`
- `A_DAT_1AB41`
- `A_DAT_1AB42`
- `A_DAT_1AB43`
- `A_DAT_1AD0A`
- `A_ELEM_1A528`
- `A_ELEM_1A52C`
- `A_ELEM_1A52E`
- `A_ELEM_1A530`
- `A_ELEM_1A532`
- `A_ELEM_1AB4C`
- `A_ELEM_1AB4D`
- `A_ELEM_1AB4E`
- `A_ELEM_BUILD`
- `A_ELEM_BUILD2`
- `A_ELEM_CLASS`
- `A_ELEM_TBL_19E19`
- `A_ELEM_TBL_19EA9`
- `A_ELEM_TBL_1A252`
- `A_ELEM_TBL_23E9C`
- `A_ELEM_TBL_2751E`
- `A_ELEM_TBL_31072`
- `A_SAVE_DESC`
- `A_SKIP_TRANSITION`
- `A_TEXT_TABLE`

