# textform.c

textform.c — formulario de texto con blits y sonido (0x141f4..0x143e2).  0x142cc prepara la lista de blits del formulario: elige el registro de la tabla 0x1a7b6 segun D0, avanza la tabla de entradas de texto (0x10f8e) y por cada una de las 3 lineas guarda [ancho][alto][fuente][pantalla][fase] en $d8f0, actualizando la caja 0x1a56c..0x1a56f y el stride 0x1a534. 0x143e2 recorre esa lista: blitea cada linea (0x11050/0x11102 segun el modo en $d8f0) con espera de VBL (0x13594) y cierra con el blit enmascarado de la caja (0x1215e). 0x141f4/0x141fa y 0x14246/0x1424c son las entradas del script: cargan el indice de A6[1], arrancan el sample (0x18f8e con D4=0x1d) y repiten el bloque desplazando el destino $d904. 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x142cc` | `textform_build` | FUN_000142cc @ 0x142cc: prepara la lista de blits del formulario. D0 = indice (1..16) del registro en la tabla 0x1a7b6; A6 = *(tabla + ((D0-1)&0xf)*4). | test_textform |
| `0x143e2` | `textform_draw` | FUN_000143e2 @ 0x143e2: dibuja la lista preparada por 0x142cc. Modo en $d8f0 (0 = blit directo, != 0 = invertido), cada linea con espera de VBL; la caja final va por el blit enmascarado. | test_textform |
| `0x141fa` | `textform_fwd` | FUN_000141fa @ 0x141fa: cuerpo del formulario hacia delante. A6 ya apunta al registro (0x141f4/0x141f6 cargan D0 de A6[1] y avanzan 2). | test_textform |
| `0x141f4` | `textform_fwd_entry` | FUN_000141f4 @ 0x141f4: entrada con D0 = A6[1], A6 += 2 y cae en 0x141fa. | test_textform |
| `0x1424c` | `textform_rev` | FUN_0001424c @ 0x1424c: cuerpo del formulario hacia atras. A6 ya apunta al registro (0x14246 carga D0 de A6[1] y avanza 2). | test_textform |
| `0x14246` | `textform_rev_entry` | FUN_00014246 @ 0x14246: entrada con D0 = A6[1], A6 += 2 y cae en 0x1424c. | test_textform |

## Estado (globales A_* usadas)

- `A_DAT_1A534`
- `A_TEXT_ENTRIES`
- `A_TEXT_FORM_0`
- `A_TEXT_FORM_1`
- `A_TEXT_FORM_2`
- `A_TEXT_FORM_BOX`
- `A_TEXT_FORM_LIST`
- `A_TEXT_FORM_MODE`
- `A_TEXT_FORM_TABLE`

