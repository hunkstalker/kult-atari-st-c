/* sprite.c — blitter de sprites y save/restore de rectángulos.
 *
 *   blit_sprite       @ 0x124f8 : dibuja un sprite formato Amiga (word-planar) en un
 *                                 descriptor A1: [u8 outer][u8 inner][long dest]
 *                                 [word flags][long src_stride][src...]. Expande los
 *                                 nibbles por el scratch 0x55b02 y mezcla con
 *                                 0xf0f0f0f0 (o 0x0f0f0f0f con el bit 2); el bit 3
 *                                 desplaza el destino 1 byte.
 *   save_rect_body    @ 0x12590 : empaqueta un rect de pantalla en un descriptor (A1);
 *                                 devuelve A4 (fin del buffer).
 *   save_rect         @ 0x1258a : save_rect_body con A1 = 0xd8f0.
 *   restore_rect      @ 0x124f2 : blit_sprite desde 0xd8f0 (inverso del save).
 *   blit_rect         @ 0x12510 : bucle del blit de sprite como entrada independiente.
 *   blit_rect_vbl     @ 0x124da : blit_rect con stride 0x1a534 + espera de VBL.
 *   copy_scratch_rows @ 0x123a0 : copia filas de 0xd8f0 a A0 (stride 0xa0).
 *
 * `movep.w`/`movep.l` transfieren bytes alternos: una escritura word deja el byte
 * alto en +0 y el bajo en +2; un long en +0/+2/+4/+6. Todo el efecto es la imagen.
 */
#include <stdint.h>

#include "machine.h"
#include "addrs.h"
#include "kult_internal.h"

void wait_vbl(uint8_t *img);

/* 0x124f8 — blitter de sprite por descriptor. */
void blit_sprite(uint8_t *img, uint32_t desc) {
    if (desc == 0)
        return;
    uint32_t outer = img[desc + 0];
    uint32_t inner = img[desc + 1];
    uint32_t dest = be32(img + desc + 2);
    uint32_t flags = be16(img + desc + 6);
    uint32_t stride = be32(img + desc + 8);
    uint32_t src = desc + 0xc;

    for (uint32_t col = 0; col < outer; col++) {
        uint32_t mask = 0xf0f0f0f0u;
        uint32_t shift = 0;
        uint32_t dst = dest;
        if (flags & 8)
            dst += 1;
        if (flags & 4) {
            mask = 0x0f0f0f0fu;
            shift = 4;
        }
        uint32_t inv = ~mask;
        uint32_t src_row = src;
        for (uint32_t row = 0; row < inner; row++) {
            uint16_t sw = be16(img + src_row);
            uint8_t oh = (sw >> 8) & 0xff, ol = sw & 0xff;
            uint16_t rot = (uint16_t)(((sw << 4) | (sw >> 12)) & 0xffff);
            uint8_t rh = (rot >> 8) & 0xff, rl = rot & 0xff;
            /* el original escribe estos cuatro bytes en 0x55b02 y los relee como long */
            img[A_BLIT_SCRATCH + 0] = oh;
            img[A_BLIT_SCRATCH + 1] = rh;
            img[A_BLIT_SCRATCH + 2] = ol;
            img[A_BLIT_SCRATCH + 3] = rl;
            uint32_t scratch = ((uint32_t)oh << 24) | ((uint32_t)rh << 16) |
                               ((uint32_t)ol << 8) | rl;
            uint32_t rr = shift ? ((scratch >> shift) | (scratch << (32 - shift))) : scratch;
            uint32_t out = (rr & mask) | (movep_read_l(img, dst) & inv);
            movep_write_l(img, dst, out);
            dst += 0xa0;
            src_row += stride;
        }
        src += 2;
        flags = (flags + 4) & 0xf;
        if (flags == 0)
            dest += 8;
    }
}

/* 0x12590 — empaqueta un rect en el descriptor A1; devuelve A4 (fin del buffer). */
uint32_t save_rect_body(uint8_t *img, uint32_t desc, uint32_t cols, uint32_t rows, uint32_t src,
                        uint32_t phase) {
    img[desc++] = cols & 0xff;
    img[desc++] = rows & 0xff;
    wr32(img + desc, src); desc += 4;
    wr16(img + desc, phase & 0xffff); desc += 2;
    uint32_t stride = ((cols & 0xffff) << 1);             /* shift = cols; lsl.l #1,shift; stride = shift */
    wr32(img + desc, stride); desc += 4;

    uint32_t col_count = (cols - 1) & 0xffff;
    uint32_t row_count = (rows - 1) & 0xffff;
    uint32_t end = desc;
    for (uint32_t col = 0; col <= col_count; col++) {
        uint32_t mask = 0xf0f0f0f0u;
        uint32_t src2 = src;
        uint32_t shift = 0;
        end = desc;
        if (phase & 8) src2 += 1;
        if (phase & 4) shift = 4;
        for (uint32_t row = 0; row <= row_count; row++) {
            uint32_t acc = rol32(movep_read_l(img, src2), shift) & mask;
            wr32(img + A_BLIT_SCRATCH, acc);                       /* move.l acc,(a5) */
            uint32_t t = ((uint32_t)img[A_BLIT_SCRATCH + 1] << 8) | img[A_BLIT_SCRATCH + 3];
            t = ror16(t, 4);                                      /* movep.w 1(a5),acc; ror.w #4 */
            uint32_t w = ((uint32_t)img[A_BLIT_SCRATCH + 0] << 8) | img[A_BLIT_SCRATCH + 2];
            wr16(img + end, (t | w) & 0xffff);                    /* move.w acc,(end) */
            src2 += 0xa0;
            end += stride;
        }
        desc += 2;
        phase = (phase + 4) & 0xf;
        if (phase == 0) src += 8;
    }
    end -= stride;
    end += 2;
    return end;
}

/* 0x1258a — save_rect_body con A1 = 0xd8f0. */
uint32_t save_rect(uint8_t *img, uint32_t cols, uint32_t rows, uint32_t src, uint32_t phase) {
    return save_rect_body(img, A_SAVE_DESC, cols, rows, src, phase);
}

/* 0x124f2 — restore_rect: blit_sprite desde 0xd8f0. */
void restore_rect(uint8_t *img) {
    blit_sprite(img, A_SAVE_DESC);
}

/* 0x12510 — bucle del blit de sprite (A0 destino, A1 origen, D2 cols, D3 filas,
 * D7 fase, A3 stride). */
void blit_rect(uint8_t *img, uint32_t dest, uint32_t src, uint32_t cols, uint32_t rows,
               uint32_t phase_in, uint32_t stride) {
    uint32_t scratch = A_BLIT_SCRATCH;
    uint32_t col = (cols - 1) & 0xffff;
    uint32_t row = (rows - 1) & 0xffff;
    uint32_t phase = phase_in & 0xffff;
    for (;;) {
        uint32_t mask = 0xf0f0f0f0u;
        uint32_t dst = dest, s = src, shift = 0;
        if (phase & 8) dst += 1;
        if (phase & 4) { mask = (mask >> 4) | (mask << 28); shift = 4; }
        uint32_t dmask = mask;
        mask = ~mask;
        uint32_t n = row;
        for (;;) {
            uint32_t word = be16(img + s);
            img[scratch] = (word >> 8) & 0xff; img[scratch + 2] = word & 0xff;
            word = ((word << 4) | (word >> 12)) & 0xffff;
            img[scratch + 1] = (word >> 8) & 0xff; img[scratch + 3] = word & 0xff;
            word = ((uint32_t)img[scratch] << 24) | ((uint32_t)img[scratch + 1] << 16) |
                 ((uint32_t)img[scratch + 2] << 8) | img[scratch + 3];
            word = rol32(word, (32 - shift) & 31);          /* ror.l shift (0 o 4) */
            word &= dmask;
            uint32_t out = movep_read_l(img, dst);
            out &= mask;
            out |= word;
            movep_write_l(img, dst, out);
            dst += 0xa0;
            s += stride;
            n = (n - 1) & 0xffff;
            if (n == 0xffff) break;
        }
        src += 2;
        phase = (phase + 4) & 0xf;
        if (phase == 0) dest += 8;
        col = (col - 1) & 0xffff;
        if (col == 0xffff) break;
    }
}

/* 0x124da — blit_rect con stride 0x1a534 + espera de VBL. */
void blit_rect_vbl(uint8_t *img, uint32_t dest, uint32_t src, uint32_t cols, uint32_t rows,
                   uint32_t phase) {
    uint32_t a3 = be32(img + 0x1a534);
    blit_rect(img, dest, src, cols, rows, phase, a3);
    wait_vbl(img);
}

/* 0x123a0 — copia D5+1 filas de (D4+1) longs desde 0xd8f0 a A0 (stride 0xa0). */
void copy_scratch_rows(uint8_t *img, uint32_t dest, uint32_t cols, uint32_t rows) {
    uint32_t src = 0xd8f0;
    for (uint32_t row = rows & 0xffff;; row = (row - 1) & 0xffff) {
        uint32_t dst = dest;
        for (uint32_t col = cols & 0xffff;; col = (col - 1) & 0xffff) {
            wr32(img + dst, be32(img + src));
            dst += 4;
            src += 4;
            if (col == 0) break;
        }
        dest += 0xa0;
        if (row == 0) break;
    }
}
