# cursor.c

cursor.c — guardado/restauración del área bajo el cursor (16×16).  cursor_draw    @ 0x10526 : guarda el área 16×16 bajo el cursor y pinta el sprite (con máscara y fase de píxel D7). cursor_restore @ 0x1060a : restaura el área guardada.  Todo el efecto es la imagen. La dirección base la da screen_address_raw (0x10fee), igual que en el original (`bsr 0x10fee`). 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x10526` | `cursor_draw` | guarda las 16 filas de 16 B bajo el cursor en A_CURSOR_SAVE_BUF+8 y pinta el sprite. El 4º word de cada media fila se OR-ea solo si el flag A_CURSOR_HOTSPOT (0x1a48a) es distinto de 0. | test_cursor |
| `0x1060a` | `cursor_restore` | restaura 16 filas de 16 B desde A_CURSOR_SAVE_BUF al destino guardado en A_CURSOR_SAVE_DEST (stride 0xa0: 16 B + 0x90). | test_cursor |

## Estado (globales A_* usadas)

- `A_CLIP_X`
- `A_CLIP_Y`
- `A_CURSOR_HOTSPOT`
- `A_CURSOR_SAVE_BUF`
- `A_CURSOR_SAVE_DEST`
- `A_CURSOR_SPRITE_PTR`
- `A_CURSOR_X`
- `A_CURSOR_Y`

