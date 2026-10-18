# maskblit.c

maskblit.c — familia del blit de filas enmascarado y tabla de hints.  blit_mask_setup  @ 0x12698 : precalcula las máscaras de borde (0x1a538 primera palabra, 0x1a53a última, 0x1a53c nº de grupos de 4 palabras) para un blit de D2 px con fase D7. blit_mask_row    @ 0x123c0 : blitea una fila de A1 a A0: primer grupo de 4 palabras enmascarado con 0x1a538 (OR 0x1a53a si hay un solo grupo), grupos centrales literales, último enmascarado con 0x1a53a. mask_restore_rows@ 0x1215e : corre el blit de fila sobre D3 filas; fuente A0-0x8000 (buffer trasero), stride 0xa0. blit_mask_rows   @ 0x12466 : copia D3 filas desde el buffer 0x1a534 con máscaras de borde; cierra con una espera de VBL. hint_slot_alloc  @ 0x120a8 : busca hueco libre en la tabla de 10 entradas de 16 B (0x1a850) y guarda el registro del icono. hint_select_on   @ 0x120dc : selecciona el icono d0-ésimo (D1=2). hint_select_off  @ 0x120e4 : idem con D1=0. hint_find        @ 0x1213a : busca el word d0 en la tabla y lo desactiva; d5=0/1. hint_blit_entry  @ 0x1222e : selecciona el icono A6[1] y lo blitea; A6 += 2. hint_blit_stroke @ 0x1232e : blitea los iconos 1 y un trazo de 2x15. hint_blit_2/3    @ 0x12368/0x12384 : blitea los iconos 2/3.  Todo el efecto es la imagen; las esperas de VBL se liberan con `schedule`. 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x12698` | `blit_mask_setup` | precalcula las máscaras de borde. | test_maskblit |
| `0x123c0` | `blit_mask_row` | blitea una fila enmascarada. | test_maskblit |
| `0x1215e` | `mask_restore_rows` | corre el blit de fila sobre D3 filas. | test_maskblit |
| `0x12466` | `blit_mask_rows` | copia D3 filas desde el buffer 0x1a534 con máscaras de borde. | test_maskblit |
| `0x120a8` | `hint_slot_alloc` | busca hueco libre en la tabla de 10 entradas de 16 B en 0x1a850 y guarda [word d4][long a0][word d7][word d0][word d1][word d2][word d3]. | test_maskblit |
| `0x120dc` | `hint_select_on` | selecciona el icono d0-ésimo (D1=2). | test_maskblit |
| `0x120e4` | `hint_select_off` | selecciona el icono d0-ésimo (D1=0). | test_maskblit |
| `0x1213a` | `hint_find` | busca el word d0 en la tabla; si lo encuentra lo desactiva (d5=0). | test_maskblit |
| `0x1222e` | `hint_blit_entry` | selecciona el icono A6[1] y lo blitea; A6 += 2. | test_maskblit |
| `0x1232e` | `hint_blit_stroke` | blitea los iconos 1 y un trazo de 2x15 desde la dirección del icono. | test_maskblit |
| `0x12368` | `hint_blit_2` | blitea los iconos 2. | test_maskblit |
| `0x12384` | `hint_blit_3` | blitea los iconos 3. | test_maskblit |

## Estado (globales A_* usadas)

- `A_BLIT_D8EA`
- `A_BLIT_GROUPS`
- `A_BLIT_MASK_FIRST`
- `A_BLIT_MASK_LAST`
- `A_TEMPL_DATA`

