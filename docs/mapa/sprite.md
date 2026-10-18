# sprite.c

sprite.c — blitter de sprites y save/restore de rectángulos.  blit_sprite       @ 0x124f8 : dibuja un sprite formato Amiga (word-planar) en un descriptor A1: [u8 outer][u8 inner][long dest] [word flags][long src_stride][src...]. Expande los nibbles por el scratch 0x55b02 y mezcla con 0xf0f0f0f0 (o 0x0f0f0f0f con el bit 2); el bit 3 desplaza el destino 1 byte. save_rect_body    @ 0x12590 : empaqueta un rect de pantalla en un descriptor (A1); devuelve A4 (fin del buffer). save_rect         @ 0x1258a : save_rect_body con A1 = 0xd8f0. restore_rect      @ 0x124f2 : blit_sprite desde 0xd8f0 (inverso del save). blit_rect         @ 0x12510 : bucle del blit de sprite como entrada independiente. blit_rect_vbl     @ 0x124da : blit_rect con stride 0x1a534 + espera de VBL. copy_scratch_rows @ 0x123a0 : copia filas de 0xd8f0 a A0 (stride 0xa0).  `movep.w`/`movep.l` transfieren bytes alternos: una escritura word deja el byte alto en +0 y el bajo en +2; un long en +0/+2/+4/+6. Todo el efecto es la imagen. 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x124f8` | `blit_sprite` | blitter de sprite por descriptor. | test_sprite |
| `0x12590` | `save_rect_body` | empaqueta un rect en el descriptor A1; devuelve A4 (fin del buffer). | — |
| `0x1258a` | `save_rect` | save_rect_body con A1 = 0xd8f0. | test_sprite |
| `0x124f2` | `restore_rect` | restore_rect: blit_sprite desde 0xd8f0. | test_sprite |
| `0x12510` | `blit_rect` | bucle del blit de sprite (A0 destino, A1 origen, D2 cols, D3 filas, D7 fase, A3 stride). | test_sprite |
| `0x124da` | `blit_rect_vbl` | blit_rect con stride 0x1a534 + espera de VBL. | test_sprite |
| `0x123a0` | `copy_scratch_rows` | copia D5+1 filas de (D4+1) longs desde 0xd8f0 a A0 (stride 0xa0). | test_sprite |

## Estado (globales A_* usadas)

- `A_BLIT_SCRATCH`
- `A_SAVE_DESC`

