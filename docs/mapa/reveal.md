# reveal.c

reveal.c — el efecto "reveal" pixelado.  FUN_00012ef6 @ 0x12ef6: mezcla 4 palabras leídas de A0-0x8000 en A0 bajo la máscara D4 rotada a derechas por el bit de fase D7 (D5 = ~máscara). Es el "dot" que el reveal dibuja. Entrada también desde 0x12edc con D4=0x8000.  FUN_00012e2e @ 0x12e2e: recorre los pixels de la región en pasos de D2 (byte en 0x1a577), calcula (x,y) del pixel con un `divu` por el ancho, dibuja el dot con FUN_00012ef6 y ejecuta un retardo (D3=0x1a56a) para que la revelación se vea paso a paso. D0/D2 se usan ya desplazados <<2 (0x10fee no re-escala). 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x12ef6` | `reveal_dot` | FUN_00012ef6 @ 0x12ef6 | test_reveal |
| `0x12e2e` | `reveal_region` | FUN_00012e2e @ 0x12e2e | test_reveal |
| `0x12fb6` | `masked_word4` | FUN_00012fb6 @ 0x12fb6: escribe 4 words en A0: para cada uno, AND con la máscara invertida (rotada de 0x8000 por D7) y OR con la parte correspondiente de D0. Puro bucle de píxeles. | test_reveal |
| `0x12f28` | `reveal_pixel` | reveal pixelado del elemento (dbf 0x64 / skip $1a570) | test_reveal |

## Estado (globales A_* usadas)

- `A_ANIM_1A56A`
- `A_ANIM_1A577`
- `A_DAT_1A49E`
- `A_ELEM_1A528`
- `A_ELEM_1A52C`
- `A_ELEM_1A52E`
- `A_ELEM_1A530`
- `A_ELEM_1A532`
- `A_REV_TOTAL`
- `A_REV_W`
- `A_REV_X0`
- `A_REV_Y0`
- `A_SKIP_TRANSITION`

