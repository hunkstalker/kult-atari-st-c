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

void copy_scratch_rows(uint8_t *, uint32_t, uint32_t, uint32_t);

/* Un paso de la expansion roxl: word = (word << 1) | x; devuelve el nuevo X. */
static uint32_t roxl1(uint8_t *img, uint32_t base, uint32_t off, uint32_t x) {
    uint32_t v = be16(img + base + off);
    uint32_t n = (uint32_t)((uint16_t)((v << 1) | (x & 1)));
    wr16(img + base + off, (uint16_t)n);
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
static void scale_setup(uint8_t *img, uint32_t w, uint32_t h, uint32_t *w_out,
                        uint32_t *rows_out, uint32_t *phase_out) {
    uint32_t val = be16(img + 0x1a542);
    *phase_out = (w + 2) & 0xffff;
    h = (h + 1) & 0xffff;
    wr16(img + 0x1a480, h);
    wr16(img + 0x1a490, (val / h) & 0xffff);
    wr16(img + 0x1a492, (((val % h) << 16) / h) & 0xffff);
    val = be16(img + 0x1a544);
    w = (w + 1) & 0xffff;
    *w_out = w;
    *rows_out = w;
    wr16(img + 0x1a498, (val / w) & 0xffff);
    wr16(img + 0x1a49a, (((val % w) << 16) / w) & 0xffff);
    wr32(img + 0x1a494, 0);
}

/* 0x13a18 — scale 16 16 */
void scale_16_16(uint8_t *img, uint32_t w_in, uint32_t h_in, uint32_t phase,
                 uint32_t dest) {
    uint32_t word, rows, phase_start;
    scale_setup(img, w_in & 0xffff, h_in & 0xffff, &word, &rows, &phase_start);
    (void)phase;
    (void)phase_start;
    uint32_t desc = 0xd8f0;
    uint32_t dest2 = dest;
    uint32_t row = 0;
    for (;;) {
        uint32_t src = be32(img + A_ELEM_1A528) +
                      2 * ((row & 0xffff) * be16(img + A_ELEM_1A52E));
        uint32_t cols = be16(img + 0x1a480);
        uint32_t shift = be16(img + 0x1a486);
        uint32_t src2 = dest;
        uint32_t bits = (0x10 - shift) & 0xffff;

        uint32_t e0 = be32(img + src2), e1 = be32(img + src2 + 4);
        e0 >>= bits;
        e1 >>= bits;
        wr32(img + desc, e0);
        wr32(img + desc + 4, e1);
        wr32(img + 0x1a48c, 0);
        wr16(img + 0x1a556, 0);

        uint32_t acc = 0;
        for (;;) {
            uint32_t word = (be16(img + src + ((acc & 0xfc) >> 1)) << (acc & 3)) & 0xffff;
            uint32_t s = (word << 1) & 0xffff;
            uint32_t x = (word >> 15) & 1;
            x = roxl1(img, desc, 0, x);
            x = (s >> 12) & 1;
            roxl1(img, desc, 2, x);
            s = (s << 4) & 0xffff;
            roxl1(img, desc, 4, (s >> 12) & 1);
            s = (s << 4) & 0xffff;
            roxl1(img, desc, 6, (s >> 12) & 1);

            bits = (bits - 1) & 0xffff;
            if (bits == 0) {
                desc += 8;
                src2 += 8;
                wr16(img + 0x1a556, (be16(img + 0x1a556) + 2) & 0xffff);
                bits = 0x10;
            }

            acc = fx_advance(be32(img + 0x1a48c), be16(img + 0x1a490),
                             be16(img + 0x1a492));
            wr32(img + 0x1a48c, acc);
            if (cols == 0)
                break;
            cols = (cols - 1) & 0xffff;
            if (cols == 0)
                acc = be16(img + 0x1a542);
        }

        {
            uint32_t lo0 = be32(img + desc), lo1 = be32(img + desc + 4);
            lo0 <<= bits;
            lo1 <<= bits;
            uint32_t word_mask = (0xffffu << bits) & 0xffff;
            uint32_t mask = (word_mask << 16) | word_mask;
            lo0 &= mask;
            lo1 &= mask;
            mask = ~mask;
            lo0 |= be32(img + src2) & mask;
            lo1 |= be32(img + src2 + 4) & mask;
            wr32(img + desc, lo0);
            wr32(img + desc + 4, lo1);
            desc += 8;
            wr16(img + 0x1a556, (be16(img + 0x1a556) + 2) & 0xffff);
        }

        dest += 0xa0;
        uint32_t vacc = fx_advance(be32(img + 0x1a494), be16(img + 0x1a498),
                                   be16(img + 0x1a49a));
        wr32(img + 0x1a494, vacc);
        if (rows == 0)
            break;
        rows = (rows - 1) & 0xffff;
        row = (rows != 0) ? (vacc & 0xffff) : be16(img + 0x1a544);
    }
    copy_scratch_rows(img, dest2, (be16(img + 0x1a556) - 1) & 0xffff, word);
}

/* 0x13bc2 — scale blit */
void scale_blit(uint8_t *img, uint32_t w_in, uint32_t h_in, uint32_t phase,
                 uint32_t dest_in) {
    uint32_t word, rows, phase_start;
    scale_setup(img, w_in & 0xffff, h_in & 0xffff, &word, &rows, &phase_start);
    (void)phase;
    (void)phase_start;
    uint32_t dest = dest_in;
    uint32_t row = 0;
    for (;;) {
        uint32_t src = be32(img + A_ELEM_1A528) +
                      2 * ((row & 0xffff) * be16(img + A_ELEM_1A52E));
        uint32_t cols = be16(img + 0x1a480);
        uint32_t shift = be16(img + 0x1a486);
        uint32_t dst = dest;
        uint32_t bits = (0x10 - shift) & 0xffff;

        wr32(img + dst, 0);
        wr32(img + dst + 4, 0);
        wr32(img + 0x1a48c, 0);

        uint32_t acc = 0;
        for (;;) {
            uint32_t word = (be16(img + src + ((acc & 0xfc) >> 1)) << (acc & 3)) & 0xffff;
            uint32_t s = (word << 1) & 0xffff;
            uint32_t x = (word >> 15) & 1;
            x = roxl1(img, dst, 0, x);
            x = (s >> 12) & 1;
            roxl1(img, dst, 2, x);
            s = (s << 4) & 0xffff;
            roxl1(img, dst, 4, (s >> 12) & 1);
            s = (s << 4) & 0xffff;
            roxl1(img, dst, 6, (s >> 12) & 1);

            bits = (bits - 1) & 0xffff;
            if (bits == 0) {
                dst += 8;
                bits = 0x10;
            }
            acc = fx_advance(be32(img + 0x1a48c), be16(img + 0x1a490),
                             be16(img + 0x1a492));
            wr32(img + 0x1a48c, acc);
            if (cols == 0)
                break;
            cols = (cols - 1) & 0xffff;
            if (cols == 0)
                acc = be16(img + 0x1a542);
        }

        {
            uint32_t lo0 = be32(img + dst), lo1 = be32(img + dst + 4);
            lo0 <<= bits;
            lo1 <<= bits;
            wr32(img + dst, lo0);
            wr32(img + dst + 4, lo1);
            dest += 0xa0;
        }

        uint32_t vacc = fx_advance(be32(img + 0x1a494), be16(img + 0x1a498),
                                   be16(img + 0x1a49a));
        wr32(img + 0x1a494, vacc);
        if (rows == 0)
            break;
        rows = (rows - 1) & 0xffff;
        row = (rows != 0) ? (vacc & 0xffff) : be16(img + 0x1a544);
    }
}

/* FUN_00013702 @ 0x13702 (byte 16): 128a4 + setup de 0x1a544/0x1a542/0x1a486;
 * escribe [filas][cols] en la entrada de icono que 128a4 deja en A2 (+0xe) y llama
 * al escalador 0x13a18. Devuelve A6 + 2 (los dos bytes leidos). */
uint32_t elem_command_wrap_ex(uint8_t *, uint32_t, int *);
uint32_t elem_blit_vbl(uint8_t *, uint32_t);
uint32_t screen_address(uint8_t *, uint32_t, uint32_t, uint32_t *);

/* 0x13702 — scale prep */
uint32_t scale_prep(uint8_t *img, uint32_t script) {
    uint32_t slot = 0x1a850;
    for (uint32_t k = 9;; k = (k - 1) & 0xffff) {
        if (be16(img + slot) == 0)
            break;
        slot += 0x10;
        if (k == 0)
            break;
    }
    int aborted;
    script = elem_command_wrap_ex(img, script, &aborted);
    if (aborted)
        return script;
    uint32_t phase;
    uint32_t addr = screen_address(img, be16(img + A_ELEM_1A532),
                                     be16(img + A_ELEM_1A530), &phase);
    uint32_t w = be16(img + A_ELEM_1A52E);
    uint32_t h = be16(img + A_ELEM_1A52C);
    wr16(img + 0x1a544, (h - 1) & 0xffff);
    wr16(img + 0x1a542, ((w << 2) - 1) & 0xffff);
    wr16(img + 0x1a486, phase);
    uint32_t b3 = img[script];
    uint32_t b2 = img[script + 1];
    uint32_t cursor = slot + 0xe;
    wr16(img + cursor, b2 & 0xffff);
    wr16(img + cursor - 2, b3 & 0xffff);
    b3 = (b3 << 2) & 0xffff;
    b2 = (b2 - 2) & 0xffff;
    b3 = (b3 - 2) & 0xffff;
    scale_16_16(img, b2, b3, phase, addr);
    return script + 2;
}

/* FUN_0001398e @ 0x1398e: construye en 0xd8f0 una lista de entradas
 * [word offset][word max(1,*0x1a1f2)][long movep(fila)] de 8 filas (stride 0xa0)
 * por grupo, avanzando A2 +1/+7 y A0 +0x500 por banda; cierra con un long 0. */
void scale_list_build(uint8_t *img, uint32_t w_in, uint32_t h_in, uint32_t phase, uint32_t base) {
    uint32_t desc = 0xd8f0;
    uint32_t cnt_ptr = 0x1a1f2;
    uint32_t w = w_in & 0xffff;
    (void)h_in;
    if (phase & 8)
        base += 1;
    if (phase & 4)
        w = (w + 1) & 0xffff;
    uint32_t count = w >> 1;
    if (!(w & 1))
        count = (count - 1) & 0xffff;
    for (;;) {
        uint32_t p = base;
        uint32_t n = count;
        for (;;) {
            uint32_t src = p;
            uint32_t off = (src - be32(img + A_SCREEN_PTR_A)) & 0xffff;
            wr16(img + desc, off & 0xffff);
            desc += 2;
            uint32_t v = img[cnt_ptr++];
            if (v == 0)
                v = 1;
            wr16(img + desc, v & 0xffff);
            desc += 2;
            wr32(img + desc, movep_read_l(img, src));
            desc += 4;
            src += 0xa0;
            for (uint32_t i = 0; i < 7; i++) {
                wr32(img + desc, movep_read_l(img, src));
                desc += 4;
                src += 0xa0;
            }
            p += 1;
            if (!(p & 1))
                p += 6;
            if (n == 0)
                break;
            n = (n - 1) & 0xffff;
        }
        base += 0x500;
        if (count == 0)
            break;
        count = (count - 1) & 0xffff;
    }
    wr32(img + desc, 0);
}

/* FUN_000138a6 @ 0x138a6 (byte 34): 120e4 + 1398e construyen la lista en 0xd8f0 y
 * luego, pasada a pasada (con espera de VBL), escribe en pantalla los grupos cuyo
 * contador < pasada, desplazando el offset segun el contador; A6 += 2. */
void hint_select_off(uint8_t *, uint32_t, uint32_t *);
void hint_blit_entry(uint8_t *, uint32_t, uint32_t *);
void wait_vbl(uint8_t *);
void scale_list_build(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t);
/* 0x138a6 — scale anim */
uint32_t scale_anim(uint8_t *img, uint32_t script) {
    if (img[A_SKIP_TRANSITION] != 0) {
        uint32_t script_out;
        hint_blit_entry(img, script, &script_out);
        return script_out;
    }
    uint32_t out[8];
    hint_select_off(img, img[script + 1], out);
    scale_list_build(img, out[5], out[6], out[2], out[0]);

    uint32_t pass = 1;
    for (;;) {
        uint32_t touched = 0;
        uint32_t p = 0xd8f0;
        for (;;) {
            uint32_t cnt = (be16(img + p + 2) >> 2) & 0xffff;
            if (cnt >= pass) {
                touched = 1;
            } else {
                uint32_t front = be32(img + A_SCREEN_PTR_A) + be16(img + p);
                uint32_t back = be32(img + A_SCREEN_PTR_B) + be16(img + p);
                uint32_t dst = front, src = back, src2 = p + 4;
                for (uint32_t k = 7;; k = (k - 1) & 0xffff) {
                    uint32_t word = movep_read_l(img, dst);
                    if (word == be32(img + src2)) {
                        word = movep_read_l(img, src);
                        movep_write_l(img, dst, word);
                    }
                    dst += 0xa0;
                    src += 0xa0;
                    src2 += 4;
                    if (k == 0)
                        break;
                }
                uint32_t c2 = be16(img + p + 2);
                if (c2 != 0) {
                    front += (uint32_t)(((c2 & 7) + 1) * 0xa0);
                    dst = front;
                    uint32_t off = front - be32(img + A_SCREEN_PTR_A);
                    wr16(img + p, off & 0xffff);
                    if (off >= 0x7800) {
                        wr16(img + p + 2, 0);
                    } else {
                        src2 = p + 4;
                        for (uint32_t k = 7;; k = (k - 1) & 0xffff) {
                            movep_write_l(img, dst, be32(img + src2));
                            dst += 0xa0;
                            src2 += 4;
                            if (k == 0)
                                break;
                        }
                        touched = 1;
                    }
                }
            }
            p += 0x24;
            if (be16(img + p) == 0)
                break;
        }
        wait_vbl(img);
        pass += 1;
        if (touched == 0) {
            script += 2;
            return script;
        }
    }
}

/* FUN_00013740 @ 0x13740 (byte 14): efecto de escalado animado. Tras 128a4 calcula
 * el origen (0x10fee) y las razones 1a54a/54c/552/554, y repite 13a18 con los
 * factores d2/d3 evolucionando por los acumuladores 1a546/1a54e, desplazando a2.
 * Termina con 124da sobre los registros originales del elemento. */
void blit_rect_vbl(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t,
                 uint32_t);
uint32_t screen_address_raw(uint8_t *, uint32_t, uint32_t, uint32_t *);

/* 0x13740 — scale loop */
uint32_t scale_loop(uint8_t *img, uint32_t script) {
    if (img[A_SKIP_TRANSITION] != 0)
        return elem_blit_vbl(img, script);
    int aborted;
    script = elem_command_wrap_ex(img, script, &aborted);
    if (aborted)
        return script;
    uint32_t x = be16(img + A_ELEM_1A532);
    uint32_t y = be16(img + A_ELEM_1A530);
    uint32_t w = be16(img + A_ELEM_1A52E);
    uint32_t h = be16(img + A_ELEM_1A52C);
    uint32_t src = be32(img + A_ELEM_1A528);
    uint32_t phase_elem;
    uint32_t addr_elem = screen_address(img, x, y, &phase_elem);
    wr16(img + 0x1a544, (h - 1) & 0xffff);
    uint32_t w2 = (w << 1) & 0xffff;
    wr16(img + 0x1a542, ((w2 << 1) - 1) & 0xffff);
    uint32_t x2 = ((x << 2) + ((w2 - 1) & 0xffff)) & 0xffff;
    uint32_t y2 = (y + (h >> 1)) & 0xffff;
    uint32_t phase;
    uint32_t addr = screen_address_raw(img, x2, y2, &phase);
    wr16(img + 0x1a486, phase);

    uint32_t w3 = (be16(img + A_ELEM_1A52E) << 2) & 0xffff;
    uint32_t w4 = w3;
    uint32_t half = w3;
    if (w3 < h)
        half = h;
    uint32_t den = half;
    half >>= 1;
    wr16(img + 0x1a54a, ((h << 1) / den) & 0xffff);
    wr16(img + 0x1a54c, ((((h << 1) % den) << 16) / den) & 0xffff);
    wr16(img + 0x1a552, ((w4 << 1) / den) & 0xffff);
    wr16(img + 0x1a554, (((((w4 << 1) % den)) << 16) / den) & 0xffff);
    wr32(img + 0x1a546, 2);
    wr32(img + 0x1a54e, 2);
    uint32_t f2 = 2, f3 = 2;
    uint32_t n = (half - 3) & 0xffff;
    uint32_t dst = addr;
    for (;;) {
        scale_16_16(img, f2, f3, phase, dst);
        {
            uint32_t acc = be32(img + 0x1a546);
            uint32_t lo = acc & 0xffff, hi = acc >> 16;
            uint32_t nlo = (lo + be16(img + 0x1a54a)) & 0xffff;
            uint32_t sum = hi + be16(img + 0x1a54c);
            uint32_t cy = (sum > 0xffff) ? 1 : 0;
            wr32(img + 0x1a546, ((sum & 0xffff) << 16) | nlo);
            f2 = nlo;
            if (cy || !(nlo & 1))
                dst -= 0xa0;
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
                f3 = (nlo + 1) & 0xffff;
                adjust = 1;
            } else {
                wr32(img + 0x1a54e, ((sum & 0xffff) << 16) | nlo);
                f3 = nlo;
                adjust = !(nlo & 1);
            }
            if (adjust) {
                uint32_t phase_tmp = be16(img + 0x1a486);
                if (phase_tmp == 0) {
                    dst -= 8;
                    phase_tmp = 0x10;
                }
                wr16(img + 0x1a486, (phase_tmp - 1) & 0xffff);
            }
        }
        if (n == 0)
            break;
        n = (n - 1) & 0xffff;
    }
    blit_rect_vbl(img, addr_elem, src, w, h, phase_elem);
    return script;
}
