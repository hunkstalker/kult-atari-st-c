# mode.c

mode.c — modo de escena y lista de objetos (bloque 0x1442c..0x14588).  FUN_0001442c @ 0x1442c (cmd idx 74): guarda 0x1acc7, corre 0x1443e y lo restaura. FUN_0001443e @ 0x1443e: fija 0x1acc6 = A6[1] (el valor anterior va a 0x1accb), escribe el marcador en los 8 bytes de 0x1a695 (0xe5 si 0x1ad32 != 0, 0xdd si A6[1] == 0x81, 0xde si == 0x82; en cualquier otro caso no toca el buffer), A6 += 2, camina la tabla de modos (0x14496) y reinicia el estado de comandos (0x14588). Devuelve A6 y expone A4/A5 porque 0x1460a los recorre despues de la llamada (la lista de objetos [A5, A4)). FUN_00014496 @ 0x14496: camina la tabla 0x1ad38 por 0x1acc6 (1-based) hasta que 0xd8eb == 0x52 (el original reintenta; con otro valor gira indefinidamente), publica los 5 bytes de cabecera de la entrada en 0x1acd2/0x1acc7/0x1ab48+ 0x1ab45/0x1acf4/0x1ab4b y, si 0x1ab4b != 0, limpia 0x1ab62..0x1abf7 y empaqueta los objetos (flags word + hasta 5 words segun bits 4..0) a 5 words por objeto. Deja 0x1a7ca/0x1a762/0x1a7d2 = fin de la lista, 0x1a7ce = fin de la entrada (los 8-byte entries de [A5, A4) los consume 0x16582/0x166ec) y limpia 0x1ab4a/0x1acd3/0x1aceb/0x1abf8/0x1acae/0x1acaa/0x1ace7. FUN_00014588 @ 0x14588: reinicia los globals de comandos (0x1ab5x..0x1acf9). 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x14588` | `command_state_reset` | — | test_mode |
| `0x14496` | `scene_mode_load` | — | test_mode |
| `0x1443e` | `scene_mode_set` | — | test_mode |
| `0x1442c` | `cmd74_wrap` | — | test_mode |
| `0x1460a` | `scene_mode_apply` | — | test_mode |
| `0x146d0` | `scene_mode_draw` | — | test_mode |
| `0x14602` | `scene_mode_draw_wrap` | — | test_mode |

## Estado (globales A_* usadas)

- `A_1AB48`
- `A_DAT_1A572`
- `A_DAT_1A695`
- `A_DAT_1A762`
- `A_DAT_1AB4B`
- `A_DAT_1AB50`
- `A_DAT_1AB52`
- `A_DAT_1AB54`
- `A_DAT_1AB56`
- `A_DAT_1AB58`
- `A_DAT_1AB5A`
- `A_DAT_1AB5C`
- `A_DAT_1AB5E`
- `A_DAT_1AB62`
- `A_DAT_1ABF8`
- `A_DAT_1ABFA`
- `A_DAT_1ACAA`
- `A_DAT_1ACAE`
- `A_DAT_1ACC7`
- `A_DAT_1ACC8`
- `A_DAT_1ACCB`
- `A_DAT_1ACCE`
- `A_DAT_1ACD1`
- `A_DAT_1ACD2`
- `A_DAT_1ACD3`
- `A_DAT_1ACE7`
- `A_DAT_1ACEB`
- `A_DAT_1ACF9`
- `A_DAT_1ACFA`
- `A_DAT_1ACFE`
- `A_DAT_1ACFF`
- `A_DAT_1AD09`
- `A_DAT_1AD0B`
- `A_DAT_1AD0F`
- `A_DAT_1AD11`
- `A_DAT_1AD25`
- `A_DAT_1AD27`
- `A_DAT_1AD32`
- `A_DAT_D8EB`
- `A_MODE_FLAG`
- `A_MODE_TABLE`
- `A_OBJECT_LIST_BEGIN`
- `A_OBJECT_LIST_END`
- `A_PAL_INDEX_CUR`
- `A_SEL_1A7D2`
- `A_SEL_1AB4A`
- `A_SEL_1ACC9`
- `A_SKIP_TRANSITION`
- `A_TEXT_SEL_A`

