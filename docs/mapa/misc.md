# misc.c

misc.c — envoltorios y entradas alternativas pequenas (varias familias).  0x1104a setup del blit de texto; 0x12324 GEMDOS Cnecin; 0x12fea cae en 0x1222e; 0x127f0 save enmascarado + blit de texto; y entradas que solo avanzan A6 y caen en su cuerpo real (0x16512->0x16514, 0x16554->0x16556, 0x16576->0x16582, 0x18d26->0x18d28) o limpian globals (0x17472). 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x1104a` | `text_blit_fwd_a3` | FUN_0001104a @ 0x1104a: fija a3 = *(0x1a534) y cae en el blit de texto (0x11050). | test_misc |
| `0x127f0` | `masked_save_blit` | FUN_000127f0 @ 0x127f0: 0x127fa (save enmascarado) y 0x1104a (blit de texto); el 0x127fa preserva D2/D3/D7/A0/A1, asi que el segundo ve los mismos argumentos. | test_misc |
| `0x12324` | `gemdos_cnecin` | FUN_00012324 @ 0x12324: GEMDOS Cnecin (trap #1/$8); devuelve la tecla en D0. | test_misc |
| `0x12fea` | `hint_blit_entry_b` | FUN_00012fea @ 0x12fea: cae en 0x1222e (icono A6[1]). | test_misc |
| `0x16512` | `hint_icons_wrap` | +1 y cae en 0x16514 (iconos de pista). | test_misc |
| `0x16554` | `draw_hint_icons_wrap` | draw hint icons wrap | test_misc |
| `0x16576` | `update_icon_list_wrap` | update icon list wrap | test_misc |
| `0x18d26` | `draw_selection_frame_wrap` | draw selection frame wrap | test_misc |
| `0x17472` | `clear_globals` | FUN_00017472 @ 0x17472: limpia 0x1aa8b y 0x1acae. | test_misc |

## Estado (globales A_* usadas)

- `A_DAT_1A534`

