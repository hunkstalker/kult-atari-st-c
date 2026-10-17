/* line.c — trazo de línea Bresenham del revelado (0x13d0e y satélites).
 *
 *   reveal_line    @ 0x13d0e : copia, a lo largo de la recta (0x1a516,0x1a51a)->
 *                              (0x1a518,0x1a51c), los bytes de los carriles
 *                              seleccionados por la máscara 0x80808080 rotada por
 *                              la fase, del buffer trasero (a1 = a0-0x8000) al
 *                              delantero. Los flags 0x55b02/0x55b03 registran el
 *                              signo de dx/dy; los pasos laterales usan las
 *                              direcciones de avance/retroceso según esos flags.
 *   reveal_lines_h @ 0x13e14 : dos tandas de trazos + espera de VBL, desplazando
 *                              x0/x1 y luego y0/y1.
 *   reveal_lines_v @ 0x13eae : tres tandas (alto de caja, ancho, y 0x1a52c).
 *
 * Todo el efecto es la imagen; la espera de VBL se libera con `schedule` (Fase 8).
 */
#include <stdint.h>

#include "machine.h"
#include "addrs.h"

uint32_t screen_address_raw(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t *d7);
void wait_vbl(uint8_t *img);

static uint32_t movep_read_l(uint8_t *img, uint32_t a) {
    return ((uint32_t)img[a] << 24) | ((uint32_t)img[a + 2] << 16) |
           ((uint32_t)img[a + 4] << 8) | img[a + 6];
}
static void movep_write_l(uint8_t *img, uint32_t a, uint32_t v) {
    img[a] = (v >> 24) & 0xff;
    img[a + 2] = (v >> 16) & 0xff;
    img[a + 4] = (v >> 8) & 0xff;
    img[a + 6] = v & 0xff;
}

/* 0x13d0e — trazo Bresenham. */
void reveal_line(uint8_t *img) {
    uint32_t a5 = A_BLIT_SCRATCH;                 /* 0x55b02 */
    wr16(img + a5, 0);

    uint32_t x0 = be16(img + 0x1a516);
    uint32_t x1 = be16(img + 0x1a518);
    uint32_t y0 = be16(img + 0x1a51a);
    uint32_t y1 = be16(img + 0x1a51c);

    uint32_t dx = (x1 - x0) & 0xffff;
    if (word_sub_extend((uint16_t)x1, (uint16_t)x0)) {
        dx = (~dx) & 0xffff;
        img[a5] = 1;
    }
    uint32_t d6 = dx;
    uint32_t d4 = (~(dx << 1)) & 0xffff;

    uint32_t dy = (y1 - y0) & 0xffff;
    if (word_sub_extend((uint16_t)y1, (uint16_t)y0)) {
        dy = (~dy) & 0xffff;
        img[a5 + 1] = 1;
    }
    d6 = (d6 + dy + 1) & 0xffff;
    uint32_t d5 = (dy << 1) & 0xffff;

    uint32_t d7;
    uint32_t a0 = screen_address_raw(img, x0, y0, &d7);
    if (d7 & 8)
        a0 += 1;
    uint32_t a1 = a0 - 0x8000;
    d7 &= 7;
    uint32_t d3m = rotate_right32(0x80808080u, d7);
    d7 = (d5 + d4) & 0xffff;

    for (;;) {
        uint32_t nd3 = ~d3m;
        movep_write_l(img, a0, (movep_read_l(img, a1) & d3m) |
                                  (movep_read_l(img, a0) & nd3));

        d6 = (d6 - 1) & 0xffff;
        if (d6 == 0)
            return;

        if ((int16_t)d7 <= 0) {
            if (img[a5] == 1) {
                uint32_t c = (d3m >> 31) & 1;
                d3m = ((d3m << 1) | c) & 0xffffffffu;
                if (c) {
                    a0 -= 1; a1 -= 1;
                    if (a0 & 1) { a0 -= 6; a1 -= 6; }     /* `beq` salta si par */
                }
            } else {
                uint32_t c = d3m & 1;
                d3m = ((d3m >> 1) | (c << 31)) & 0xffffffffu;
                if (c) {
                    a0 += 1; a1 += 1;
                    if (!(a0 & 1)) { a0 += 6; a1 += 6; }  /* `bne` salta si impar */
                }
            }
            d7 = (d7 + d5) & 0xffff;
            if ((int16_t)d7 > 0) {
                d6 = (d6 - 1) & 0xffff;
                if (d6 == 0)
                    return;
            } else {
                continue;
            }
        }
        if (img[a5 + 1] == 1) {
            a0 -= 0xa0; a1 -= 0xa0;
        } else {
            a0 += 0xa0; a1 += 0xa0;
        }
        d7 = (d7 + d4) & 0xffff;
    }
}

/* 0x13e14 — dos tandas de trazos (d2*4 pasos y *(0x1a52c)). */
void reveal_lines_h(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3) {
    d0 = (d0 << 2) & 0xffff;
    wr16(img + 0x1a516, d0);
    d2 = (d2 << 2) & 0xffff;
    d0 = (d0 + d2 - 1) & 0xffff;
    wr16(img + 0x1a518, d0);
    wr16(img + 0x1a51a, d1 & 0xffff);
    d3 = (d3 + d1 - 1) & 0xffff;
    wr16(img + 0x1a51c, d3);

    uint32_t n1 = (d2 - 1) & 0xffff;
    for (;;) {
        reveal_line(img);
        wait_vbl(img);
        wr16(img + 0x1a516, (be16(img + 0x1a516) + 1) & 0xffff);
        wr16(img + 0x1a518, (be16(img + 0x1a518) - 1) & 0xffff);
        if (n1 == 0)
            break;
        n1 = (n1 - 1) & 0xffff;
    }

    uint32_t n2 = (be16(img + A_ELEM_1A52C) - 1) & 0xffff;
    uint32_t e0 = (be16(img + 0x1a516) - 1) & 0xffff;
    uint32_t e1 = (be16(img + 0x1a518) + 1) & 0xffff;
    wr16(img + 0x1a518, e0);
    wr16(img + 0x1a516, e1);
    uint32_t f0 = be16(img + 0x1a51a);
    wr16(img + 0x1a51a, be16(img + 0x1a51c));
    wr16(img + 0x1a51c, f0);
    for (;;) {
        reveal_line(img);
        wait_vbl(img);
        wr16(img + 0x1a51a, (be16(img + 0x1a51a) - 1) & 0xffff);
        wr16(img + 0x1a51c, (be16(img + 0x1a51c) + 1) & 0xffff);
        if (n2 == 0)
            break;
        n2 = (n2 - 1) & 0xffff;
    }
}

/* 0x13eae — tres tandas de trazos (alto, ancho<<2 y 0x1a52c). */
void reveal_lines_v(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3) {
    d0 = (d0 << 2) & 0xffff;
    wr16(img + 0x1a516, d0);
    d2 = (d2 << 1) & 0xffff;
    d0 = (d0 + d2 - 1) & 0xffff;
    wr16(img + 0x1a518, d0);
    d2 = d3 & 0xffff;
    d3 = (d3 + d1 - 1) & 0xffff;
    wr16(img + 0x1a51c, d3);
    wr16(img + 0x1a51a, d3);

    uint32_t n1 = (d2 - 1) & 0xffff;
    for (;;) {
        reveal_line(img);
        wait_vbl(img);
        wr16(img + 0x1a51a, (be16(img + 0x1a51a) - 1) & 0xffff);
        if (n1 == 0)
            break;
        n1 = (n1 - 1) & 0xffff;
    }

    uint32_t n2 = ((be16(img + A_ELEM_1A52E) << 2) - 1) & 0xffff;
    for (;;) {
        reveal_line(img);
        wait_vbl(img);
        wr16(img + 0x1a516, (be16(img + 0x1a516) + 1) & 0xffff);
        if (n2 == 0)
            break;
        n2 = (n2 - 1) & 0xffff;
    }

    uint32_t n3 = be16(img + A_ELEM_1A52C);
    for (;;) {
        reveal_line(img);
        wait_vbl(img);
        wr16(img + 0x1a51a, (be16(img + 0x1a51a) + 1) & 0xffff);
        if (n3 == 0)
            break;
        n3 = (n3 - 1) & 0xffff;
    }
}
