/* screens.c — buffers de pantalla y direccionamiento planar.
 *
 *   copy_screen       @ 0x121c6 : copia screen_ptr_b -> screen_ptr_a (8000 longs).
 *   copy_screen_a6    @ 0x121c4 : `addq.l #1,a6` y cae en copy_screen.
 *   swap_screens      @ 0x121de : intercambia los dos buffers long a long.
 *   screen_address    @ 0x10fec : dirección planar de (x,y) sobre screen_ptr_a (d0<<2).
 *   screen_address_raw@ 0x10fee : idem sin el `lsl.w #2` (d0 ya escalado).
 *   screen_offset     @ 0x10ff4 : cuerpo común; suma el offset a A0 (dado por el llamante).
 *
 * Todo el efecto es la imagen (o el valor devuelto): sin hardware, verificable
 * por el diff de bytes. Las `screen_address*` devuelven A0 (dirección) y entregan
 * D7 (fase de píxel dentro de la palabra) por out-param, según PLAN §4.1
 * (primario = retorno, resto = out-param).
 */
#include <stdint.h>

#include "machine.h"
#include "addrs.h"

#define SCREEN_LONGS 8000   /* 0x1f40 longs = 32000 B (framebuffer baja resolución) */
#define SCREEN_ROW   0xa0   /* bytes por fila (160 B) */

/* 0x121c6 — copia 0x1f40 longs de screen_ptr_b a screen_ptr_a. */
void copy_screen(uint8_t *img) {
    uint32_t src = be32(img + A_SCREEN_PTR_B);
    uint32_t dst = be32(img + A_SCREEN_PTR_A);
    for (uint32_t i = 0; i < SCREEN_LONGS; i++)
        wr32(img + dst + i * 4, be32(img + src + i * 4));
}

/* 0x121c4 — `addq.l #1,a6` y cae en copy_screen. Devuelve A6 avanzado. */
uint32_t copy_screen_a6(uint8_t *img, uint32_t a6) {
    a6 += 1;
    copy_screen(img);
    return a6;
}

/* 0x121de — intercambia screen_ptr_a y screen_ptr_b long a long. */
void swap_screens(uint8_t *img) {
    uint32_t a0 = be32(img + A_SCREEN_PTR_A);
    uint32_t a1 = be32(img + A_SCREEN_PTR_B);
    for (uint32_t i = 0; i < SCREEN_LONGS; i++) {
        uint32_t d1 = be32(img + a0);
        wr32(img + a0, be32(img + a1));
        wr32(img + a1, d1);
        a0 += 4;
        a1 += 4;
    }
}

/* 0x10ff4 — cuerpo común: A0 += ((d0 & 0xfff0) >> 1) + (d1 & 0xffff)*0xa0;
 * D7 = d0 & 0xf. Devuelve A0 y entrega D7. */
uint32_t screen_offset(uint8_t *img, uint32_t a0, uint32_t d0, uint32_t d1, uint32_t *d7) {
    (void)img;
    d0 &= 0xffff;
    d1 &= 0xffff;
    *d7 = d0 & 0xf;
    d0 &= 0xfff0;
    d0 >>= 1;
    return a0 + d0 + d1 * SCREEN_ROW;
}

/* 0x10fee — screen_address_raw: A0 = screen_ptr_a y cae en screen_offset. */
uint32_t screen_address_raw(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t *d7) {
    return screen_offset(img, be32(img + A_SCREEN_PTR_A), d0, d1, d7);
}

/* 0x10fec — screen_address: `lsl.w #2,d0` y cae en screen_address_raw. */
uint32_t screen_address(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t *d7) {
    return screen_address_raw(img, (d0 << 2) & 0xffff, d1, d7);
}
