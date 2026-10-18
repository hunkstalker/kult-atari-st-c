# textbox.c

textbox.c — el cuerpo de la caja de texto.  FUN_00010e66 @ 0x10e66: dibuja el marco (0x12ffa/0x13216), coloca el cursor en la primera línea y, para cada línea que quepa, imprime ';' + la línea (0x10b24) + '<' hasta que print_word señale el final de la cadena ($1a508 = 0xff). Después pinta el borde inferior. A5 lleva el puntero al banco de texto (lo fija position_text). 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x10e66` | `text_box_body` | marco + palabras + borde inferior de la caja de texto | test_textbox |
| `0x10440` | `draw_text_box_variant` | idem (otra ruta de entrada) | test_textbox |
| `0x1042c` | `draw_text_box` | calcula geometria de caja de texto y la dibuja | test_textbox |
| `0x10dfa` | `text_box_icon` | text box icon | test_textbox |
| `0x10b6c` | `text_window` | ventana de texto con adorno/flecha (bits 0xe0) | — |
| `0x10b52` | `text_window_entry` | FUN_00010b52 @ 0x10b52 (cmd idx 78): text index de A6[1] sobre el banco 0x3c534, copia 0x1a482 a 0x1a484 y dibuja la ventana (0x10b6c). Devuelve A6 + 5. | test_textbox |
| `0x13f86` | `scene_text_box` | caja/overlay autodimensionado de la escena | test_textbox |

## Estado (globales A_* usadas)

- `A_COLOR_TABLE`
- `A_CURSOR_TEXT_X`
- `A_CURSOR_TEXT_Y`
- `A_DAT_1A484`
- `A_DAT_1A49E`
- `A_DAT_1A4B2`
- `A_ELEM_1A528`
- `A_ELEM_1A530`
- `A_ELEM_1A532`
- `A_SCREEN_DRAW_BASE`
- `A_SCREEN_PTR_A`
- `A_TEXT_BOX_SPAN`
- `A_TEXT_BOX_W`
- `A_TEXT_BOX_X`
- `A_TEXT_BOX_Y`
- `A_TEXT_END_FLAG`
- `A_TEXT_INDEX`

