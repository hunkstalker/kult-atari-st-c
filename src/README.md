# src/ — C de la reimplementación (empieza vacío)

Aquí va el C moderno y legible, un fichero por subsistema (`palette.c`,
`cursor.c`, `text.c`, …), escrito a partir del ASM de
`/app/kult-atari-st-decomp/matching/`.

Convenciones y primera tarea (piloto de paleta): ver `../docs/PLAN.md`.

Cada función: nombre **igual al de `work/names.txt`** + comentario con su
**dirección original**, p. ej.:

```c
/* 0x162e2 — fija una entrada de paleta (A_PAL_TABLE + idx*0xa -> A_PALETTE_PTR+2). */
void set_palette_entry(uint8_t *img, uint32_t index) { ... }
```

Convenciones confirmadas (estado plano, registros de salida, pruebas): ver
`../docs/PLAN.md` §4.1.
