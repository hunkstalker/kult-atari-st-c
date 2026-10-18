/* scale.c — escaladores de elemento (0x13a18/0x13bc2) y sus preparaciones.
 *
 * FUN_00013a18 @ 0x13a18: construye el elemento escalado en el descriptor 0xd8f0
 * (filas empaquetadas) y lo blitea a A2 (pantalla) con 0x123a0. La posicion de
 * origen la lleva un acumulador 16.16 (0x1a48c) alimentado por 0x1a490/0x1a492;
 * 0x1a494/0x1a498/0x1a49a hacen lo propio con la fila. Cada columna expande un bit
 * del bitmap (A0) en 4 planos del descriptor con `roxl`.
 *
 * FUN_00013bc2 @ 0x13bc2: el mismo bucle pero escribiendo en A1 (buffer del
 * llamante, filas a 0xa0) y sin el blit final.
 */
#include <stdint.h>

#include "machine.h"
#include "addrs.h"
#include "kult_internal.h"

void copy_scratch_rows(uint8_t *img, uint32_t a0, uint32_t d4, uint32_t d5);

/* Un paso de la expansion roxl: word = (word << 1) | x; devuelve el nuevo X. */
static uint32_t roxl1(uint8_t *img, uint32_t a, uint32_t off, uint32_t x) {
    uint32_t v = be16(img + a + off);
    uint32_t n = (uint32_t)((uint16_t)((v << 1) | (x & 1)));
    wr16(img + a + off, (uint16_t)n);
    return (v >> 15) & 1;
}

/* Avance 16.16 de (lo,hi) con las sumas de 0x1a490/0x1a492: el original hace
 * `add.w` (a la mitad baja), `swap`, `add.w` (a la otra mitad) y `swap`; por eso
 * la suma de la fraccion cae en la palabra BAJA y el acarreo sube a la alta. */
static uint32_t fx_advance(uint32_t acc, uint32_t step_lo, uint32_t step_hi) {
    uint32_t lo = acc & 0xffff, hi = acc >> 16;
    uint32_t nlo = (lo + step_lo) & 0xffff;
    uint32_t sum = hi + step_hi;
    uint32_t nhi = sum & 0xffff;
    uint32_t cy = (sum > 0xffff) ? 1 : 0;
    return (nhi << 16) | ((nlo + cy) & 0xffff);
}

/* Cabecera comun: fija 0x1a480/490/492/498/49a y 0x1a494 = 0. */
static void scale_setup(uint8_t *img, uint32_t d2, uint32_t d3, uint32_t *d2o,
                        uint32_t *d6o, uint32_t *d7o) {
    uint32_t d0 = be16(img + 0x1a542);
    *d7o = (d2 + 2) & 0xffff;
    d3 = (d3 + 1) & 0xffff;
    wr16(img + 0x1a480, d3);
    wr16(img + 0x1a490, (d0 / d3) & 0xffff);
    wr16(img + 0x1a492, (((d0 % d3) << 16) / d3) & 0xffff);
    d0 = be16(img + 0x1a544);
    d2 = (d2 + 1) & 0xffff;
    *d2o = d2;
    *d6o = d2;
    wr16(img + 0x1a498, (d0 / d2) & 0xffff);
    wr16(img + 0x1a49a, (((d0 % d2) << 16) / d2) & 0xffff);
    wr32(img + 0x1a494, 0);
}

/* 0x13a18 — scale 16 16 */
void scale_16_16(uint8_t *img, uint32_t d2in, uint32_t d3in, uint32_t d7,
                 uint32_t a2) {
    uint32_t d2, d6, d7start;
    scale_setup(img, d2in & 0xffff, d3in & 0xffff, &d2, &d6, &d7start);
    (void)d7;
    (void)d7start;
    uint32_t a1 = 0xd8f0;
    uint32_t a5 = a2;
    uint32_t row = 0;
    for (;;) {
        uint32_t a0 = be32(img + A_ELEM_1A528) +
                      2 * ((row & 0xffff) * be16(img + A_ELEM_1A52E));
        uint32_t d7c = be16(img + 0x1a480);
        uint32_t d5 = be16(img + 0x1a486);
        uint32_t a3 = a2;
        uint32_t d4 = (0x10 - d5) & 0xffff;

        uint32_t e0 = be32(img + a3), e1 = be32(img + a3 + 4);
        e0 >>= d4;
        e1 >>= d4;
        wr32(img + a1, e0);
        wr32(img + a1 + 4, e1);
        wr32(img + 0x1a48c, 0);
        wr16(img + 0x1a556, 0);

        uint32_t acc = 0;
        for (;;) {
            uint32_t w = (be16(img + a0 + ((acc & 0xfc) >> 1)) << (acc & 3)) & 0xffff;
            uint32_t s = (w << 1) & 0xffff;
            uint32_t x = (w >> 15) & 1;
            x = roxl1(img, a1, 0, x);
            x = (s >> 12) & 1;
            roxl1(img, a1, 2, x);
            s = (s << 4) & 0xffff;
            roxl1(img, a1, 4, (s >> 12) & 1);
            s = (s << 4) & 0xffff;
            roxl1(img, a1, 6, (s >> 12) & 1);

            d4 = (d4 - 1) & 0xffff;
            if (d4 == 0) {
                a1 += 8;
                a3 += 8;
                wr16(img + 0x1a556, (be16(img + 0x1a556) + 2) & 0xffff);
                d4 = 0x10;
            }

            acc = fx_advance(be32(img + 0x1a48c), be16(img + 0x1a490),
                             be16(img + 0x1a492));
            wr32(img + 0x1a48c, acc);
            if (d7c == 0)
                break;
            d7c = (d7c - 1) & 0xffff;
            if (d7c == 0)
                acc = be16(img + 0x1a542);
        }

        {
            uint32_t d0 = be32(img + a1), d1 = be32(img + a1 + 4);
            d0 <<= d4;
            d1 <<= d4;
            uint32_t d2m = (0xffffu << d4) & 0xffff;
            uint32_t mask = (d2m << 16) | d2m;
            d0 &= mask;
            d1 &= mask;
            mask = ~mask;
            d0 |= be32(img + a3) & mask;
            d1 |= be32(img + a3 + 4) & mask;
            wr32(img + a1, d0);
            wr32(img + a1 + 4, d1);
            a1 += 8;
            wr16(img + 0x1a556, (be16(img + 0x1a556) + 2) & 0xffff);
        }

        a2 += 0xa0;
        uint32_t vacc = fx_advance(be32(img + 0x1a494), be16(img + 0x1a498),
                                   be16(img + 0x1a49a));
        wr32(img + 0x1a494, vacc);
        if (d6 == 0)
            break;
        d6 = (d6 - 1) & 0xffff;
        row = (d6 != 0) ? (vacc & 0xffff) : be16(img + 0x1a544);
    }
    copy_scratch_rows(img, a5, (be16(img + 0x1a556) - 1) & 0xffff, d2);
}

/* 0x13bc2 — scale blit */
void scale_blit(uint8_t *img, uint32_t d2in, uint32_t d3in, uint32_t d7,
                 uint32_t a1in) {
    uint32_t d2, d6, d7start;
    scale_setup(img, d2in & 0xffff, d3in & 0xffff, &d2, &d6, &d7start);
    (void)d7;
    (void)d7start;
    uint32_t a1 = a1in;
    uint32_t row = 0;
    for (;;) {
        uint32_t a0 = be32(img + A_ELEM_1A528) +
                      2 * ((row & 0xffff) * be16(img + A_ELEM_1A52E));
        uint32_t d7c = be16(img + 0x1a480);
        uint32_t d5 = be16(img + 0x1a486);
        uint32_t a4 = a1;
        uint32_t d4 = (0x10 - d5) & 0xffff;

        wr32(img + a4, 0);
        wr32(img + a4 + 4, 0);
        wr32(img + 0x1a48c, 0);

        uint32_t acc = 0;
        for (;;) {
            uint32_t w = (be16(img + a0 + ((acc & 0xfc) >> 1)) << (acc & 3)) & 0xffff;
            uint32_t s = (w << 1) & 0xffff;
            uint32_t x = (w >> 15) & 1;
            x = roxl1(img, a4, 0, x);
            x = (s >> 12) & 1;
            roxl1(img, a4, 2, x);
            s = (s << 4) & 0xffff;
            roxl1(img, a4, 4, (s >> 12) & 1);
            s = (s << 4) & 0xffff;
            roxl1(img, a4, 6, (s >> 12) & 1);

            d4 = (d4 - 1) & 0xffff;
            if (d4 == 0) {
                a4 += 8;
                d4 = 0x10;
            }
            acc = fx_advance(be32(img + 0x1a48c), be16(img + 0x1a490),
                             be16(img + 0x1a492));
            wr32(img + 0x1a48c, acc);
            if (d7c == 0)
                break;
            d7c = (d7c - 1) & 0xffff;
            if (d7c == 0)
                acc = be16(img + 0x1a542);
        }

        {
            uint32_t d0 = be32(img + a4), d1 = be32(img + a4 + 4);
            d0 <<= d4;
            d1 <<= d4;
            wr32(img + a4, d0);
            wr32(img + a4 + 4, d1);
            a1 += 0xa0;
        }

        uint32_t vacc = fx_advance(be32(img + 0x1a494), be16(img + 0x1a498),
                                   be16(img + 0x1a49a));
        wr32(img + 0x1a494, vacc);
        if (d6 == 0)
            break;
        d6 = (d6 - 1) & 0xffff;
        row = (d6 != 0) ? (vacc & 0xffff) : be16(img + 0x1a544);
    }
}

/* FUN_00013702 @ 0x13702 (byte 16): 128a4 + setup de 0x1a544/0x1a542/0x1a486;
 * escribe [filas][cols] en la entrada de icono que 128a4 deja en A2 (+0xe) y llama
 * al escalador 0x13a18. Devuelve A6 + 2 (los dos bytes leidos). */
uint32_t elem_command_wrap(uint8_t *img, uint32_t a6);
uint32_t elem_blit_vbl(uint8_t *img, uint32_t a6);
uint32_t screen_address(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t *d7out);

/* 0x13702 — scale prep */
uint32_t scale_prep(uint8_t *img, uint32_t a6) {
    uint32_t slot = 0x1a850;
    for (uint32_t k = 9;; k = (k - 1) & 0xffff) {
        if (be16(img + slot) == 0)
            break;
        slot += 0x10;
        if (k == 0)
            break;
    }
    a6 = elem_command_wrap(img, a6);
    uint32_t d7;
    uint32_t a0 = screen_address(img, be16(img + A_ELEM_1A532),
                                     be16(img + A_ELEM_1A530), &d7);
    uint32_t d2 = be16(img + A_ELEM_1A52E);
    uint32_t d3 = be16(img + A_ELEM_1A52C);
    wr16(img + 0x1a544, (d3 - 1) & 0xffff);
    wr16(img + 0x1a542, ((d2 << 2) - 1) & 0xffff);
    wr16(img + 0x1a486, d7);
    uint32_t b3 = img[a6];
    uint32_t b2 = img[a6 + 1];
    uint32_t cursor = slot + 0xe;
    wr16(img + cursor, b2 & 0xffff);
    wr16(img + cursor - 2, b3 & 0xffff);
    b3 = (b3 << 2) & 0xffff;
    b2 = (b2 - 2) & 0xffff;
    b3 = (b3 - 2) & 0xffff;
    scale_16_16(img, b2, b3, d7, a0);
    return a6 + 2;
}

/* FUN_0001398e @ 0x1398e: construye en 0xd8f0 una lista de entradas
 * [word offset][word max(1,*0x1a1f2)][long movep(fila)] de 8 filas (stride 0xa0)
 * por grupo, avanzando A2 +1/+7 y A0 +0x500 por banda; cierra con un long 0. */
void scale_list_build(uint8_t *img, uint32_t d2in, uint32_t d3in, uint32_t d7, uint32_t a0) {
    uint32_t a4 = 0xd8f0;
    uint32_t a5 = 0x1a1f2;
    uint32_t d2 = d2in & 0xffff;
    (void)d3in;
    if (d7 & 8)
        a0 += 1;
    if (d7 & 4)
        d2 = (d2 + 1) & 0xffff;
    uint32_t count = d2 >> 1;
    if (!(d2 & 1))
        count = (count - 1) & 0xffff;
    for (;;) {
        uint32_t a2 = a0;
        uint32_t d6 = count;
        for (;;) {
            uint32_t a3 = a2;
            uint32_t off = (a3 - be32(img + A_SCREEN_PTR_A)) & 0xffff;
            wr16(img + a4, off & 0xffff);
            a4 += 2;
            uint32_t v = img[a5++];
            if (v == 0)
                v = 1;
            wr16(img + a4, v & 0xffff);
            a4 += 2;
            wr32(img + a4, movep_read_l(img, a3));
            a4 += 4;
            a3 += 0xa0;
            for (uint32_t i = 0; i < 7; i++) {
                wr32(img + a4, movep_read_l(img, a3));
                a4 += 4;
                a3 += 0xa0;
            }
            a2 += 1;
            if (!(a2 & 1))
                a2 += 6;
            if (d6 == 0)
                break;
            d6 = (d6 - 1) & 0xffff;
        }
        a0 += 0x500;
        if (count == 0)
            break;
        count = (count - 1) & 0xffff;
    }
    wr32(img + a4, 0);
}

/* FUN_000138a6 @ 0x138a6 (byte 34): 120e4 + 1398e construyen la lista en 0xd8f0 y
 * luego, pasada a pasada (con espera de VBL), escribe en pantalla los grupos cuyo
 * contador < pasada, desplazando el offset segun el contador; A6 += 2. */
void hint_select_off(uint8_t *img, uint32_t d0, uint32_t *o);
void hint_blit_entry(uint8_t *img, uint32_t a6, uint32_t *a6_out);
void wait_vbl(uint8_t *img);
void scale_list_build(uint8_t *img, uint32_t d2, uint32_t d3, uint32_t d7, uint32_t a0);
/* 0x138a6 — scale anim */
uint32_t scale_anim(uint8_t *img, uint32_t a6) {
    if (img[A_SKIP_TRANSITION] != 0) {
        uint32_t a6o;
        hint_blit_entry(img, a6, &a6o);
        return a6o;
    }
    uint32_t o[8];
    hint_select_off(img, img[a6 + 1], o);
    scale_list_build(img, o[5], o[6], o[2], o[0]);

    uint32_t d6 = 1;
    for (;;) {
        uint32_t d5 = 0;
        uint32_t a4 = 0xd8f0;
        for (;;) {
            uint32_t cnt = (be16(img + a4 + 2) >> 2) & 0xffff;
            if (cnt >= d6) {
                d5 = 1;
            } else {
                uint32_t a0 = be32(img + A_SCREEN_PTR_A) + be16(img + a4);
                uint32_t a1 = be32(img + A_SCREEN_PTR_B) + be16(img + a4);
                uint32_t a2 = a0, a3 = a1, a5 = a4 + 4;
                for (uint32_t k = 7;; k = (k - 1) & 0xffff) {
                    uint32_t d1 = movep_read_l(img, a2);
                    if (d1 == be32(img + a5)) {
                        d1 = movep_read_l(img, a3);
                        movep_write_l(img, a2, d1);
                    }
                    a2 += 0xa0;
                    a3 += 0xa0;
                    a5 += 4;
                    if (k == 0)
                        break;
                }
                uint32_t c2 = be16(img + a4 + 2);
                if (c2 != 0) {
                    a0 += (uint32_t)(((c2 & 7) + 1) * 0xa0);
                    a2 = a0;
                    uint32_t off = a0 - be32(img + A_SCREEN_PTR_A);
                    wr16(img + a4, off & 0xffff);
                    if (off >= 0x7800) {
                        wr16(img + a4 + 2, 0);
                    } else {
                        a5 = a4 + 4;
                        for (uint32_t k = 7;; k = (k - 1) & 0xffff) {
                            movep_write_l(img, a2, be32(img + a5));
                            a2 += 0xa0;
                            a5 += 4;
                            if (k == 0)
                                break;
                        }
                        d5 = 1;
                    }
                }
            }
            a4 += 0x24;
            if (be16(img + a4) == 0)
                break;
        }
        wait_vbl(img);
        d6 += 1;
        if (d5 == 0) {
            a6 += 2;
            return a6;
        }
    }
}

/* FUN_00013740 @ 0x13740 (byte 14): efecto de escalado animado. Tras 128a4 calcula
 * el origen (0x10fee) y las razones 1a54a/54c/552/554, y repite 13a18 con los
 * factores d2/d3 evolucionando por los acumuladores 1a546/1a54e, desplazando a2.
 * Termina con 124da sobre los registros originales del elemento. */
void blit_rect_vbl(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t d2, uint32_t d3,
                 uint32_t d7);
uint32_t screen_address_raw(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t *d7out);

/* 0x13740 — scale loop */
uint32_t scale_loop(uint8_t *img, uint32_t a6) {
    if (img[A_SKIP_TRANSITION] != 0)
        return elem_blit_vbl(img, a6);
    a6 = elem_command_wrap(img, a6);
    uint32_t x = be16(img + A_ELEM_1A532);
    uint32_t y = be16(img + A_ELEM_1A530);
    uint32_t w = be16(img + A_ELEM_1A52E);
    uint32_t h = be16(img + A_ELEM_1A52C);
    uint32_t a1e = be32(img + A_ELEM_1A528);
    uint32_t d7e;
    uint32_t elem_a0 = screen_address(img, x, y, &d7e);
    wr16(img + 0x1a544, (h - 1) & 0xffff);
    uint32_t d2i = (w << 1) & 0xffff;
    wr16(img + 0x1a542, ((d2i << 1) - 1) & 0xffff);
    uint32_t d0 = ((x << 2) + ((d2i - 1) & 0xffff)) & 0xffff;
    uint32_t d1 = (y + (h >> 1)) & 0xffff;
    uint32_t d7;
    uint32_t a0 = screen_address_raw(img, d0, d1, &d7);
    wr16(img + 0x1a486, d7);

    uint32_t d0b = (be16(img + A_ELEM_1A52E) << 2) & 0xffff;
    uint32_t d2b = d0b;
    uint32_t d4b = d0b;
    if (d0b < h)
        d4b = h;
    uint32_t d3b = d4b;
    d4b >>= 1;
    wr16(img + 0x1a54a, ((h << 1) / d3b) & 0xffff);
    wr16(img + 0x1a54c, ((((h << 1) % d3b) << 16) / d3b) & 0xffff);
    wr16(img + 0x1a552, ((d2b << 1) / d3b) & 0xffff);
    wr16(img + 0x1a554, (((((d2b << 1) % d3b)) << 16) / d3b) & 0xffff);
    wr32(img + 0x1a546, 2);
    wr32(img + 0x1a54e, 2);
    uint32_t d2 = 2, d3 = 2;
    uint32_t d4 = (d4b - 3) & 0xffff;
    uint32_t a2 = a0;
    for (;;) {
        scale_16_16(img, d2, d3, d7, a2);
        {
            uint32_t acc = be32(img + 0x1a546);
            uint32_t lo = acc & 0xffff, hi = acc >> 16;
            uint32_t nlo = (lo + be16(img + 0x1a54a)) & 0xffff;
            uint32_t sum = hi + be16(img + 0x1a54c);
            uint32_t cy = (sum > 0xffff) ? 1 : 0;
            wr32(img + 0x1a546, ((sum & 0xffff) << 16) | nlo);
            d2 = nlo;
            if (cy || !(nlo & 1))
                a2 -= 0xa0;
        }
        {
            uint32_t acc = be32(img + 0x1a54e);
            uint32_t lo = acc & 0xffff, hi = acc >> 16;
            uint32_t nlo = (lo + be16(img + 0x1a552)) & 0xffff;
            uint32_t sum = hi + be16(img + 0x1a554);
            uint32_t cy = (sum > 0xffff) ? 1 : 0;
            int adjust;
            if (cy) {
                wr32(img + 0x1a54e, ((sum & 0xffff) << 16) | ((nlo + 1) & 0xffff));
                d3 = (nlo + 1) & 0xffff;
                adjust = 1;
            } else {
                wr32(img + 0x1a54e, ((sum & 0xffff) << 16) | nlo);
                d3 = nlo;
                adjust = !(nlo & 1);
            }
            if (adjust) {
                uint32_t d0p = be16(img + 0x1a486);
                if (d0p == 0) {
                    a2 -= 8;
                    d0p = 0x10;
                }
                wr16(img + 0x1a486, (d0p - 1) & 0xffff);
            }
        }
        if (d4 == 0)
            break;
        d4 = (d4 - 1) & 0xffff;
    }
    blit_rect_vbl(img, elem_a0, a1e, w, h, d7e);
    return a6;
}
