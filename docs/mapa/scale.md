# scale.c

scale.c — escaladores de elemento (0x13a18/0x13bc2) y sus preparaciones.  FUN_00013a18 @ 0x13a18: construye el elemento escalado en el descriptor 0xd8f0 (filas empaquetadas) y lo blitea a A2 (pantalla) con 0x123a0. La posicion de origen la lleva un acumulador 16.16 (0x1a48c) alimentado por 0x1a490/0x1a492; 0x1a494/0x1a498/0x1a49a hacen lo propio con la fila. Cada columna expande un bit del bitmap (A0) en 4 planos del descriptor con `roxl`.  FUN_00013bc2 @ 0x13bc2: el mismo bucle pero escribiendo en A1 (buffer del llamante, filas a 0xa0) y sin el blit final. 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x13a18` | `scale_16_16` | scale 16 16 | test_scale |
| `0x13bc2` | `scale_blit` | scale blit | test_scale |
| `0x13702` | `scale_prep` | scale prep | test_scale |
| `0x1398e` | `scale_list_build` | FUN_0001398e @ 0x1398e: construye en 0xd8f0 una lista de entradas [word offset][word max(1,*0x1a1f2)][long movep(fila)] de 8 filas (stride 0xa0) por grupo, avanzando A2 +1/+7 y A0 +0x500 por banda; cierra con un long 0. | test_scale |
| `0x138a6` | `scale_anim` | scale anim | test_scale |
| `0x13740` | `scale_loop` | scale loop | test_scale |

## Estado (globales A_* usadas)

- `A_ELEM_1A528`
- `A_ELEM_1A52C`
- `A_ELEM_1A52E`
- `A_ELEM_1A530`
- `A_ELEM_1A532`
- `A_SCREEN_PTR_A`
- `A_SCREEN_PTR_B`
- `A_SKIP_TRANSITION`

