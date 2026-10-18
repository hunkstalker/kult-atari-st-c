# boxdraw.c

boxdraw.c — primitivas de dibujo de la caja de texto (runs de píxeles).  box_hline      @ 0x12ffa : corre horizontal de D3+1 píxeles en (D0,D1) con color D2 (un bit por plano) sobre screen_ptr_a. box_vline      @ 0x13216 : la misma idea pero vertical (a0 += 0xa0 por píxel). box_hline_copy @ 0x130a4 : copia un run horizontal de screen_ptr_b a screen_ptr_a. box_vline_copy @ 0x13162 : idem vertical. box_hline_back @ 0x12fee : box_hline con draw base fijo 0x8000. box_vline_back @ 0x1320a : box_vline con draw base fijo 0x8000.  Layout de 4 planos en grupos de 8 bytes por 16 px ([p0][p1][p2][p3]); el bit dentro del grupo baja 15..0 y el grupo avanza 8 bytes al desbordar. La dirección base la da screen_address_raw (0x10fee). Todo el efecto es la imagen. 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x12ffa` | `box_hline` | run horizontal con draw base 0. Preserva D0/D1/D6/D7 (no observado). | test_boxdraw |
| `0x13216` | `box_vline` | run vertical con draw base 0 (a0 += 0xa0 por píxel). | test_boxdraw |
| `0x130a4` | `box_hline_copy` | copia un run horizontal. | test_boxdraw |
| `0x13162` | `box_vline_copy` | copia un run vertical. | test_boxdraw |
| `0x12fee` | `box_hline_back` | box_hline con draw base fijo 0x8000. | test_boxdraw |
| `0x1320a` | `box_vline_back` | box_vline con draw base fijo 0x8000. | test_boxdraw |

## Estado (globales A_* usadas)

- `A_SCREEN_DRAW_BASE`

