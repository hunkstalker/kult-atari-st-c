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
#include "kult_internal.h"

uint32_t screen_address_raw(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t *d7);
void wait_vbl(uint8_t *img);

/* 0x13d0e — trazo Bresenham. */
void reveal_line(uint8_t *img) {
    uint32_t flags = A_BLIT_SCRATCH;                 /* 0x55b02 */
    wr16(img + flags, 0);

    uint32_t x0 = be16(img + 0x1a516);
    uint32_t x1 = be16(img + 0x1a518);
    uint32_t y0 = be16(img + 0x1a51a);
    uint32_t y1 = be16(img + 0x1a51c);

    uint32_t dx = (x1 - x0) & 0xffff;
    if (word_sub_extend((uint16_t)x1, (uint16_t)x0)) {
        dx = (~dx) & 0xffff;
        img[flags] = 1;
    }
    uint32_t steps = dx;
    uint32_t err_inc = (~(dx << 1)) & 0xffff;

    uint32_t dy = (y1 - y0) & 0xffff;
    if (word_sub_extend((uint16_t)y1, (uint16_t)y0)) {
        dy = (~dy) & 0xffff;
        img[flags + 1] = 1;
    }
    steps = (steps + dy + 1) & 0xffff;
    uint32_t dy2 = (dy << 1) & 0xffff;

    uint32_t err;
    uint32_t front = screen_address_raw(img, x0, y0, &err);
    if (err & 8)
        front += 1;
    uint32_t back = front - 0x8000;
    err &= 7;
    uint32_t mask = rotate_right32(0x80808080u, err);
    err = (dy2 + err_inc) & 0xffff;

    for (;;) {
        uint32_t nmask = ~mask;
        movep_write_l(img, front, (movep_read_l(img, back) & mask) |
                                  (movep_read_l(img, front) & nmask));

        steps = (steps - 1) & 0xffff;
        if (steps == 0)
            return;

        if ((int16_t)err <= 0) {
            if (img[flags] == 1) {
                uint32_t carry = (mask >> 31) & 1;
                mask = ((mask << 1) | carry) & 0xffffffffu;
                if (carry) {
                    front -= 1; back -= 1;
                    if (front & 1) { front -= 6; back -= 6; }     /* `beq` salta si par */
                }
            } else {
                uint32_t carry = mask & 1;
                mask = ((mask >> 1) | (carry << 31)) & 0xffffffffu;
                if (carry) {
                    front += 1; back += 1;
                    if (!(front & 1)) { front += 6; back += 6; }  /* `bne` salta si impar */
                }
            }
            err = (err + dy2) & 0xffff;
            if ((int16_t)err > 0) {
                steps = (steps - 1) & 0xffff;
                if (steps == 0)
                    return;
            } else {
                continue;
            }
        }
        if (img[flags + 1] == 1) {
            front -= 0xa0; back -= 0xa0;
        } else {
            front += 0xa0; back += 0xa0;
        }
        err = (err + err_inc) & 0xffff;
    }
}

/* 0x13e14 — dos tandas de trazos (d2*4 pasos y *(0x1a52c)). */
void reveal_lines_h(uint8_t *img, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    x = (x << 2) & 0xffff;
    wr16(img + 0x1a516, x);
    w = (w << 2) & 0xffff;
    x = (x + w - 1) & 0xffff;
    wr16(img + 0x1a518, x);
    wr16(img + 0x1a51a, y & 0xffff);
    h = (h + y - 1) & 0xffff;
    wr16(img + 0x1a51c, h);

    uint32_t n1 = (w - 1) & 0xffff;
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
void reveal_lines_v(uint8_t *img, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    x = (x << 2) & 0xffff;
    wr16(img + 0x1a516, x);
    w = (w << 1) & 0xffff;
    x = (x + w - 1) & 0xffff;
    wr16(img + 0x1a518, x);
    w = h & 0xffff;
    h = (h + y - 1) & 0xffff;
    wr16(img + 0x1a51c, h);
    wr16(img + 0x1a51a, h);

    uint32_t n1 = (w - 1) & 0xffff;
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
