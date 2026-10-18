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

void wait_vbl(uint8_t *img);
void copy_scratch_rows(uint8_t *img, uint32_t a0, uint32_t d4, uint32_t d5);

static uint16_t r16l(uint16_t v, uint32_t n) { return rotate_left16(v, n); }
static uint16_t r16r(uint16_t v, uint32_t n) { return rotate_right16(v, n); }

/* Registro final del ultimo blitter de 0x11xxx (D0,D1,D2,D3,A0,A1). Lo usan
 * 0x12ce4/0x12d16 para guardar los bloques 0x1a82e/0x1a83e (movem d0/d2/a0-a1). */
uint32_t overlay_blit_regs[6];

void overlay_compose_planes(uint8_t *img, uint32_t d0in, uint32_t d1in, uint32_t d2in,
                 uint32_t d3in, uint32_t a0in, uint32_t a1in) {
    uint32_t d0 = d0in & 0xffff, d1 = d1in & 0xffff, d2 = d2in & 0xffff;
    uint32_t d3 = d3in & 0xffff, a0 = a0in, a1 = a1in;

    for (;;) {
        uint32_t sd0 = d0, sd1 = d1, sd2 = d2, sd3 = d3, sa0 = a0, sa1 = a1;

        uint32_t uVar11 = 0xffffffffu >> (d0 & 0x3f);
        uint32_t uVar8 = ~uVar11;
        uint32_t uVar4 = (0x10 - d0) & 0xffff;
        uint32_t uVar12 = 1;
        int bVar23 = 1;
        uint32_t uVar15 = (d2 - uVar4) & 0xffff;
        if (uVar4 <= d2 && uVar15 != 0) {
            for (;;) {
                uVar12 = (uVar12 + 1) & 0xffff;
                bVar23 ^= 1;
                uint32_t bVar22 = (0xf < uVar15);
                uVar15 = (uVar15 - 0x10) & 0xffff;
                if (!(bVar22 && uVar15 != 0))
                    break;
            }
        }
        uint32_t neg = (uint32_t)(-(int32_t)uVar15);
        uint32_t uVar13 = (~(0xffffffffu << (neg & 0x3f))) & 0xffffffffu;
        uVar12 = (uVar12 >> 1) & 0xffff;
        if (bVar23) {
            uVar12 = (uVar12 + 1) & 0xffff;
            uVar13 = (uVar13 << 16) | (uVar13 >> 16) | 0xffff;
        }

        uint32_t rows = d3;
        uint32_t subword = d1 & 3;
        uint32_t a2 = A_SAVE_DESC;
        uint32_t case_d4;
        uint32_t wa0 = a0, wa1 = a1, wa2 = a2;

        if (uVar12 == 4) {
            case_d4 = 0xf;
            bVar23 = 0;
            for (uint32_t r = rows;; r = (r - 1) & 0xffff) {
                uint32_t a0r = wa0, a2r = wa2;
                uint32_t d6 = r16l(be16(img + wa1), subword);
                d6 = r16r((uint16_t)d6, d0);
                uVar15 = d6;
                uint16_t dmask = r16r(0x8000, d0);
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t u3 = be16(img + a0r + 8);
                    uint32_t u9 = ((uint32_t)be16(img + a0r) << 16) | u3;
                    uint32_t u2 = be16(img + a0r + 0x18);
                    uint32_t u1 = be16(img + a0r + 0x28);
                    uint32_t u10 = ((uint32_t)be16(img + a0r + 0x30) << 16) |
                                   be16(img + a0r + 0x38);
                    uint32_t u5b = (uint32_t)bVar23;
                    uint32_t u7 = (((uint32_t)be16(img + a0r + 0x10) << 16) | u2) >> 1;
                    uint32_t u6 = (((uint32_t)be16(img + a0r + 0x20) << 16) | u1) >> 1;
                    bVar23 = (be16(img + a0r + 0x38) & 1) != 0;
                    wr16(img + a2r + 0x18, (uint16_t)u7);
                    wr16(img + a2r + 0x10, (uint16_t)((((u3 & 1) << 31) | u7) >> 16));
                    wr16(img + a2r + 0x28, (uint16_t)u6);
                    wr16(img + a2r + 0x20, (uint16_t)((((u2 & 1) << 31) | u6) >> 16));
                    u5b = (uVar11 & ((u5b << 31) | (u9 >> 1))) | (uVar8 & u9);
                    wr16(img + a2r + 8, (uint16_t)u5b);
                    wr16(img + a2r, (uint16_t)((dmask & uVar15) |
                                               (~dmask & (u5b >> 16))));
                    u5b = (~uVar13 & (((u1 & 1) << 31) | (u10 >> 1))) | (uVar13 & u10);
                    wr16(img + a2r + 0x38, (uint16_t)u5b);
                    wr16(img + a2r + 0x30, (uint16_t)(u5b >> 16));
                    uVar15 = r16l((uint16_t)uVar15, 4);
                    a0r += 2; a2r += 2;
                    if (k == 1)
                        break;
                }
                wa0 += 0xa0;
                wa1 += be32(img + A_DAT_1A534);
                wa2 += 0x40;
                if (r == 0)
                    break;
            }
        } else if (uVar12 == 3) {
            case_d4 = 0xb;
            bVar23 = 0;
            for (uint32_t r = rows;; r = (r - 1) & 0xffff) {
                uint32_t a0r = wa0, a2r = wa2;
                uint32_t d6 = r16l(be16(img + wa1), subword);
                d6 = r16r((uint16_t)d6, d0);
                uVar15 = d6;
                uint16_t dmask = r16r(0x8000, d0);
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t u3 = be16(img + a0r + 8);
                    uint32_t u6 = ((uint32_t)be16(img + a0r) << 16) | u3;
                    uint32_t u2 = be16(img + a0r + 0x18);
                    uint32_t u9 = ((uint32_t)be16(img + a0r + 0x20) << 16) |
                                  be16(img + a0r + 0x28);
                    uint32_t u5b = (uint32_t)bVar23;
                    uint32_t u7 = (((uint32_t)be16(img + a0r + 0x10) << 16) | u2) >> 1;
                    bVar23 = (be16(img + a0r + 0x28) & 1) != 0;
                    wr16(img + a2r + 0x18, (uint16_t)u7);
                    wr16(img + a2r + 0x10, (uint16_t)((((u3 & 1) << 31) | u7) >> 16));
                    u5b = (uVar11 & ((u5b << 31) | (u6 >> 1))) | (uVar8 & u6);
                    wr16(img + a2r + 8, (uint16_t)u5b);
                    wr16(img + a2r, (uint16_t)((dmask & uVar15) |
                                               (~dmask & (u5b >> 16))));
                    u5b = (~uVar13 & (((u2 & 1) << 31) | (u9 >> 1))) | (uVar13 & u9);
                    wr16(img + a2r + 0x28, (uint16_t)u5b);
                    wr16(img + a2r + 0x20, (uint16_t)(u5b >> 16));
                    uVar15 = r16l((uint16_t)uVar15, 4);
                    a0r += 2; a2r += 2;
                    if (k == 1)
                        break;
                }
                wa0 += 0xa0;
                wa1 += be32(img + A_DAT_1A534);
                wa2 += 0x30;
                if (r == 0)
                    break;
            }
        } else if (uVar12 == 2) {
            case_d4 = 7;
            int bVar22 = 0;
            for (uint32_t r = rows;; r = (r - 1) & 0xffff) {
                uint32_t a0r = wa0, a2r = wa2;
                uint32_t d6 = r16l(be16(img + wa1), subword);
                d6 = r16r((uint16_t)d6, d0);
                uVar15 = d6;
                uint16_t dmask = r16r(0x8000, d0);
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t u3 = be16(img + a0r + 8);
                    uint32_t u7 = ((uint32_t)be16(img + a0r) << 16) | u3;
                    uint32_t u6 = ((uint32_t)be16(img + a0r + 0x10) << 16) |
                                  be16(img + a0r + 0x18);
                    uint32_t u5b = (uint32_t)bVar22;
                    bVar22 = (be16(img + a0r + 0x18) & 1) != 0;
                    u5b = (uVar11 & ((u5b << 31) | (u7 >> 1))) | (uVar8 & u7);
                    wr16(img + a2r + 8, (uint16_t)u5b);
                    wr16(img + a2r, (uint16_t)((dmask & uVar15) |
                                               (~dmask & (u5b >> 16))));
                    u5b = (~uVar13 & (((u3 & 1) << 31) | (u6 >> 1))) | (uVar13 & u6);
                    wr16(img + a2r + 0x18, (uint16_t)u5b);
                    wr16(img + a2r + 0x10, (uint16_t)(u5b >> 16));
                    uVar15 = r16l((uint16_t)uVar15, 4);
                    a0r += 2; a2r += 2;
                    if (k == 1)
                        break;
                }
                wa0 += 0xa0;
                wa1 += be32(img + A_DAT_1A534);
                wa2 += 0x20;
                if (r == 0)
                    break;
            }
        } else {
            case_d4 = 3;
            int bVar22 = 1;
            for (uint32_t r = rows;; r = (r - 1) & 0xffff) {
                uint32_t a0r = wa0, a2r = wa2;
                uint32_t d6 = r16l(be16(img + wa1), subword);
                d6 = r16r((uint16_t)d6, d0);
                uVar15 = d6;
                uint16_t dmask = r16r(0x8000, d0);
                bVar22 = bVar23;
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t u11b = ((uint32_t)be16(img + a0r) << 16) |
                                    be16(img + a0r + 8);
                    bVar23 = (be16(img + a0r + 8) & 1) != 0;
                    u11b = (~(uVar13 | uVar8) & (((uint32_t)bVar22 << 31) | (u11b >> 1))) |
                           ((uVar13 | uVar8) & u11b);
                    wr16(img + a2r + 8, (uint16_t)u11b);
                    wr16(img + a2r, (uint16_t)((dmask & uVar15) |
                                               (~dmask & (u11b >> 16))));
                    uVar15 = r16l((uint16_t)uVar15, 4);
                    a0r += 2; a2r += 2;
                    bVar22 = bVar23;
                    if (k == 1)
                        break;
                }
                wa0 += 0xa0;
                wa1 += be32(img + A_DAT_1A534);
                wa2 += 0x10;
                if (r == 0)
                    break;
            }
        }
        (void)wa0; (void)wa2;

        wait_vbl(img);
        copy_scratch_rows(img, sa0, case_d4, sd3);
        if ((sd1 & 3) == 0)
            a1 = sa1 - 2;
        else
            a1 = sa1;
        d2 = (sd2 + 1) & 0xffff;
        d1 = (sd1 - 1) & 0xffff;
        d0 = sd0;
        d3 = sd3;
        a0 = sa0;
        if (d1 == 0xffff)
            break;
    }
    overlay_blit_regs[0] = d0; overlay_blit_regs[1] = d1; overlay_blit_regs[2] = d2;
    overlay_blit_regs[3] = d3; overlay_blit_regs[4] = a0; overlay_blit_regs[5] = a1;
}

/* FUN_000111ac @ 0x111ac */
void overlay_compose_planes_wrap(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3,
                 uint32_t a0, uint32_t a1) {
    (void)d2;
    overlay_compose_planes(img, d0, (d1 << 2) - 1, 5, d3 - 1, a0, a1);
}

/* FUN_000114ba @ 0x114ba: variante de 32 bits de 0x111b6; el acarreo vive en D6
 * (entrada/salida) y el sub-word usa (3 - (D1&3)). Compone igual en 0xd8f0 y
 * aplica con 0x123a0. FUN_000114b0 @ 0x114b0 fija D3-=1, D2=5, D1=(D1<<2)-1. */
void overlay_compose_planes32(uint8_t *img, uint32_t d0in, uint32_t d1in, uint32_t d2in,
                 uint32_t d3in, uint32_t d6in, uint32_t a0in, uint32_t a1in) {
    uint32_t d0 = d0in & 0xffff, d1 = d1in & 0xffff, d2 = d2in & 0xffff;
    uint32_t d3 = d3in & 0xffff, a0 = a0in, a1 = a1in, d6 = d6in;

    for (;;) {
        uint32_t sd0 = d0, sd1 = d1, sd2 = d2, sd3 = d3, sa0 = a0, sa1 = a1;

        uint32_t uVar9 = 0xffffffffu >> (d0 & 0x3f);
        uint32_t uVar4 = ~uVar9;
        uint32_t uVar7 = (0x10 - d0) & 0xffff;
        uint32_t uVar10 = 1;
        int bVar22 = 1;
        uint32_t uVar8 = (d2 - uVar7) & 0xffff;
        if (uVar7 <= d2 && uVar8 != 0) {
            for (;;) {
                uVar10 = (uVar10 + 1) & 0xffff;
                bVar22 ^= 1;
                int bVar21 = (0xf < uVar8);
                uVar8 = (uVar8 - 0x10) & 0xffff;
                if (!(bVar21 && uVar8 != 0))
                    break;
            }
        }
        uint32_t uVar11 = (~(0xffffffffu << ((uint32_t)(-(int32_t)uVar8) & 0x3f))) &
                          0xffffffffu;
        uVar7 = (uVar8 + 0x1f) & 0xffff;
        uVar10 = (uVar10 >> 1) & 0xffff;
        if (bVar22) {
            uVar10 = (uVar10 + 1) & 0xffff;
            uVar7 = (uVar8 + 0xf) & 0xffff;
            uVar11 = (uVar11 << 16) | (uVar11 >> 16) | 0xffff;
        }
        uint32_t rot6 = (3 - (d1 & 3)) & 0xffff;
        uint32_t rotm = uVar7;
        uint32_t rows = d3;
        uint32_t wa0 = a0, wa1 = a1, wa2 = A_SAVE_DESC;
        uint32_t case_d4;
        bVar22 = 0;

        if (uVar10 == 4) {
            case_d4 = 0xf;
            for (uint32_t r = rows;; r = (r - 1) & 0xffff) {
                uint32_t a0r = wa0, a2r = wa2;
                uint32_t hi = (d6 >> 16) & 0xffff;
                uint32_t u14 = ((be16(img + wa1) << 16) | hi) & 0xffffffffu;
                u14 = rotate_right32(u14, rotm);
                d6 = rotate_left32(u14, rot6);
                uint32_t dmask = rotate_right32(0x80000000u, rotm);
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t w0 = be16(img + a0r), w4 = be16(img + a0r + 8);
                    uint32_t w8 = be16(img + a0r + 0x10), wc = be16(img + a0r + 0x18);
                    uint32_t w10 = be16(img + a0r + 0x20), w14 = be16(img + a0r + 0x28);
                    uint32_t w18 = be16(img + a0r + 0x30), w1c = be16(img + a0r + 0x38);
                    uint32_t u5v = (w0 << 16) | w4;
                    uint32_t u6v = (w18 << 16) | w1c;
                    uint32_t u2v = (uint32_t)bVar22;
                    uint32_t u1v = (((w10 << 16) | w14) << 1);
                    uint32_t u3v = (((w8 << 16) | wc) << 1);
                    bVar22 = (w0 & 0x8000) != 0;
                    wr16(img + a2r + 0x18, (uint16_t)(u3v | ((w10 & 0x8000) != 0)));
                    wr16(img + a2r + 0x10, (uint16_t)(u3v >> 16));
                    wr16(img + a2r + 0x28, (uint16_t)(u1v | ((w18 & 0x8000) != 0)));
                    wr16(img + a2r + 0x20, (uint16_t)(u1v >> 16));
                    u1v = (uVar9 & ((u5v << 1) | ((w8 & 0x8000) != 0))) | (uVar4 & u5v);
                    wr16(img + a2r + 8, (uint16_t)u1v);
                    wr16(img + a2r, (uint16_t)(u1v >> 16));
                    u1v = (dmask & d6) |
                          ((~dmask) & ((~uVar11 & ((u6v << 1) | u2v)) | (uVar11 & u6v)));
                    wr16(img + a2r + 0x38, (uint16_t)u1v);
                    wr16(img + a2r + 0x30, (uint16_t)(u1v >> 16));
                    d6 = rotate_left32(d6, 4);
                    a0r += 2; a2r += 2;
                    if (k == 1) break;
                }
                wa0 += 0xa0;
                wa1 += be32(img + A_DAT_1A534);
                wa2 += 0x40;
                if (r == 0) break;
            }
        } else if (uVar10 == 3) {
            case_d4 = 0xb;
            for (uint32_t r = rows;; r = (r - 1) & 0xffff) {
                uint32_t a0r = wa0, a2r = wa2;
                uint32_t hi = (d6 >> 16) & 0xffff;
                uint32_t u14 = ((be16(img + wa1) << 16) | hi) & 0xffffffffu;
                u14 = rotate_right32(u14, rotm);
                d6 = rotate_left32(u14, rot6);
                uint32_t dmask = rotate_right32(0x80000000u, rotm);
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t w0 = be16(img + a0r), w4 = be16(img + a0r + 8);
                    uint32_t w8 = be16(img + a0r + 0x10), wc = be16(img + a0r + 0x18);
                    uint32_t w10 = be16(img + a0r + 0x20), w14 = be16(img + a0r + 0x28);
                    uint32_t u2v = (w0 << 16) | w4;
                    uint32_t u5v = (w10 << 16) | w14;
                    uint32_t u3v = (uint32_t)bVar22;
                    uint32_t u1v = (((w8 << 16) | wc) << 1);
                    bVar22 = (w0 & 0x8000) != 0;
                    wr16(img + a2r + 0x18, (uint16_t)(u1v | ((w10 & 0x8000) != 0)));
                    wr16(img + a2r + 0x10, (uint16_t)(u1v >> 16));
                    u1v = (uVar9 & ((u2v << 1) | ((w8 & 0x8000) != 0))) | (uVar4 & u2v);
                    wr16(img + a2r + 8, (uint16_t)u1v);
                    wr16(img + a2r, (uint16_t)(u1v >> 16));
                    u1v = (dmask & d6) |
                          ((~dmask) & ((~uVar11 & ((u5v << 1) | u3v)) | (uVar11 & u5v)));
                    wr16(img + a2r + 0x28, (uint16_t)u1v);
                    wr16(img + a2r + 0x20, (uint16_t)(u1v >> 16));
                    d6 = rotate_left32(d6, 4);
                    a0r += 2; a2r += 2;
                    if (k == 1) break;
                }
                wa0 += 0xa0;
                wa1 += be32(img + A_DAT_1A534);
                wa2 += 0x30;
                if (r == 0) break;
            }
        } else if (uVar10 == 2) {
            case_d4 = 7;
            int bVar21 = 0;
            for (uint32_t r = rows;; r = (r - 1) & 0xffff) {
                uint32_t a0r = wa0, a2r = wa2;
                uint32_t hi = (d6 >> 16) & 0xffff;
                uint32_t u14 = ((be16(img + wa1) << 16) | hi) & 0xffffffffu;
                u14 = rotate_right32(u14, rotm);
                d6 = rotate_left32(u14, rot6);
                uint32_t dmask = rotate_right32(0x80000000u, rotm);
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t w0 = be16(img + a0r), w4 = be16(img + a0r + 8);
                    uint32_t w8 = be16(img + a0r + 0x10), wc = be16(img + a0r + 0x18);
                    uint32_t u1v = (w0 << 16) | w4;
                    uint32_t u2v = (w8 << 16) | wc;
                    uint32_t u3v = (uint32_t)bVar21;
                    bVar21 = (w0 & 0x8000) != 0;
                    u1v = (uVar9 & ((u1v << 1) | ((w8 & 0x8000) != 0))) | (uVar4 & u1v);
                    wr16(img + a2r + 8, (uint16_t)u1v);
                    wr16(img + a2r, (uint16_t)(u1v >> 16));
                    u1v = (dmask & d6) |
                          ((~dmask) & ((~uVar11 & ((u2v << 1) | u3v)) | (uVar11 & u2v)));
                    wr16(img + a2r + 0x18, (uint16_t)u1v);
                    wr16(img + a2r + 0x10, (uint16_t)(u1v >> 16));
                    d6 = rotate_left32(d6, 4);
                    a0r += 2; a2r += 2;
                    if (k == 1) break;
                }
                wa0 += 0xa0;
                wa1 += be32(img + A_DAT_1A534);
                wa2 += 0x20;
                if (r == 0) break;
            }
        } else {
            case_d4 = 3;
            int bVar21 = 1;
            for (uint32_t r = rows;; r = (r - 1) & 0xffff) {
                uint32_t a0r = wa0, a2r = wa2;
                uint32_t hi = (d6 >> 16) & 0xffff;
                uint32_t u14 = ((be16(img + wa1) << 16) | hi) & 0xffffffffu;
                u14 = rotate_right32(u14, rotm);
                d6 = rotate_left32(u14, rot6);
                uint32_t dmask = rotate_right32(0x80000000u, rotm);
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t w0 = be16(img + a0r), w4 = be16(img + a0r + 8);
                    uint32_t u14b = (w0 << 16) | w4;
                    bVar22 = (w0 & 0x8000) != 0;
                    u14b = (dmask & d6) |
                           ((~dmask) & ((~(uVar11 | uVar4) & ((u14b << 1) | (uint32_t)bVar21)) |
                                        ((uVar11 | uVar4) & u14b)));
                    wr16(img + a2r + 8, (uint16_t)u14b);
                    wr16(img + a2r, (uint16_t)(u14b >> 16));
                    d6 = rotate_left32(d6, 4);
                    a0r += 2; a2r += 2;
                    bVar21 = bVar22;
                    if (k == 1) break;
                }
                wa0 += 0xa0;
                wa1 += be32(img + A_DAT_1A534);
                wa2 += 0x10;
                if (r == 0) break;
            }
        }
        (void)wa0; (void)wa2;

        wait_vbl(img);
        copy_scratch_rows(img, sa0, case_d4, sd3);
        if ((sd1 & 3) == 0)
            a1 = sa1 + 2;
        else
            a1 = sa1;
        d2 = (sd2 + 1) & 0xffff;
        if (sd0 != 0) {
            d0 = (sd0 - 1) & 0xffff;
            a0 = sa0;
        } else {
            d0 = 0xf;
            a0 = sa0 - 8;
        }
        d1 = (sd1 - 1) & 0xffff;
        d3 = sd3;
        if (d1 == 0xffff)
            break;
    }
    overlay_blit_regs[0] = d0; overlay_blit_regs[1] = d1; overlay_blit_regs[2] = d2;
    overlay_blit_regs[3] = d3; overlay_blit_regs[4] = a0; overlay_blit_regs[5] = a1;
}

void overlay_compose_planes32_wrap(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3,
                 uint32_t d6, uint32_t a0, uint32_t a1) {
    (void)d2;
    overlay_compose_planes32(img, d0, (d1 << 2) - 1, 5, d3 - 1, d6, a0, a1);
}

/* FUN_000117fe @ 0x117fe: compone dos fuentes (A0 = mascara, A1 = dato) en 0xd8f0
 * con la mascara ror32(0x80000000, D0), lo aplica con 0x123a0 y repite. El flag
 * 0x1a53e (lo fija el envoltorio) se resta a D2 cada pasada. FUN_000117de fija
 * 0x1a53e=1 y FUN_000117e6 =0; ambas preparan D1/D2/D3 y A1 = A0-0x8000. */
void overlay_compose_masked(uint8_t *img, uint32_t d0in, uint32_t d1in, uint32_t d2in,
                 uint32_t d3in, uint32_t a0in, uint32_t a1in) {
    uint32_t d0 = d0in & 0xffff, d1 = d1in & 0xffff, d2 = d2in & 0xffff;
    uint32_t d3 = d3in & 0xffff, a0 = a0in, a1 = a1in;

    for (;;) {
        uint32_t sd0 = d0, sd1 = d1, sd2 = d2, sd3 = d3, sa0 = a0, sa1 = a1;
        uint32_t uVar15 = rotate_right32(0x80000000u, d0);
        uint32_t uVar11 = 0xffffffffu >> (d0 & 0x3f);
        uint32_t uVar8 = ~(uVar11 >> 1);
        uint32_t uVar12 = 1;
        int bVar24 = 1;
        uint32_t uVar5 = (0x10 - d0) & 0xffff;
        uint32_t uVar10 = (d2 - uVar5) & 0xffff;
        if (uVar5 <= d2 && uVar10 != 0) {
            for (;;) {
                uVar12 = (uVar12 + 1) & 0xffff;
                bVar24 ^= 1;
                int b23 = (0xf < uVar10);
                uVar10 = (uVar10 - 0x10) & 0xffff;
                if (!(b23 && uVar10 != 0))
                    break;
            }
        }
        uint32_t uVar13 = (~(0xffffffffu << ((uint32_t)(-(int32_t)uVar10) & 0x3f))) &
                          0xffffffffu;
        uVar12 = (uVar12 >> 1) & 0xffff;
        if (bVar24) {
            uVar12 = (uVar12 + 1) & 0xffff;
            uVar13 = (uVar13 << 16) | (uVar13 >> 16) | 0xffff;
        }
        uint32_t rows = d3;
        uint32_t wa0 = a0, wa1 = a1, wa2 = A_SAVE_DESC;
        uint32_t case_d4;

        if (uVar12 == 2) {
            case_d4 = 7;
            for (uint32_t r = rows;; r = (r - 1) & 0xffff) {
                uint32_t a0r = wa0, a1r = wa1, a2r = wa2;
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t w4 = be16(img + a0r + 8);
                    uint32_t u6 = ((uint32_t)be16(img + a0r) << 16) | w4;
                    uint32_t w10 = be16(img + a0r + 0x10);
                    uint32_t w18 = be16(img + a0r + 0x18);
                    uint32_t a1v = ((uint32_t)be16(img + a1r) << 16) | be16(img + a1r + 8);
                    u6 = (~uVar15 & (((u6 & uVar11) >> 1) | (uVar8 & u6))) | (uVar15 & a1v);
                    wr16(img + a2r + 8, (uint16_t)u6);
                    wr16(img + a2r, (uint16_t)(u6 >> 16));
                    u6 = (~uVar13 & (((w4 & 1) << 31) | (((w10 << 16) | w18) >> 1))) |
                         (uVar13 & ((w10 << 16) | w18));
                    wr16(img + a2r + 0x18, (uint16_t)u6);
                    wr16(img + a2r + 0x10, (uint16_t)(u6 >> 16));
                    a0r += 2; a1r += 2; a2r += 2;
                    if (k == 1) break;
                }
                wa0 += 0xa0; wa1 += 0xa0; wa2 += 0x20;
                if (r == 0) break;
            }
        } else if (uVar12 == 3) {
            case_d4 = 0xb;
            for (uint32_t r = rows;; r = (r - 1) & 0xffff) {
                uint32_t a0r = wa0, a1r = wa1, a2r = wa2;
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t w4 = be16(img + a0r + 8);
                    uint32_t u7 = ((uint32_t)be16(img + a0r) << 16) | w4;
                    uint32_t w10 = be16(img + a0r + 0x10);
                    uint32_t w18 = be16(img + a0r + 0x18);
                    uint32_t w20 = be16(img + a0r + 0x20);
                    uint32_t w28 = be16(img + a0r + 0x28);
                    uint32_t a1v = ((uint32_t)be16(img + a1r) << 16) | be16(img + a1r + 8);
                    uint32_t u6 = ((((uint32_t)w10 << 16) | w18) >> 1);
                    wr16(img + a2r + 0x18, (uint16_t)u6);
                    wr16(img + a2r + 0x10, (uint16_t)((((w4 & 1) << 31) | u6) >> 16));
                    u6 = (~uVar15 & (((uVar11 & u7) >> 1) | (uVar8 & u7))) | (uVar15 & a1v);
                    wr16(img + a2r + 8, (uint16_t)u6);
                    wr16(img + a2r, (uint16_t)(u6 >> 16));
                    u6 = (~uVar13 & (((w18 & 1) << 31) | (((w20 << 16) | w28) >> 1))) |
                         (uVar13 & ((w20 << 16) | w28));
                    wr16(img + a2r + 0x28, (uint16_t)u6);
                    wr16(img + a2r + 0x20, (uint16_t)(u6 >> 16));
                    a0r += 2; a1r += 2; a2r += 2;
                    if (k == 1) break;
                }
                wa0 += 0xa0; wa1 += 0xa0; wa2 += 0x30;
                if (r == 0) break;
            }
        } else if (uVar12 == 4) {
            case_d4 = 0xf;
            for (uint32_t r = rows;; r = (r - 1) & 0xffff) {
                uint32_t a0r = wa0, a1r = wa1, a2r = wa2;
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t w4 = be16(img + a0r + 8);
                    uint32_t u9 = ((uint32_t)be16(img + a0r) << 16) | w4;
                    uint32_t w10 = be16(img + a0r + 0x10);
                    uint32_t w18 = be16(img + a0r + 0x18);
                    uint32_t w20 = be16(img + a0r + 0x20);
                    uint32_t w28 = be16(img + a0r + 0x28);
                    uint32_t w30 = be16(img + a0r + 0x30);
                    uint32_t w38 = be16(img + a0r + 0x38);
                    uint32_t a1v = ((uint32_t)be16(img + a1r) << 16) | be16(img + a1r + 8);
                    uint32_t u6 = ((((uint32_t)w10 << 16) | w18) >> 1);
                    uint32_t u7 = ((((uint32_t)w20 << 16) | w28) >> 1);
                    wr16(img + a2r + 0x18, (uint16_t)u6);
                    wr16(img + a2r + 0x10, (uint16_t)((((w4 & 1) << 31) | u6) >> 16));
                    wr16(img + a2r + 0x28, (uint16_t)u7);
                    wr16(img + a2r + 0x20, (uint16_t)((((w18 & 1) << 31) | u7) >> 16));
                    u6 = (~uVar15 & (((uVar11 & u9) >> 1) | (uVar8 & u9))) | (uVar15 & a1v);
                    wr16(img + a2r + 8, (uint16_t)u6);
                    wr16(img + a2r, (uint16_t)(u6 >> 16));
                    u6 = (~uVar13 & (((w28 & 1) << 31) | (((w30 << 16) | w38) >> 1))) |
                         (uVar13 & ((w30 << 16) | w38));
                    wr16(img + a2r + 0x38, (uint16_t)u6);
                    wr16(img + a2r + 0x30, (uint16_t)(u6 >> 16));
                    a0r += 2; a1r += 2; a2r += 2;
                    if (k == 1) break;
                }
                wa0 += 0xa0; wa1 += 0xa0; wa2 += 0x40;
                if (r == 0) break;
            }
        } else {
            case_d4 = 3;
            int bVar23 = 1;
            for (uint32_t r = rows;; r = (r - 1) & 0xffff) {
                uint32_t a0r = wa0, a1r = wa1, a2r = wa2;
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t w0 = be16(img + a0r);
                    uint32_t w4 = be16(img + a0r + 8);
                    uint32_t a1v = ((uint32_t)be16(img + a1r) << 16) | be16(img + a1r + 8);
                    uint32_t u11b = (w0 << 16) | w4;
                    bVar24 = (w4 & 1) != 0;
                    u11b = (~uVar15 & ((~(uVar13 | uVar8) &
                                        (((uint32_t)bVar23 << 31) | (u11b >> 1))) |
                                       ((uVar13 | uVar8) & u11b))) |
                           (uVar15 & a1v);
                    wr16(img + a2r + 8, (uint16_t)u11b);
                    wr16(img + a2r, (uint16_t)(u11b >> 16));
                    a0r += 2; a1r += 2; a2r += 2;
                    bVar23 = bVar24;
                    if (k == 1) break;
                }
                wa0 += 0xa0; wa1 += 0xa0; wa2 += 0x10;
                if (r == 0) break;
            }
        }
        (void)wa0; (void)wa2;

        wait_vbl(img);
        copy_scratch_rows(img, sa0, case_d4, sd3);
        d2 = (sd2 - be16(img + 0x1a53e)) & 0xffff;
        uint32_t nd0 = (sd0 + 1) & 0xf;
        if (nd0 == 0) {
            a1 = sa1 + 8;
            a0 = sa0 + 8;
        } else {
            a1 = sa1;
            a0 = sa0;
        }
        d0 = nd0;
        d1 = (sd1 - 1) & 0xffff;
        d3 = sd3;
        if (d1 == 0xffff)
            break;
    }
    overlay_blit_regs[0] = d0; overlay_blit_regs[1] = d1; overlay_blit_regs[2] = d2;
    overlay_blit_regs[3] = d3; overlay_blit_regs[4] = a0; overlay_blit_regs[5] = a1;
}

void overlay_compose_masked_wrap(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3,
                 uint32_t a0) {
    wr16(img + 0x1a53e, 1);
    overlay_compose_masked(img, d0, (d1 << 2) - 1, d2 << 2, d3 - 1, a0, a0 - 0x8000);
}

void overlay_compose_shift(uint8_t *img, uint32_t d0in, uint32_t d1in, uint32_t d2in,
                 uint32_t d3in, uint32_t a0in, uint32_t a1in) {
    uint32_t d0 = d0in & 0xffff, d1 = d1in & 0xffff, d2 = d2in & 0xffff;
    uint32_t d3 = d3in & 0xffff, a0 = a0in, a1 = a1in;
    uint32_t flag = be16(img + 0x1a53e);

    for (;;) {
        uint32_t sd3 = d3;
        uint32_t uVar10 = 0xffffffffu >> (d0 & 0x3f);
        uint32_t uVar7 = ~uVar10;
        uint32_t uVar11 = 1;
        int bVar23 = 1;
        uint32_t uVar4 = (0x10 - d0) & 0xffff;
        uint32_t uVar9 = (d2 - uVar4) & 0xffff;
        if (uVar4 <= d2 && uVar9 != 0) {
            for (;;) {
                uVar11 = (uVar11 + 1) & 0xffff;
                bVar23 ^= 1;
                int bVar22 = (0xf < uVar9);
                uVar9 = (uVar9 - 0x10) & 0xffff;
                if (!(bVar22 && uVar9 != 0))
                    break;
            }
        }
        uint32_t q = (0 - uVar9) & 0xffff;
        uint32_t uVar12 = ~(0xffffffffu << (q & 0x3f));
        uint32_t uVar4b = (uVar9 + 0x1f) & 0xffff;
        uVar11 = (uVar11 >> 1) & 0xffff;
        if (bVar23) {
            uVar11 = (uVar11 + 1) & 0xffff;
            uVar4b = (uVar9 + 0xf) & 0xffff;
            uVar12 = ((uVar12 << 16) | (uVar12 >> 16)) | 0xffff;
        }
        uint32_t uVar14 = rotate_right32(0x80000000u, uVar4b & 0x3f);
        uint32_t rows = sd3;
        uint32_t wa0 = a0, wa1 = a1, wa2 = A_SAVE_DESC;
        uint32_t case_d4;

        if (uVar11 == 2) {
            case_d4 = 7;
            uint32_t fill = 0;
            for (uint32_t r = rows;; r = (r - 1) & 0xffff) {
                uint32_t a0r = wa0, a1r = wa1, a2r = wa2;
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t d1o = ((uint32_t)be16(img + a0r) << 16) | be16(img + a0r + 8);
                    uint32_t d3o = ((uint32_t)be16(img + a0r + 0x10) << 16) | be16(img + a0r + 0x18);
                    uint32_t d0o = ((uint32_t)be16(img + a1r + 0x10) << 16) | be16(img + a1r + 0x18);
                    uint32_t d3s = (d3o << 1) | fill;
                    uint32_t d1s = (d1o << 1) | (d3o >> 31);
                    fill = d1o >> 31;
                    uint32_t u1 = (d1o & uVar7) | (d1s & uVar10);
                    uint32_t u2 = (d3o & uVar12) | (d3s & ~uVar12);
                    uint32_t u3 = (d0o & uVar14) | (u2 & ~uVar14);
                    wr16(img + a2r + 8, (uint16_t)u1);
                    wr16(img + a2r, (uint16_t)(u1 >> 16));
                    wr16(img + a2r + 0x18, (uint16_t)u3);
                    wr16(img + a2r + 0x10, (uint16_t)(u3 >> 16));
                    a0r += 2; a1r += 2; a2r += 2;
                    if (k == 1) break;
                }
                wa0 += 0xa0; wa1 += 0xa0; wa2 += 0x20;
                if (r == 0) break;
            }
        } else if (uVar11 == 3) {
            case_d4 = 0xb;
            for (uint32_t r = rows;; r = (r - 1) & 0xffff) {
                uint32_t a0r = wa0, a1r = wa1, a2r = wa2;
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t d0o = ((uint32_t)be16(img + a1r + 0x20) << 16) | be16(img + a1r + 0x28);
                    uint32_t d1o = ((uint32_t)be16(img + a0r) << 16) | be16(img + a0r + 8);
                    uint32_t d2o = ((uint32_t)be16(img + a0r + 0x10) << 16) | be16(img + a0r + 0x18);
                    uint32_t d3o = ((uint32_t)be16(img + a0r + 0x20) << 16) | be16(img + a0r + 0x28);
                    uint32_t d3s = d3o << 1;
                    uint32_t d2s = (d2o << 1) | (d3o >> 31);
                    uint32_t d1s = (d1o << 1) | (d2o >> 31);
                    uint32_t u1 = (d1o & uVar7) | (d1s & uVar10);
                    uint32_t u2 = (d3o & uVar12) | (d3s & ~uVar12);
                    uint32_t u3 = (d0o & uVar14) | (u2 & ~uVar14);
                    wr16(img + a2r + 0x18, (uint16_t)d2s);
                    wr16(img + a2r + 0x10, (uint16_t)(d2s >> 16));
                    wr16(img + a2r + 8, (uint16_t)u1);
                    wr16(img + a2r, (uint16_t)(u1 >> 16));
                    wr16(img + a2r + 0x28, (uint16_t)u3);
                    wr16(img + a2r + 0x20, (uint16_t)(u3 >> 16));
                    a0r += 2; a1r += 2; a2r += 2;
                    if (k == 1) break;
                }
                wa0 += 0xa0; wa1 += 0xa0; wa2 += 0x30;
                if (r == 0) break;
            }
        } else if (uVar11 == 4) {
            case_d4 = 0xf;
            for (uint32_t r = rows;; r = (r - 1) & 0xffff) {
                uint32_t a0r = wa0, a1r = wa1, a2r = wa2;
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t d0o = ((uint32_t)be16(img + a1r + 0x30) << 16) | be16(img + a1r + 0x38);
                    uint32_t d1o = ((uint32_t)be16(img + a0r) << 16) | be16(img + a0r + 8);
                    uint32_t d2o = ((uint32_t)be16(img + a0r + 0x10) << 16) | be16(img + a0r + 0x18);
                    uint32_t d4o = ((uint32_t)be16(img + a0r + 0x20) << 16) | be16(img + a0r + 0x28);
                    uint32_t d3o = ((uint32_t)be16(img + a0r + 0x30) << 16) | be16(img + a0r + 0x38);
                    uint32_t d3s = d3o << 1;
                    uint32_t d4s = (d4o << 1) | (d3o >> 31);
                    uint32_t d2s = (d2o << 1) | (d4o >> 31);
                    uint32_t d1s = (d1o << 1) | (d2o >> 31);
                    uint32_t u1 = (d1o & uVar7) | (d1s & uVar10);
                    uint32_t u2 = (d3o & uVar12) | (d3s & ~uVar12);
                    uint32_t u3 = (d0o & uVar14) | (u2 & ~uVar14);
                    wr16(img + a2r + 0x18, (uint16_t)d2s);
                    wr16(img + a2r + 0x10, (uint16_t)(d2s >> 16));
                    wr16(img + a2r + 0x28, (uint16_t)d4s);
                    wr16(img + a2r + 0x20, (uint16_t)(d4s >> 16));
                    wr16(img + a2r + 8, (uint16_t)u1);
                    wr16(img + a2r, (uint16_t)(u1 >> 16));
                    wr16(img + a2r + 0x38, (uint16_t)u3);
                    wr16(img + a2r + 0x30, (uint16_t)(u3 >> 16));
                    a0r += 2; a1r += 2; a2r += 2;
                    if (k == 1) break;
                }
                wa0 += 0xa0; wa1 += 0xa0; wa2 += 0x40;
                if (r == 0) break;
            }
        } else {
            case_d4 = 3;
            uint32_t fill = 0;
            for (uint32_t r = rows;; r = (r - 1) & 0xffff) {
                uint32_t a0r = wa0, a1r = wa1, a2r = wa2;
                for (uint32_t k = 4;; k = (k - 1) & 0xffff) {
                    uint32_t d1o = ((uint32_t)be16(img + a0r) << 16) | be16(img + a0r + 8);
                    uint32_t d0o = ((uint32_t)be16(img + a1r) << 16) | be16(img + a1r + 8);
                    uint32_t d1s = (d1o << 1) | fill;
                    fill = d1o >> 31;
                    uint32_t u1 = (d1o & (uVar7 | uVar12)) | (d1s & ~(uVar7 | uVar12));
                    uint32_t u2 = (d0o & uVar14) | (u1 & ~uVar14);
                    wr16(img + a2r + 8, (uint16_t)u2);
                    wr16(img + a2r, (uint16_t)(u2 >> 16));
                    a0r += 2; a1r += 2; a2r += 2;
                    if (k == 1) break;
                }
                wa0 += 0xa0; wa1 += 0xa0; wa2 += 0x10;
                if (r == 0) break;
            }
        }
        (void)wa0; (void)wa2;

        wait_vbl(img);
        copy_scratch_rows(img, a0, case_d4, sd3);
        d2 = (d2 - flag) & 0xffff;
        if (flag == 0) {
            uint32_t before = d0;
            d0 = (d0 - 1) & 0xffff;
            if (before == 0) {
                a1 = (a1 - 8) & 0xffffffff;
                a0 = (a0 - 8) & 0xffffffff;
                d0 = 0xf;
            }
        }
        if (d1 == 0)
            break;
        d1 = (d1 - 1) & 0xffff;
    }
    overlay_blit_regs[0] = d0; overlay_blit_regs[1] = 0xffff; overlay_blit_regs[2] = d2;
    overlay_blit_regs[3] = d3; overlay_blit_regs[4] = a0; overlay_blit_regs[5] = a1;
}

void overlay_compose_shift_wrap(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3,
                 uint32_t a0) {
    wr16(img + 0x1a53e, 1);
    overlay_compose_shift(img, d0, (d1 << 2) - 1, d2 << 2, d3 - 1, a0, a0 - 0x8000);
}
