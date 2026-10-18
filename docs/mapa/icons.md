# icons.c

icons.c — dibujo/restauración de los iconos de pista (tabla 0x1a776).  draw_hint_icons @ 0x16556: blitea los 16 descriptores de la tabla con blit_sprite. FUN_00016514 @ 0x16514 + FUN_0001652e @ 0x1652e: restaura esas 16 zonas con 0x12190. FUN_000164d4 @ 0x164d4 y FUN_000164f6 @ 0x164f6: despachadores por el modo $1acc7. 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x164a4` | `icon_table_entry` | FUN_000164a4 @ 0x164a4: guarda el rect de pantalla del objeto A5 en el descriptor A1, enlaza A1 en la tabla de iconos (slot A2) y devuelve el nuevo A1 (fin del buffer). La fase D7 la calcula 0x10fec (fase de pixel); A1/A2/A5 los hila 0x16460. El original salva A2 en la pila alrededor de 0x12590 porque ese cuerpo machaca A2; en C A2 es local del llamante. | test_icons |
| `0x16460` | `icon_table` | FUN_00016460 @ 0x16460: reconstruye la tabla de iconos de pista (0x1a776, 16 entradas): la pone a 0 y, por cada objeto con bit6 en byte4, guarda su rect en buffers consecutivos desde 0x3a556 (A1) y enlaza el descriptor en el slot. | test_icons |
| `0x1652e` | `icon_apply` | icon apply | — |
| `0x16514` | `hint_icons` | hint icons | test_icons |
| `0x16556` | `draw_hint_icons` | ctx: itera 16 entradas (DAT_0001a776) llamando FUN_000124f8 | test_icons |
| `0x164d4` | `icon_state_a` | FUN_000164d4 @ 0x164d4: copia front->back si el modo $1acc7 es 0x29 o está en [0x16,0x19). | test_icons |
| `0x164f6` | `icon_state_b` | FUN_000164f6 @ 0x164f6: si el modo $1acc7 está en [0x16,0x19) recorre ARPLA (0x13f56). | test_icons |
| `0x18ce0` | `icon_anim_c` | FUN_00018ce0 @ 0x18ce0: recorre D0+1 entradas de 6 bytes en A0; si byte0 == 0x80 lo pasa a 0x40 y copia $1acd2 a byte1. | test_icons |
| `0x18c8e` | `icon_anim_a` | FUN_00018c8e @ 0x18c8e: recorre la tabla de 0x3f entradas de 6 bytes en 0x1a8f0 y limpia las que valen 0x80; a6++ y limpia $1acf7. | test_icons |
| `0x18cb2` | `icon_anim_b` | FUN_00018cb2 @ 0x18cb2: tres pasadas de 0x18ce0 sobre las tablas 0x1a95c/0x1a9d4/0x1aa3a. | test_icons |
| `0x18316` | `icon_list_build` | y 0x18342 (A5 = 0/0x8000): recorre 256 descriptores de 0xa bytes en 0xd8f0. Borra dos longs de pantalla con la mascara 0x7fff7fff rotada por la fase (word +2... | test_icons |
| `0x182f0` | `icon_list_reset` | FUN_000182f0 @ 0x182f0: 256 veces: limpia el long en 0xd8f0+k*0xa y llama 0x18316. | test_icons |
| `0x18d28` | `draw_selection_frame` | marco del objeto/hint seleccionado (0x1a81e) | test_icons |

## Estado (globales A_* usadas)

- `A_COMMAND_HINTS_LIST`
- `A_OBJECT_LIST_BEGIN`
- `A_OBJECT_LIST_END`
- `A_SCREEN_PTR_B`
- `A_SELECTED_ICON_PTR`
- `A_SELECTED_OBJECT`
- `A_SEL_1A7D2`
- `A_SEL_1AB4A`
- `A_SEL_1ACC9`

