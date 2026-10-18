# line.c

line.c — trazo de línea Bresenham del revelado (0x13d0e y satélites).  reveal_line    @ 0x13d0e : copia, a lo largo de la recta (0x1a516,0x1a51a)-> (0x1a518,0x1a51c), los bytes de los carriles seleccionados por la máscara 0x80808080 rotada por la fase, del buffer trasero (a1 = a0-0x8000) al delantero. Los flags 0x55b02/0x55b03 registran el signo de dx/dy; los pasos laterales usan las direcciones de avance/retroceso según esos flags. reveal_lines_h @ 0x13e14 : dos tandas de trazos + espera de VBL, desplazando x0/x1 y luego y0/y1. reveal_lines_v @ 0x13eae : tres tandas (alto de caja, ancho, y 0x1a52c).  Todo el efecto es la imagen; la espera de VBL se libera con `schedule` (Fase 8). 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x13d0e` | `reveal_line` | trazo Bresenham. | test_line |
| `0x13e14` | `reveal_lines_h` | dos tandas de trazos (d2*4 pasos y *(0x1a52c)). | test_line |
| `0x13eae` | `reveal_lines_v` | tres tandas de trazos (alto, ancho<<2 y 0x1a52c). | test_line |

## Estado (globales A_* usadas)

- `A_BLIT_SCRATCH`
- `A_ELEM_1A52C`
- `A_ELEM_1A52E`

