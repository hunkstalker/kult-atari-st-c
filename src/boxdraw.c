/* boxdraw.c — primitivas de dibujo de la caja de texto (runs de píxeles).
 *
 *   box_hline      @ 0x12ffa : corre horizontal de D3+1 píxeles en (D0,D1) con
 *                              color D2 (un bit por plano) sobre screen_ptr_a.
 *   box_vline      @ 0x13216 : la misma idea pero vertical (a0 += 0xa0 por píxel).
 *   box_hline_copy @ 0x130a4 : copia un run horizontal de screen_ptr_b a screen_ptr_a.
 *   box_vline_copy @ 0x13162 : idem vertical.
 *   box_hline_back @ 0x12fee : box_hline con draw base fijo 0x8000.
 *   box_vline_back @ 0x1320a : box_vline con draw base fijo 0x8000.
 *
 * Layout de 4 planos en grupos de 8 bytes por 16 px ([p0][p1][p2][p3]); el bit
 * dentro del grupo baja 15..0 y el grupo avanza 8 bytes al desbordar. La
 * dirección base la da screen_address_raw (0x10fee). Todo el efecto es la imagen.
 */
#include <stdint.h>

#include "machine.h"
#include "addrs.h"

uint32_t screen_address_raw(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t *d7);

/* Pinta (d2 & (1<<plano)) en el bit `bit` de los 4 planos en a0. */
static void set_planes(uint8_t *img, uint32_t dest, uint32_t bit, uint32_t color) {
    for (uint32_t p = 0; p < 4; p++) {
        uint32_t v = be16(img + dest + p * 2);
        if (color & (1u << p))
            v |= (1u << bit);
        else
            v &= ~(1u << bit);
        wr16(img + dest + p * 2, v & 0xffff);
    }
}

/* 0x12ffa — run horizontal con draw base 0. Preserva D0/D1/D6/D7 (no observado). */
void box_hline(uint8_t *img, uint32_t x, uint32_t y, uint32_t color, uint32_t len) {
    wr32(img + A_SCREEN_DRAW_BASE, 0);
    uint32_t phase;
    uint32_t dest = screen_address_raw(img, x, y, &phase) - be32(img + A_SCREEN_DRAW_BASE);
    uint32_t bit = (15 - (phase & 0xf)) & 0xffff;
    len = (len - 1) & 0xffff;
    for (;;) {
        set_planes(img, dest, bit, color);
        if (bit != 0)
            bit = (bit - 1) & 0xffff;
        else {
            dest += 8;
            bit = 0xf;
        }
        len = (len - 1) & 0xffff;
        if (len == 0xffff)
            break;
    }
}

/* 0x13216 — run vertical con draw base 0 (a0 += 0xa0 por píxel). */
void box_vline(uint8_t *img, uint32_t x, uint32_t y, uint32_t color, uint32_t len) {
    wr32(img + A_SCREEN_DRAW_BASE, 0);
    uint32_t phase;
    uint32_t dest = screen_address_raw(img, x, y, &phase) - be32(img + A_SCREEN_DRAW_BASE);
    uint32_t bit = (15 - (phase & 0xf)) & 0xffff;
    len = (len - 1) & 0xffff;
    for (;;) {
        set_planes(img, dest, bit, color);
        dest += 0xa0;
        len = (len - 1) & 0xffff;
        if (len == 0xffff)
            break;
    }
}

/* Copia el bit de color de cada plano desde screen_ptr_b a screen_ptr_a. `step` es
 * el avance (8 = grupo de 16 px, 0xa0 = una fila); `wrap_bit` recorre los 16 bits. */
static void copy_run(uint8_t *img, uint32_t x, uint32_t y, uint32_t len,
                     uint32_t step, int wrap_bit) {
    uint32_t off = ((x & 0xfff0) >> 1) + (y & 0xffff) * 0xa0u;
    uint32_t front = be32(img + A_SCREEN_PTR_A) + off;
    uint32_t back = be32(img + A_SCREEN_PTR_B) + off;
    uint32_t bit = (15 - (x & 0xf)) & 0xffff;
    len = (len - 1) & 0xffff;
    for (;;) {
        for (uint32_t p = 0; p < 4; p++) {
            uint32_t v = be16(img + front + p * 2);
            uint32_t b = be16(img + back + p * 2);
            if (b & (1u << bit))
                v |= (1u << bit);
            else
                v &= ~(1u << bit);
            wr16(img + front + p * 2, v & 0xffff);
        }
        if (wrap_bit && bit != 0)
            bit = (bit - 1) & 0xffff;
        else {
            front += step;
            back += step;
            if (wrap_bit)
                bit = 0xf;
        }
        len = (len - 1) & 0xffff;
        if (len == 0xffff)
            break;
    }
}

/* 0x130a4 — copia un run horizontal. */
void box_hline_copy(uint8_t *img, uint32_t x, uint32_t y, uint32_t len) {
    copy_run(img, x, y, len, 8, 1);
}

/* 0x13162 — copia un run vertical. */
void box_vline_copy(uint8_t *img, uint32_t x, uint32_t y, uint32_t len) {
    copy_run(img, x, y, len, 0xa0, 0);
}

/* 0x12fee — box_hline con draw base fijo 0x8000. */
void box_hline_back(uint8_t *img, uint32_t x, uint32_t y, uint32_t color, uint32_t len) {
    wr32(img + A_SCREEN_DRAW_BASE, 0x8000);
    uint32_t phase;
    uint32_t dest = screen_address_raw(img, x, y, &phase) - be32(img + A_SCREEN_DRAW_BASE);
    uint32_t bit = (15 - (phase & 0xf)) & 0xffff;
    len = (len - 1) & 0xffff;
    for (;;) {
        set_planes(img, dest, bit, color);
        if (bit != 0)
            bit = (bit - 1) & 0xffff;
        else {
            dest += 8;
            bit = 0xf;
        }
        len = (len - 1) & 0xffff;
        if (len == 0xffff)
            break;
    }
}

/* 0x1320a — box_vline con draw base fijo 0x8000. */
void box_vline_back(uint8_t *img, uint32_t x, uint32_t y, uint32_t color, uint32_t len) {
    wr32(img + A_SCREEN_DRAW_BASE, 0x8000);
    uint32_t phase;
    uint32_t dest = screen_address_raw(img, x, y, &phase) - be32(img + A_SCREEN_DRAW_BASE);
    uint32_t bit = (15 - (phase & 0xf)) & 0xffff;
    len = (len - 1) & 0xffff;
    for (;;) {
        set_planes(img, dest, bit, color);
        dest += 0xa0;
        len = (len - 1) & 0xffff;
        if (len == 0xffff)
            break;
    }
}
