# src/ — C de la reimplementación

Un fichero por subsistema (`palette.c`, `cursor.c`, `text.c`, …), escrito a
partir del ASM de `../kult-atari-st-decomp/matching/`.

Convenciones: ver `../docs/PLAN.md`, especialmente §4.1.

Cada función lleva un nombre **igual al de `symbols/names.txt`** y un comentario
con su **dirección original**, por ejemplo:

```c
/* 0x162e2 — fija una entrada de paleta (A_PAL_TABLE + idx*0xa -> A_PALETTE_PTR+2). */
void set_palette_entry(uint8_t *img, uint32_t index) { ... }
```
