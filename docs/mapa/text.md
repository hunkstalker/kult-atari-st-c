# text.c

text.c — capa de texto: fuente, medida, justificado y overlays.  print_char       @ 0x10838 : dibuja el glifo de D0 en (cursor_x, cursor_y) con la fuente 6x4 en 0x1d202 y la tabla de color *(0x1a4d8); avanza el cursor. La base es screen_address(x,y) menos el draw base *(0x1a46a). print_word       @ 0x108a2 : dibuja la cadena en A5 hasta cur_str_end manejando los flags 0x40/0x80/0xc0. Estado en D0 (1 = fin) y A5 por out. position_text    @ 0x10fc4 : salta (D4-4) entradas [u8 len] y fija cur_str_end. advance_text_entry @ 0x10f8e : avanza D4 entradas [word len] y fija cur_str_end. print_words_fit  @ 0x1090e : imprime desde A5 las palabras que caben en A_TEXT_BOX_W. measure_word     @ 0x10946 : mide una palabra del banco. measure_words    @ 0x10998 : cuenta palabras (D5) y la más ancha (D6). wrap_line        @ 0x109bc : word-wrap greedy al ancho de A_TEXT_BOX_W. print_line       @ 0x10b24 : mide/crece el ancho, imprime y rellena con espacios. print_spaces     @ 0x10b46 : imprime D6+1 espacios. text_index_arg   @ 0x10fb4 : position_text tomando el índice de A6[1] si D4 < 4. print_line_center@ 0x10aee : centra la línea de A5 y la imprime. overlay_a_draw   @ 0x109fe / overlay_b_draw @ 0x10a88 : overlays de texto A/B. overlay_a_frame  @ 0x10a54 / overlay_b_frame @ 0x10acc : marco de los overlays.  Todo el efecto es la imagen (y los registros devueltos por out-param). 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x10838` | `print_char` | dibuja el glifo de D0. | test_handlers, test_text |
| `0x108a2` | `print_word` | imprime la palabra de A5 hasta cur_str_end. | test_text, test_textbox |
| `0x10fc4` | `position_text` | position_text. | test_text, test_textmode |
| `0x10f8e` | `advance_text_entry` | avanza D4 entradas [word len] y fija cur_str_end. | test_text |
| `0x1090e` | `print_words_fit` | imprime las palabras que caben en A_TEXT_BOX_W. | test_text, test_textmeasure |
| `0x10b24` | `print_line` | mide la línea, ajusta el ancho y la imprime rellenando. | test_text, test_textmeasure |
| `0x10b46` | `print_spaces` | imprime D6+1 espacios. | test_text, test_textmode |
| `0x10946` | `measure_word` | mide una palabra. | test_text, test_textmeasure |
| `0x10998` | `measure_words` | cuenta palabras (D5) y la más ancha (D6). | test_text, test_textmeasure |
| `0x109bc` | `wrap_line` | word-wrap greedy al ancho A_TEXT_BOX_W. | test_text, test_textmeasure |
| `0x10fb4` | `text_index_arg` | position_text tomando el índice de A6[1] si D4 < 4. | test_text, test_textmode |
| `0x10aee` | `print_line_center` | centra la línea de A5 y cae en print_line. | test_text, test_textmode |
| `0x109fe` | `overlay_a_draw` | overlay de texto A (banco 0x41b76). | test_text, test_textmode |
| `0x10a88` | `overlay_b_draw` | overlay de texto B (banco 0x41586), posición fija. | test_text, test_textmode |
| `0x10a54` | `overlay_a_frame` | marco del overlay A. | test_text, test_textmode |
| `0x10acc` | `overlay_b_frame` | marco del overlay B, posición fija. | test_text, test_textmode |

## Estado (globales A_* usadas)

- `A_COLOR_TABLE`
- `A_CURSOR_TEXT_X`
- `A_CURSOR_TEXT_Y`
- `A_CUR_STR_END`
- `A_FONT_DATA`
- `A_MODE_FLAG`
- `A_OVERLAY_X`
- `A_OVERLAY_Y`
- `A_SCREEN_DRAW_BASE`
- `A_TEXT_BOX_W`
- `A_TEXT_COL`
- `A_TEXT_END_FLAG`
- `A_TEXT_INDEX`
- `A_TEXT_ROW`
- `A_TEXT_SEL_A`
- `A_TEXT_SEL_B`

