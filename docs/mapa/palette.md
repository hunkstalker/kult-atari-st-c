# palette.c

palette.c — paleta del shifter (familia 0x135da / 0x162ce..0x16300).  Estado (imagen plana + vistas documentadas; ver PLAN.md §4, opción a): A_PALETTE_PTR   (0x1a466, long) : puntero a la tabla de 16 words que ve el shifter; es el argumento de XBIOS Setpalette. A_PAL_TABLE     (0x1a10a)       : páginas de color, 5 words por entrada (A_PAL_TABLE + índice*0xa). A_PAL_INDEX_CUR (0x1acf4, byte) : índice de la paleta actual.  Las direcciones con nombre viven en include/addrs.h; el acceso a memoria es big-endian (be16/be32/wr16/wr32 de machine.h), como el 68000.  XBIOS Setpalette (0x16300) no escribe imagen: su efecto está en el shifter, así que la prueba diferencial lo comprueba por el ledger de eventos del kit (`os_setpalette` -> OS_EVENT_SETPALETTE), que es lo que emite el oráculo. 0x162e2 sí escribe (los 5 words) y además cae, sin `rts`, en 0x16300.  Cadena real de control (cada entrada cae en la siguiente): 0x162d8 -> 0x162da -> 0x162e2 -> 0x16300 0x162ce -> 0x162e2 -> 0x16300 0x135da -> 0x16300 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x16300` | `xbios_setpalette` | XBIOS Setpalette(*(A_PALETTE_PTR)): trap #14, función 6. Efecto de hardware (off-image); el candidato lo registra por os_setpalette(). | test_palette |
| `0x135da` | `set_palette` | `bsr.w 0x16300; rts`: alias público del Setpalette. | test_palette |
| `0x162e2` | `set_palette_entry` | fija una entrada de paleta: copia los 5 words de A_PAL_TABLE + d0*0xa a *(A_PALETTE_PTR)+2 y, al no tener `rts`, cae en 0x16300 (aplica la paleta). Es la entrada que usan 0x162da y 0x162ce. | test_palette |
| `0x162da` | `set_palette_current` | d0 = byte A_PAL_INDEX_CUR (0x1acf4) y cae en 0x162e2. | test_palette |
| `0x162ce` | `set_palette_from_entry` | d0 = A6[1] (índice tomado del guion), avanza A6 +2 y cae en 0x162e2. Devuelve el A6 resultante para poder comprobarlo como registro de salida. | test_palette |
| `0x162d8` | `set_palette_advance` | `addq.l #1,a6` y cae en 0x162da (paleta actual). Devuelve el A6 resultante (avanza el guion un byte). | test_palette |

## Estado (globales A_* usadas)

- `A_PALETTE_PTR`
- `A_PAL_INDEX_CUR`
- `A_PAL_TABLE`

