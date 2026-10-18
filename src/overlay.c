/* overlay.c — blitters de planos de los overlays de texto (0x111b6 y satelites).
 *
 *   overlay_compose_planes      @ 0x111b6 : compone D3 filas de un bitmap de A1
 *        (stride 0x1a534) en el descriptor 0xd8f0, con 1..4 planos segun D2 y
 *        desplazamiento de pixel D0 y de sub-word D1&3; espera VBL por pasada y
 *        aplica el descriptor a A0 (copy_scratch_rows). Repite D1+1 veces.
 *   overlay_compose_planes_wrap @ 0x111ac : D3-=1, D2=5, D1=(D1<<2)-1.
 *   overlay_compose_planes32    @ 0x114ba : variante de 32 bits; el acarreo vive en D6
 *        y el sub-word usa (3 - (D1&3)).
 *   overlay_compose_planes32_wrap @ 0x114b0: wrapper de 0x114ba.
 *   overlay_compose_masked      @ 0x117fe : compone dos fuentes (A0 mascara, A1 dato)
 *        con ror32(0x80000000,D0); el flag 0x1a53e se resta a D2 por pasada.
 *   overlay_compose_masked_wrap @ 0x117de : fija 0x1a53e=1.
 *   overlay_compose_shift       @ 0x11afa : variante roxl (flag 0x1a53e).
 *   overlay_compose_shift_wrap  @ 0x11ada : fija 0x1a53e=1.
 *
 * Los registros finales (D0-D3/A0/A1) se dejan en `overlay_blit_regs` (los usan
 * 0x12ce4/0x12d16). Todo el efecto de imagen es el descriptor + el screen.
 */
#include <stdint.h>

#include "machine.h"
#include "addrs.h"

void wait_vbl(uint8_t *);
void copy_scratch_rows(uint8_t *, uint32_t, uint32_t, uint32_t);

static uint16_t r16l(uint16_t v, uint32_t n) { return rotate_left16(v, n); }
static uint16_t r16r(uint16_t v, uint32_t n) { return rotate_right16(v, n); }

/* Registro final del ultimo blitter de 0x11xxx (D0,D1,D2,D3,A0,A1). Lo usan
 * 0x12ce4/0x12d16 para guardar los bloques 0x1a82e/0x1a83e (movem d0/d2/a0-a1). */
uint32_t overlay_blit_regs[6];

/* 0x111b6 — compone 1..4 planos de A1 en 0xd8f0 (shift D0/D1) + VBL */
void overlay_compose_planes(uint8_t *img, uint32_t d0in, uint32_t d1in, uint32_t d2in,
                 uint32_t d3in, uint32_t a0in, uint32_t a1in) {
    uint32_t pix = d0in & 0xffff, sub = d1in & 0xffff, width = d2in & 0xffff;
    uint32_t height = d3in & 0xffff, dest = a0in, src = a1in;

    for (;;) {
        uint32_t s_pix = pix, s_sub = sub, s_width = width, s_height = height, s_dest = dest, s_src = src;

        uint32_t hi_mask = 0xffffffffu >> (pix & 0x3f);
        uint32_t lo_mask = ~hi_mask;
        uint32_t first_bits = (0x10 - pix) & 0xffff;
        uint32_t planes = 1;
        int carry = 1;
        uint32_t data = (width - first_bits) & 0xffff;
        if (first_bits <= width && data != 0) {
            for (;;) {
                planes = (planes + 1) & 0xffff;
                carry ^= 1;
                uint32_t bit = (0xf < data);
                data = (data - 0x10) & 0xffff;
                if (!(bit && data != 0))
                    break;
            }
        }
        uint32_t neg = (uint32_t)(-(int32_t)data);
        uint32_t end_mask = (~(0xffffffffu << (neg & 0x3f))) & 0xffffffffu;
        planes = (planes >> 1) & 0xffff;
        if (carry) {
            planes = (planes + 1) & 0xffff;
            end_mask = (end_mask << 16) | (end_mask >> 16) | 0xffff;
        }

        uint32_t rows = height;
        uint32_t subword = sub & 3;
        uint32_t desc = A_SAVE_DESC;
        uint32_t case_d4;
        uint32_t w_dest = dest, w_src = src, w_desc = desc;

        if (planes == 4) {
            case_d4 = 0xf;
            carry = 0;
            for (uint32_t row = rows;; row = (row - 1) & 0xffff) {
                uint32_t row_dest = w_dest, row_desc = w_desc;
                uint32_t word = r16l(be16(img + w_src), subword);
                word = r16r((uint16_t)word, pix);
                data = word;
                uint16_t dmask = r16r(0x8000, pix);
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t w08 = be16(img + row_dest + 8);
                    uint32_t long00 = ((uint32_t)be16(img + row_dest) << 16) | w08;
                    uint32_t w18 = be16(img + row_dest + 0x18);
                    uint32_t w28 = be16(img + row_dest + 0x28);
                    uint32_t long30 = ((uint32_t)be16(img + row_dest + 0x30) << 16) |
                                   be16(img + row_dest + 0x38);
                    uint32_t acc = (uint32_t)carry;
                    uint32_t pair10 = (((uint32_t)be16(img + row_dest + 0x10) << 16) | w18) >> 1;
                    uint32_t pair20 = (((uint32_t)be16(img + row_dest + 0x20) << 16) | w28) >> 1;
                    carry = (be16(img + row_dest + 0x38) & 1) != 0;
                    wr16(img + row_desc + 0x18, (uint16_t)pair10);
                    wr16(img + row_desc + 0x10, (uint16_t)((((w08 & 1) << 31) | pair10) >> 16));
                    wr16(img + row_desc + 0x28, (uint16_t)pair20);
                    wr16(img + row_desc + 0x20, (uint16_t)((((w18 & 1) << 31) | pair20) >> 16));
                    acc = (hi_mask & ((acc << 31) | (long00 >> 1))) | (lo_mask & long00);
                    wr16(img + row_desc + 8, (uint16_t)acc);
                    wr16(img + row_desc, (uint16_t)((dmask & data) |
                                               (~dmask & (acc >> 16))));
                    acc = (~end_mask & (((w28 & 1) << 31) | (long30 >> 1))) | (end_mask & long30);
                    wr16(img + row_desc + 0x38, (uint16_t)acc);
                    wr16(img + row_desc + 0x30, (uint16_t)(acc >> 16));
                    data = r16l((uint16_t)data, 4);
                    row_dest += 2; row_desc += 2;
                    if (k == 1)
                        break;
                }
                w_dest += 0xa0;
                w_src += be32(img + A_DAT_1A534);
                w_desc += 0x40;
                if (row == 0)
                    break;
            }
        } else if (planes == 3) {
            case_d4 = 0xb;
            carry = 0;
            for (uint32_t row = rows;; row = (row - 1) & 0xffff) {
                uint32_t row_dest = w_dest, row_desc = w_desc;
                uint32_t word = r16l(be16(img + w_src), subword);
                word = r16r((uint16_t)word, pix);
                data = word;
                uint16_t dmask = r16r(0x8000, pix);
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t w08 = be16(img + row_dest + 8);
                    uint32_t pair20 = ((uint32_t)be16(img + row_dest) << 16) | w08;
                    uint32_t w18 = be16(img + row_dest + 0x18);
                    uint32_t long00 = ((uint32_t)be16(img + row_dest + 0x20) << 16) |
                                  be16(img + row_dest + 0x28);
                    uint32_t acc = (uint32_t)carry;
                    uint32_t pair10 = (((uint32_t)be16(img + row_dest + 0x10) << 16) | w18) >> 1;
                    carry = (be16(img + row_dest + 0x28) & 1) != 0;
                    wr16(img + row_desc + 0x18, (uint16_t)pair10);
                    wr16(img + row_desc + 0x10, (uint16_t)((((w08 & 1) << 31) | pair10) >> 16));
                    acc = (hi_mask & ((acc << 31) | (pair20 >> 1))) | (lo_mask & pair20);
                    wr16(img + row_desc + 8, (uint16_t)acc);
                    wr16(img + row_desc, (uint16_t)((dmask & data) |
                                               (~dmask & (acc >> 16))));
                    acc = (~end_mask & (((w18 & 1) << 31) | (long00 >> 1))) | (end_mask & long00);
                    wr16(img + row_desc + 0x28, (uint16_t)acc);
                    wr16(img + row_desc + 0x20, (uint16_t)(acc >> 16));
                    data = r16l((uint16_t)data, 4);
                    row_dest += 2; row_desc += 2;
                    if (k == 1)
                        break;
                }
                w_dest += 0xa0;
                w_src += be32(img + A_DAT_1A534);
                w_desc += 0x30;
                if (row == 0)
                    break;
            }
        } else if (planes == 2) {
            case_d4 = 7;
            int bit = 0;
            for (uint32_t row = rows;; row = (row - 1) & 0xffff) {
                uint32_t row_dest = w_dest, row_desc = w_desc;
                uint32_t word = r16l(be16(img + w_src), subword);
                word = r16r((uint16_t)word, pix);
                data = word;
                uint16_t dmask = r16r(0x8000, pix);
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t w08 = be16(img + row_dest + 8);
                    uint32_t pair10 = ((uint32_t)be16(img + row_dest) << 16) | w08;
                    uint32_t pair20 = ((uint32_t)be16(img + row_dest + 0x10) << 16) |
                                  be16(img + row_dest + 0x18);
                    uint32_t acc = (uint32_t)bit;
                    bit = (be16(img + row_dest + 0x18) & 1) != 0;
                    acc = (hi_mask & ((acc << 31) | (pair10 >> 1))) | (lo_mask & pair10);
                    wr16(img + row_desc + 8, (uint16_t)acc);
                    wr16(img + row_desc, (uint16_t)((dmask & data) |
                                               (~dmask & (acc >> 16))));
                    acc = (~end_mask & (((w08 & 1) << 31) | (pair20 >> 1))) | (end_mask & pair20);
                    wr16(img + row_desc + 0x18, (uint16_t)acc);
                    wr16(img + row_desc + 0x10, (uint16_t)(acc >> 16));
                    data = r16l((uint16_t)data, 4);
                    row_dest += 2; row_desc += 2;
                    if (k == 1)
                        break;
                }
                w_dest += 0xa0;
                w_src += be32(img + A_DAT_1A534);
                w_desc += 0x20;
                if (row == 0)
                    break;
            }
        } else {
            case_d4 = 3;
            int bit = 1;
            for (uint32_t row = rows;; row = (row - 1) & 0xffff) {
                uint32_t row_dest = w_dest, row_desc = w_desc;
                uint32_t word = r16l(be16(img + w_src), subword);
                word = r16r((uint16_t)word, pix);
                data = word;
                uint16_t dmask = r16r(0x8000, pix);
                bit = carry;
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t long00b = ((uint32_t)be16(img + row_dest) << 16) |
                                    be16(img + row_dest + 8);
                    carry = (be16(img + row_dest + 8) & 1) != 0;
                    long00b = (~(end_mask | lo_mask) & (((uint32_t)bit << 31) | (long00b >> 1))) |
                           ((end_mask | lo_mask) & long00b);
                    wr16(img + row_desc + 8, (uint16_t)long00b);
                    wr16(img + row_desc, (uint16_t)((dmask & data) |
                                               (~dmask & (long00b >> 16))));
                    data = r16l((uint16_t)data, 4);
                    row_dest += 2; row_desc += 2;
                    bit = carry;
                    if (k == 1)
                        break;
                }
                w_dest += 0xa0;
                w_src += be32(img + A_DAT_1A534);
                w_desc += 0x10;
                if (row == 0)
                    break;
            }
        }
        (void)w_dest; (void)w_desc;

        wait_vbl(img);
        copy_scratch_rows(img, s_dest, case_d4, s_height);
        if ((s_sub & 3) == 0)
            src = s_src - 2;
        else
            src = s_src;
        width = (s_width + 1) & 0xffff;
        sub = (s_sub - 1) & 0xffff;
        pix = s_pix;
        height = s_height;
        dest = s_dest;
        if (sub == 0xffff)
            break;
    }
    overlay_blit_regs[0] = pix; overlay_blit_regs[1] = sub; overlay_blit_regs[2] = width;
    overlay_blit_regs[3] = height; overlay_blit_regs[4] = dest; overlay_blit_regs[5] = src;
}

/* FUN_000111ac @ 0x111ac */
void overlay_compose_planes_wrap(uint8_t *img, uint32_t pix, uint32_t sub, uint32_t width, uint32_t height,
                 uint32_t dest, uint32_t src) {
    (void)width;
    overlay_compose_planes(img, pix, (sub << 2) - 1, 5, height - 1, dest, src);
}

/* FUN_000114ba @ 0x114ba: variante de 32 bits de 0x111b6; el acarreo vive en D6
 * (entrada/salida) y el sub-word usa (3 - (D1&3)). Compone igual en 0xd8f0 y
 * aplica con 0x123a0. FUN_000114b0 @ 0x114b0 fija D3-=1, D2=5, D1=(D1<<2)-1. */
void overlay_compose_planes32(uint8_t *img, uint32_t d0in, uint32_t d1in, uint32_t d2in,
                 uint32_t d3in, uint32_t carry_in, uint32_t a0in, uint32_t a1in) {
    uint32_t pix = d0in & 0xffff, sub = d1in & 0xffff, width = d2in & 0xffff;
    uint32_t height = d3in & 0xffff, dest = a0in, src = a1in, carry = carry_in;

    for (;;) {
        uint32_t s_pix = pix, s_sub = sub, s_width = width, s_height = height, s_dest = dest, s_src = src;

        uint32_t hi_mask = 0xffffffffu >> (pix & 0x3f);
        uint32_t lo_mask = ~hi_mask;
        uint32_t shift = (0x10 - pix) & 0xffff;
        uint32_t planes = 1;
        int bit = 1;
        uint32_t rest = (width - shift) & 0xffff;
        if (shift <= width && rest != 0) {
            for (;;) {
                planes = (planes + 1) & 0xffff;
                bit ^= 1;
                int bit2 = (0xf < rest);
                rest = (rest - 0x10) & 0xffff;
                if (!(bit2 && rest != 0))
                    break;
            }
        }
        uint32_t end_mask = (~(0xffffffffu << ((uint32_t)(-(int32_t)rest) & 0x3f))) &
                          0xffffffffu;
        shift = (rest + 0x1f) & 0xffff;
        planes = (planes >> 1) & 0xffff;
        if (bit) {
            planes = (planes + 1) & 0xffff;
            shift = (rest + 0xf) & 0xffff;
            end_mask = (end_mask << 16) | (end_mask >> 16) | 0xffff;
        }
        uint32_t rot6 = (3 - (sub & 3)) & 0xffff;
        uint32_t rotm = shift;
        uint32_t rows = height;
        uint32_t w_dest = dest, w_src = src, w_desc = A_SAVE_DESC;
        uint32_t case_d4;
        bit = 0;

        if (planes == 4) {
            case_d4 = 0xf;
            for (uint32_t row = rows;; row = (row - 1) & 0xffff) {
                uint32_t row_dest = w_dest, row_desc = w_desc;
                uint32_t hi = (carry >> 16) & 0xffff;
                uint32_t word = ((be16(img + w_src) << 16) | hi) & 0xffffffffu;
                word = rotate_right32(word, rotm);
                carry = rotate_left32(word, rot6);
                uint32_t dmask = rotate_right32(0x80000000u, rotm);
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t w0 = be16(img + row_dest), w4 = be16(img + row_dest + 8);
                    uint32_t w8 = be16(img + row_dest + 0x10), wc = be16(img + row_dest + 0x18);
                    uint32_t w10 = be16(img + row_dest + 0x20), w14 = be16(img + row_dest + 0x28);
                    uint32_t w18 = be16(img + row_dest + 0x30), w1c = be16(img + row_dest + 0x38);
                    uint32_t long0 = (w0 << 16) | w4;
                    uint32_t long30 = (w18 << 16) | w1c;
                    uint32_t carry_in = (uint32_t)bit;
                    uint32_t acc = (((w10 << 16) | w14) << 1);
                    uint32_t acc2 = (((w8 << 16) | wc) << 1);
                    bit = (w0 & 0x8000) != 0;
                    wr16(img + row_desc + 0x18, (uint16_t)(acc2 | ((w10 & 0x8000) != 0)));
                    wr16(img + row_desc + 0x10, (uint16_t)(acc2 >> 16));
                    wr16(img + row_desc + 0x28, (uint16_t)(acc | ((w18 & 0x8000) != 0)));
                    wr16(img + row_desc + 0x20, (uint16_t)(acc >> 16));
                    acc = (hi_mask & ((long0 << 1) | ((w8 & 0x8000) != 0))) | (lo_mask & long0);
                    wr16(img + row_desc + 8, (uint16_t)acc);
                    wr16(img + row_desc, (uint16_t)(acc >> 16));
                    acc = (dmask & carry) |
                          ((~dmask) & ((~end_mask & ((long30 << 1) | carry_in)) | (end_mask & long30)));
                    wr16(img + row_desc + 0x38, (uint16_t)acc);
                    wr16(img + row_desc + 0x30, (uint16_t)(acc >> 16));
                    carry = rotate_left32(carry, 4);
                    row_dest += 2; row_desc += 2;
                    if (k == 1) break;
                }
                w_dest += 0xa0;
                w_src += be32(img + A_DAT_1A534);
                w_desc += 0x40;
                if (row == 0) break;
            }
        } else if (planes == 3) {
            case_d4 = 0xb;
            for (uint32_t row = rows;; row = (row - 1) & 0xffff) {
                uint32_t row_dest = w_dest, row_desc = w_desc;
                uint32_t hi = (carry >> 16) & 0xffff;
                uint32_t word = ((be16(img + w_src) << 16) | hi) & 0xffffffffu;
                word = rotate_right32(word, rotm);
                carry = rotate_left32(word, rot6);
                uint32_t dmask = rotate_right32(0x80000000u, rotm);
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t w0 = be16(img + row_dest), w4 = be16(img + row_dest + 8);
                    uint32_t w8 = be16(img + row_dest + 0x10), wc = be16(img + row_dest + 0x18);
                    uint32_t w10 = be16(img + row_dest + 0x20), w14 = be16(img + row_dest + 0x28);
                    uint32_t carry_in = (w0 << 16) | w4;
                    uint32_t long0 = (w10 << 16) | w14;
                    uint32_t acc2 = (uint32_t)bit;
                    uint32_t acc = (((w8 << 16) | wc) << 1);
                    bit = (w0 & 0x8000) != 0;
                    wr16(img + row_desc + 0x18, (uint16_t)(acc | ((w10 & 0x8000) != 0)));
                    wr16(img + row_desc + 0x10, (uint16_t)(acc >> 16));
                    acc = (hi_mask & ((carry_in << 1) | ((w8 & 0x8000) != 0))) | (lo_mask & carry_in);
                    wr16(img + row_desc + 8, (uint16_t)acc);
                    wr16(img + row_desc, (uint16_t)(acc >> 16));
                    acc = (dmask & carry) |
                          ((~dmask) & ((~end_mask & ((long0 << 1) | acc2)) | (end_mask & long0)));
                    wr16(img + row_desc + 0x28, (uint16_t)acc);
                    wr16(img + row_desc + 0x20, (uint16_t)(acc >> 16));
                    carry = rotate_left32(carry, 4);
                    row_dest += 2; row_desc += 2;
                    if (k == 1) break;
                }
                w_dest += 0xa0;
                w_src += be32(img + A_DAT_1A534);
                w_desc += 0x30;
                if (row == 0) break;
            }
        } else if (planes == 2) {
            case_d4 = 7;
            int bit2 = 0;
            for (uint32_t row = rows;; row = (row - 1) & 0xffff) {
                uint32_t row_dest = w_dest, row_desc = w_desc;
                uint32_t hi = (carry >> 16) & 0xffff;
                uint32_t word = ((be16(img + w_src) << 16) | hi) & 0xffffffffu;
                word = rotate_right32(word, rotm);
                carry = rotate_left32(word, rot6);
                uint32_t dmask = rotate_right32(0x80000000u, rotm);
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t w0 = be16(img + row_dest), w4 = be16(img + row_dest + 8);
                    uint32_t w8 = be16(img + row_dest + 0x10), wc = be16(img + row_dest + 0x18);
                    uint32_t acc = (w0 << 16) | w4;
                    uint32_t carry_in = (w8 << 16) | wc;
                    uint32_t acc2 = (uint32_t)bit2;
                    bit2 = (w0 & 0x8000) != 0;
                    acc = (hi_mask & ((acc << 1) | ((w8 & 0x8000) != 0))) | (lo_mask & acc);
                    wr16(img + row_desc + 8, (uint16_t)acc);
                    wr16(img + row_desc, (uint16_t)(acc >> 16));
                    acc = (dmask & carry) |
                          ((~dmask) & ((~end_mask & ((carry_in << 1) | acc2)) | (end_mask & carry_in)));
                    wr16(img + row_desc + 0x18, (uint16_t)acc);
                    wr16(img + row_desc + 0x10, (uint16_t)(acc >> 16));
                    carry = rotate_left32(carry, 4);
                    row_dest += 2; row_desc += 2;
                    if (k == 1) break;
                }
                w_dest += 0xa0;
                w_src += be32(img + A_DAT_1A534);
                w_desc += 0x20;
                if (row == 0) break;
            }
        } else {
            case_d4 = 3;
            int bit2 = 1;
            for (uint32_t row = rows;; row = (row - 1) & 0xffff) {
                uint32_t row_dest = w_dest, row_desc = w_desc;
                uint32_t hi = (carry >> 16) & 0xffff;
                uint32_t word = ((be16(img + w_src) << 16) | hi) & 0xffffffffu;
                word = rotate_right32(word, rotm);
                carry = rotate_left32(word, rot6);
                uint32_t dmask = rotate_right32(0x80000000u, rotm);
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t w0 = be16(img + row_dest), w4 = be16(img + row_dest + 8);
                    uint32_t word2 = (w0 << 16) | w4;
                    bit = (w0 & 0x8000) != 0;
                    word2 = (dmask & carry) |
                           ((~dmask) & ((~(end_mask | lo_mask) & ((word2 << 1) | (uint32_t)bit2)) |
                                        ((end_mask | lo_mask) & word2)));
                    wr16(img + row_desc + 8, (uint16_t)word2);
                    wr16(img + row_desc, (uint16_t)(word2 >> 16));
                    carry = rotate_left32(carry, 4);
                    row_dest += 2; row_desc += 2;
                    bit2 = bit;
                    if (k == 1) break;
                }
                w_dest += 0xa0;
                w_src += be32(img + A_DAT_1A534);
                w_desc += 0x10;
                if (row == 0) break;
            }
        }
        (void)w_dest; (void)w_desc;

        wait_vbl(img);
        copy_scratch_rows(img, s_dest, case_d4, s_height);
        if ((s_sub & 3) == 0)
            src = s_src + 2;
        else
            src = s_src;
        width = (s_width + 1) & 0xffff;
        if (s_pix != 0) {
            pix = (s_pix - 1) & 0xffff;
            dest = s_dest;
        } else {
            pix = 0xf;
            dest = s_dest - 8;
        }
        sub = (s_sub - 1) & 0xffff;
        height = s_height;
        if (sub == 0xffff)
            break;
    }
    overlay_blit_regs[0] = pix; overlay_blit_regs[1] = sub; overlay_blit_regs[2] = width;
    overlay_blit_regs[3] = height; overlay_blit_regs[4] = dest; overlay_blit_regs[5] = src;
}

/* 0x114b0 — 0x114ba con D3-=1, D2=5, D1=(D1<<2)-1 */
void overlay_compose_planes32_wrap(uint8_t *img, uint32_t pix, uint32_t sub, uint32_t width, uint32_t height,
                 uint32_t carry, uint32_t dest, uint32_t src) {
    (void)width;
    overlay_compose_planes32(img, pix, (sub << 2) - 1, 5, height - 1, carry, dest, src);
}

/* FUN_000117fe @ 0x117fe: compone dos fuentes (A0 = mascara, A1 = dato) en 0xd8f0
 * con la mascara ror32(0x80000000, D0), lo aplica con 0x123a0 y repite. El flag
 * 0x1a53e (lo fija el envoltorio) se resta a D2 cada pasada. FUN_000117de fija
 * 0x1a53e=1 y FUN_000117e6 =0; ambas preparan D1/D2/D3 y A1 = A0-0x8000. */
void overlay_compose_masked(uint8_t *img, uint32_t d0in, uint32_t d1in, uint32_t d2in,
                 uint32_t d3in, uint32_t a0in, uint32_t a1in) {
    uint32_t pix = d0in & 0xffff, sub = d1in & 0xffff, width = d2in & 0xffff;
    uint32_t height = d3in & 0xffff, dest = a0in, src = a1in;

    for (;;) {
        uint32_t s_pix = pix, s_sub = sub, s_width = width, s_height = height, s_dest = dest, s_src = src;
        uint32_t pix_mask = rotate_right32(0x80000000u, pix);
        uint32_t hi_mask = 0xffffffffu >> (pix & 0x3f);
        uint32_t lo_mask = ~(hi_mask >> 1);
        uint32_t planes = 1;
        int carry = 1;
        uint32_t first_bits = (0x10 - pix) & 0xffff;
        uint32_t rest = (width - first_bits) & 0xffff;
        if (first_bits <= width && rest != 0) {
            for (;;) {
                planes = (planes + 1) & 0xffff;
                carry ^= 1;
                int more = (0xf < rest);
                rest = (rest - 0x10) & 0xffff;
                if (!(more && rest != 0))
                    break;
            }
        }
        uint32_t end_mask = (~(0xffffffffu << ((uint32_t)(-(int32_t)rest) & 0x3f))) &
                          0xffffffffu;
        planes = (planes >> 1) & 0xffff;
        if (carry) {
            planes = (planes + 1) & 0xffff;
            end_mask = (end_mask << 16) | (end_mask >> 16) | 0xffff;
        }
        uint32_t rows = height;
        uint32_t w_dest = dest, w_src = src, w_desc = A_SAVE_DESC;
        uint32_t case_d4;

        if (planes == 2) {
            case_d4 = 7;
            for (uint32_t row = rows;; row = (row - 1) & 0xffff) {
                uint32_t row_dest = w_dest, row_src = w_src, row_desc = w_desc;
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t w4 = be16(img + row_dest + 8);
                    uint32_t acc = ((uint32_t)be16(img + row_dest) << 16) | w4;
                    uint32_t w10 = be16(img + row_dest + 0x10);
                    uint32_t w18 = be16(img + row_dest + 0x18);
                    uint32_t src1 = ((uint32_t)be16(img + row_src) << 16) | be16(img + row_src + 8);
                    acc = (~pix_mask & (((acc & hi_mask) >> 1) | (lo_mask & acc))) | (pix_mask & src1);
                    wr16(img + row_desc + 8, (uint16_t)acc);
                    wr16(img + row_desc, (uint16_t)(acc >> 16));
                    acc = (~end_mask & (((w4 & 1) << 31) | (((w10 << 16) | w18) >> 1))) |
                         (end_mask & ((w10 << 16) | w18));
                    wr16(img + row_desc + 0x18, (uint16_t)acc);
                    wr16(img + row_desc + 0x10, (uint16_t)(acc >> 16));
                    row_dest += 2; row_src += 2; row_desc += 2;
                    if (k == 1) break;
                }
                w_dest += 0xa0; w_src += 0xa0; w_desc += 0x20;
                if (row == 0) break;
            }
        } else if (planes == 3) {
            case_d4 = 0xb;
            for (uint32_t row = rows;; row = (row - 1) & 0xffff) {
                uint32_t row_dest = w_dest, row_src = w_src, row_desc = w_desc;
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t w4 = be16(img + row_dest + 8);
                    uint32_t acc2 = ((uint32_t)be16(img + row_dest) << 16) | w4;
                    uint32_t w10 = be16(img + row_dest + 0x10);
                    uint32_t w18 = be16(img + row_dest + 0x18);
                    uint32_t w20 = be16(img + row_dest + 0x20);
                    uint32_t w28 = be16(img + row_dest + 0x28);
                    uint32_t src1 = ((uint32_t)be16(img + row_src) << 16) | be16(img + row_src + 8);
                    uint32_t acc = ((((uint32_t)w10 << 16) | w18) >> 1);
                    wr16(img + row_desc + 0x18, (uint16_t)acc);
                    wr16(img + row_desc + 0x10, (uint16_t)((((w4 & 1) << 31) | acc) >> 16));
                    acc = (~pix_mask & (((hi_mask & acc2) >> 1) | (lo_mask & acc2))) | (pix_mask & src1);
                    wr16(img + row_desc + 8, (uint16_t)acc);
                    wr16(img + row_desc, (uint16_t)(acc >> 16));
                    acc = (~end_mask & (((w18 & 1) << 31) | (((w20 << 16) | w28) >> 1))) |
                         (end_mask & ((w20 << 16) | w28));
                    wr16(img + row_desc + 0x28, (uint16_t)acc);
                    wr16(img + row_desc + 0x20, (uint16_t)(acc >> 16));
                    row_dest += 2; row_src += 2; row_desc += 2;
                    if (k == 1) break;
                }
                w_dest += 0xa0; w_src += 0xa0; w_desc += 0x30;
                if (row == 0) break;
            }
        } else if (planes == 4) {
            case_d4 = 0xf;
            for (uint32_t row = rows;; row = (row - 1) & 0xffff) {
                uint32_t row_dest = w_dest, row_src = w_src, row_desc = w_desc;
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t w4 = be16(img + row_dest + 8);
                    uint32_t long0 = ((uint32_t)be16(img + row_dest) << 16) | w4;
                    uint32_t w10 = be16(img + row_dest + 0x10);
                    uint32_t w18 = be16(img + row_dest + 0x18);
                    uint32_t w20 = be16(img + row_dest + 0x20);
                    uint32_t w28 = be16(img + row_dest + 0x28);
                    uint32_t w30 = be16(img + row_dest + 0x30);
                    uint32_t w38 = be16(img + row_dest + 0x38);
                    uint32_t src1 = ((uint32_t)be16(img + row_src) << 16) | be16(img + row_src + 8);
                    uint32_t acc = ((((uint32_t)w10 << 16) | w18) >> 1);
                    uint32_t acc2 = ((((uint32_t)w20 << 16) | w28) >> 1);
                    wr16(img + row_desc + 0x18, (uint16_t)acc);
                    wr16(img + row_desc + 0x10, (uint16_t)((((w4 & 1) << 31) | acc) >> 16));
                    wr16(img + row_desc + 0x28, (uint16_t)acc2);
                    wr16(img + row_desc + 0x20, (uint16_t)((((w18 & 1) << 31) | acc2) >> 16));
                    acc = (~pix_mask & (((hi_mask & long0) >> 1) | (lo_mask & long0))) | (pix_mask & src1);
                    wr16(img + row_desc + 8, (uint16_t)acc);
                    wr16(img + row_desc, (uint16_t)(acc >> 16));
                    acc = (~end_mask & (((w28 & 1) << 31) | (((w30 << 16) | w38) >> 1))) |
                         (end_mask & ((w30 << 16) | w38));
                    wr16(img + row_desc + 0x38, (uint16_t)acc);
                    wr16(img + row_desc + 0x30, (uint16_t)(acc >> 16));
                    row_dest += 2; row_src += 2; row_desc += 2;
                    if (k == 1) break;
                }
                w_dest += 0xa0; w_src += 0xa0; w_desc += 0x40;
                if (row == 0) break;
            }
        } else {
            case_d4 = 3;
            int bit = 1;
            for (uint32_t row = rows;; row = (row - 1) & 0xffff) {
                uint32_t row_dest = w_dest, row_src = w_src, row_desc = w_desc;
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t w0 = be16(img + row_dest);
                    uint32_t w4 = be16(img + row_dest + 8);
                    uint32_t src1 = ((uint32_t)be16(img + row_src) << 16) | be16(img + row_src + 8);
                    uint32_t long0b = (w0 << 16) | w4;
                    carry = (w4 & 1) != 0;
                    long0b = (~pix_mask & ((~(end_mask | lo_mask) &
                                        (((uint32_t)bit << 31) | (long0b >> 1))) |
                                       ((end_mask | lo_mask) & long0b))) |
                           (pix_mask & src1);
                    wr16(img + row_desc + 8, (uint16_t)long0b);
                    wr16(img + row_desc, (uint16_t)(long0b >> 16));
                    row_dest += 2; row_src += 2; row_desc += 2;
                    bit = carry;
                    if (k == 1) break;
                }
                w_dest += 0xa0; w_src += 0xa0; w_desc += 0x10;
                if (row == 0) break;
            }
        }
        (void)w_dest; (void)w_desc;

        wait_vbl(img);
        copy_scratch_rows(img, s_dest, case_d4, s_height);
        width = (s_width - be16(img + 0x1a53e)) & 0xffff;
        uint32_t nd0 = (s_pix + 1) & 0xf;
        if (nd0 == 0) {
            src = s_src + 8;
            dest = s_dest + 8;
        } else {
            src = s_src;
            dest = s_dest;
        }
        pix = nd0;
        sub = (s_sub - 1) & 0xffff;
        height = s_height;
        if (sub == 0xffff)
            break;
    }
    overlay_blit_regs[0] = pix; overlay_blit_regs[1] = sub; overlay_blit_regs[2] = width;
    overlay_blit_regs[3] = height; overlay_blit_regs[4] = dest; overlay_blit_regs[5] = src;
}

/* 0x117de — 0x117fe con 0x1a53e=1 */
void overlay_compose_masked_wrap(uint8_t *img, uint32_t pix, uint32_t sub, uint32_t width, uint32_t height,
                 uint32_t dest) {
    wr16(img + 0x1a53e, 1);
    overlay_compose_masked(img, pix, (sub << 2) - 1, width << 2, height - 1, dest, dest - 0x8000);
}

/* 0x11afa — variante roxl (flag 0x1a53e) */
void overlay_compose_shift(uint8_t *img, uint32_t d0in, uint32_t d1in, uint32_t d2in,
                 uint32_t d3in, uint32_t a0in, uint32_t a1in) {
    uint32_t pix = d0in & 0xffff, sub = d1in & 0xffff, width = d2in & 0xffff;
    uint32_t height = d3in & 0xffff, dest = a0in, src = a1in;
    uint32_t flag = be16(img + 0x1a53e);

    for (;;) {
        uint32_t s_height = height;
        uint32_t hi_mask = 0xffffffffu >> (pix & 0x3f);
        uint32_t lo_mask = ~hi_mask;
        uint32_t planes = 1;
        int carry = 1;
        uint32_t first_bits = (0x10 - pix) & 0xffff;
        uint32_t rest = (width - first_bits) & 0xffff;
        if (first_bits <= width && rest != 0) {
            for (;;) {
                planes = (planes + 1) & 0xffff;
                carry ^= 1;
                int more = (0xf < rest);
                rest = (rest - 0x10) & 0xffff;
                if (!(more && rest != 0))
                    break;
            }
        }
        uint32_t neg = (0 - rest) & 0xffff;
        uint32_t end_mask = ~(0xffffffffu << (neg & 0x3f));
        uint32_t rotm = (rest + 0x1f) & 0xffff;
        planes = (planes >> 1) & 0xffff;
        if (carry) {
            planes = (planes + 1) & 0xffff;
            rotm = (rest + 0xf) & 0xffff;
            end_mask = ((end_mask << 16) | (end_mask >> 16)) | 0xffff;
        }
        uint32_t pix_mask = rotate_right32(0x80000000u, rotm & 0x3f);
        uint32_t rows = s_height;
        uint32_t w_dest = dest, w_src = src, w_desc = A_SAVE_DESC;
        uint32_t case_d4;

        if (planes == 2) {
            case_d4 = 7;
            uint32_t fill = 0;
            for (uint32_t row = rows;; row = (row - 1) & 0xffff) {
                uint32_t row_dest = w_dest, row_src = w_src, row_desc = w_desc;
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t src1 = ((uint32_t)be16(img + row_dest) << 16) | be16(img + row_dest + 8);
                    uint32_t src3 = ((uint32_t)be16(img + row_dest + 0x10) << 16) | be16(img + row_dest + 0x18);
                    uint32_t src0 = ((uint32_t)be16(img + row_src + 0x10) << 16) | be16(img + row_src + 0x18);
                    uint32_t sh3 = (src3 << 1) | fill;
                    uint32_t sh1 = (src1 << 1) | (src3 >> 31);
                    fill = src1 >> 31;
                    uint32_t acc = (src1 & lo_mask) | (sh1 & hi_mask);
                    uint32_t acc2 = (src3 & end_mask) | (sh3 & ~end_mask);
                    uint32_t acc3 = (src0 & pix_mask) | (acc2 & ~pix_mask);
                    wr16(img + row_desc + 8, (uint16_t)acc);
                    wr16(img + row_desc, (uint16_t)(acc >> 16));
                    wr16(img + row_desc + 0x18, (uint16_t)acc3);
                    wr16(img + row_desc + 0x10, (uint16_t)(acc3 >> 16));
                    row_dest += 2; row_src += 2; row_desc += 2;
                    if (k == 1) break;
                }
                w_dest += 0xa0; w_src += 0xa0; w_desc += 0x20;
                if (row == 0) break;
            }
        } else if (planes == 3) {
            case_d4 = 0xb;
            for (uint32_t row = rows;; row = (row - 1) & 0xffff) {
                uint32_t row_dest = w_dest, row_src = w_src, row_desc = w_desc;
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t src0 = ((uint32_t)be16(img + row_src + 0x20) << 16) | be16(img + row_src + 0x28);
                    uint32_t src1 = ((uint32_t)be16(img + row_dest) << 16) | be16(img + row_dest + 8);
                    uint32_t src2 = ((uint32_t)be16(img + row_dest + 0x10) << 16) | be16(img + row_dest + 0x18);
                    uint32_t src3 = ((uint32_t)be16(img + row_dest + 0x20) << 16) | be16(img + row_dest + 0x28);
                    uint32_t sh3 = src3 << 1;
                    uint32_t sh2 = (src2 << 1) | (src3 >> 31);
                    uint32_t sh1 = (src1 << 1) | (src2 >> 31);
                    uint32_t acc = (src1 & lo_mask) | (sh1 & hi_mask);
                    uint32_t acc2 = (src3 & end_mask) | (sh3 & ~end_mask);
                    uint32_t acc3 = (src0 & pix_mask) | (acc2 & ~pix_mask);
                    wr16(img + row_desc + 0x18, (uint16_t)sh2);
                    wr16(img + row_desc + 0x10, (uint16_t)(sh2 >> 16));
                    wr16(img + row_desc + 8, (uint16_t)acc);
                    wr16(img + row_desc, (uint16_t)(acc >> 16));
                    wr16(img + row_desc + 0x28, (uint16_t)acc3);
                    wr16(img + row_desc + 0x20, (uint16_t)(acc3 >> 16));
                    row_dest += 2; row_src += 2; row_desc += 2;
                    if (k == 1) break;
                }
                w_dest += 0xa0; w_src += 0xa0; w_desc += 0x30;
                if (row == 0) break;
            }
        } else if (planes == 4) {
            case_d4 = 0xf;
            for (uint32_t row = rows;; row = (row - 1) & 0xffff) {
                uint32_t row_dest = w_dest, row_src = w_src, row_desc = w_desc;
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t src0 = ((uint32_t)be16(img + row_src + 0x30) << 16) | be16(img + row_src + 0x38);
                    uint32_t src1 = ((uint32_t)be16(img + row_dest) << 16) | be16(img + row_dest + 8);
                    uint32_t src2 = ((uint32_t)be16(img + row_dest + 0x10) << 16) | be16(img + row_dest + 0x18);
                    uint32_t src4 = ((uint32_t)be16(img + row_dest + 0x20) << 16) | be16(img + row_dest + 0x28);
                    uint32_t src3 = ((uint32_t)be16(img + row_dest + 0x30) << 16) | be16(img + row_dest + 0x38);
                    uint32_t sh3 = src3 << 1;
                    uint32_t sh4 = (src4 << 1) | (src3 >> 31);
                    uint32_t sh2 = (src2 << 1) | (src4 >> 31);
                    uint32_t sh1 = (src1 << 1) | (src2 >> 31);
                    uint32_t acc = (src1 & lo_mask) | (sh1 & hi_mask);
                    uint32_t acc2 = (src3 & end_mask) | (sh3 & ~end_mask);
                    uint32_t acc3 = (src0 & pix_mask) | (acc2 & ~pix_mask);
                    wr16(img + row_desc + 0x18, (uint16_t)sh2);
                    wr16(img + row_desc + 0x10, (uint16_t)(sh2 >> 16));
                    wr16(img + row_desc + 0x28, (uint16_t)sh4);
                    wr16(img + row_desc + 0x20, (uint16_t)(sh4 >> 16));
                    wr16(img + row_desc + 8, (uint16_t)acc);
                    wr16(img + row_desc, (uint16_t)(acc >> 16));
                    wr16(img + row_desc + 0x38, (uint16_t)acc3);
                    wr16(img + row_desc + 0x30, (uint16_t)(acc3 >> 16));
                    row_dest += 2; row_src += 2; row_desc += 2;
                    if (k == 1) break;
                }
                w_dest += 0xa0; w_src += 0xa0; w_desc += 0x40;
                if (row == 0) break;
            }
        } else {
            case_d4 = 3;
            uint32_t fill = 0;
            for (uint32_t row = rows;; row = (row - 1) & 0xffff) {
                uint32_t row_dest = w_dest, row_src = w_src, row_desc = w_desc;
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t src1 = ((uint32_t)be16(img + row_dest) << 16) | be16(img + row_dest + 8);
                    uint32_t src0 = ((uint32_t)be16(img + row_src) << 16) | be16(img + row_src + 8);
                    uint32_t sh1 = (src1 << 1) | fill;
                    fill = src1 >> 31;
                    uint32_t acc = (src1 & (lo_mask | end_mask)) | (sh1 & ~(lo_mask | end_mask));
                    uint32_t acc2 = (src0 & pix_mask) | (acc & ~pix_mask);
                    wr16(img + row_desc + 8, (uint16_t)acc2);
                    wr16(img + row_desc, (uint16_t)(acc2 >> 16));
                    row_dest += 2; row_src += 2; row_desc += 2;
                    if (k == 1) break;
                }
                w_dest += 0xa0; w_src += 0xa0; w_desc += 0x10;
                if (row == 0) break;
            }
        }
        (void)w_dest; (void)w_desc;

        wait_vbl(img);
        copy_scratch_rows(img, dest, case_d4, s_height);
        width = (width - flag) & 0xffff;
        if (flag == 0) {
            uint32_t before = pix;
            pix = (pix - 1) & 0xffff;
            if (before == 0) {
                src = (src - 8) & 0xffffffff;
                dest = (dest - 8) & 0xffffffff;
                pix = 0xf;
            }
        }
        if (sub == 0)
            break;
        sub = (sub - 1) & 0xffff;
    }
    overlay_blit_regs[0] = pix; overlay_blit_regs[1] = 0xffff; overlay_blit_regs[2] = width;
    overlay_blit_regs[3] = height; overlay_blit_regs[4] = dest; overlay_blit_regs[5] = src;
}

/* 0x11ada — 0x11afa con 0x1a53e=1 */
void overlay_compose_shift_wrap(uint8_t *img, uint32_t pix, uint32_t sub, uint32_t width, uint32_t height,
                 uint32_t dest) {
    wr16(img + 0x1a53e, 1);
    overlay_compose_shift(img, pix, (sub << 2) - 1, width << 2, height - 1, dest, dest - 0x8000);
}
