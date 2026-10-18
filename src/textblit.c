/* textblit.c — the register-driven proportional text blitters.
 *
 * FUN_00011196 @ 0x11196: map D0's low word through a nibble/byte permutation and a
 * 16-step rotate-through-X chain; the result lands in D0 (move.w d1,d0) and D1.
 * FUN_00011050 @ 0x11050: forward blit — A0 dest (planes via movep, stride 0xa0),
 * A1 source, A3 source stride, D2 outer/columns, D3 inner/rows, D7 pixel phase.
 * FUN_00011102 @ 0x11102: reverse variant — each source word goes through
 * FUN_00011196 first and the phase walks downward (D7 -= 4, A0 -= 8 on wrap).
 *
 * Both blitters expand each source word to four plane bytes through the 0x55b02
 * scratch (the same `movep` nibble expansion blit_sprite uses) and merge them into
 * the destination long with a mask computed from the expanded word itself.
 */
#include <stdint.h>

#include "machine.h"
#include "addrs.h"
#include "os.h"

uint32_t advance_text_entry(uint8_t *img, uint32_t a1, uint32_t d4);
uint32_t screen_address(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t *d7out);

static uint32_t ror32(uint32_t v, uint32_t n) {
    n &= 31;
    return n ? ((v >> n) | (v << (32 - n))) : v;
}
static uint32_t rol32(uint32_t v, uint32_t n) {
    n &= 31;
    return n ? ((v << n) | (v >> (32 - n))) : v;
}
static uint32_t swap_words(uint32_t v) {
    return (v << 16) | (v >> 16);
}

/* the 0x55b02 scratch is read back with `move.l (a5)`, four consecutive bytes */
static uint32_t read_long(uint8_t *img, uint32_t a) {
    return ((uint32_t)img[a] << 24) | ((uint32_t)img[a + 1] << 16) |
           ((uint32_t)img[a + 2] << 8) | img[a + 3];
}

static void movep_write_w(uint8_t *img, uint32_t a, uint32_t v) {
    img[a] = (v >> 8) & 0xff;
    img[a + 2] = v & 0xff;
}

/* movep: bytes alternos (alto en +0, bajo en +2, ...) */
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

void bit_reverse_word(uint32_t d0, uint32_t d1, uint32_t *d0o, uint32_t *d1o) {
    d0 &= 0xffff;
    d1 &= 0xffff;
    uint32_t x;
    uint32_t b = d0 & 0xff;
    x = (b >> 3) & 1;
    d0 = (d0 & 0xff00) | (((b >> 4) | (b << 4)) & 0xff);
    x = (d0 >> 7) & 1;
    d0 = ((d0 >> 8) | (d0 << 8)) & 0xffff;
    b = d0 & 0xff;
    x = (b >> 3) & 1;
    d0 = (d0 & 0xff00) | (((b >> 4) | (b << 4)) & 0xff);
    for (int i = 0; i < 16; i++) {
        uint32_t nd = ((x << 15) | (d0 >> 1)) & 0xffff;
        x = d0 & 1;
        d0 = nd;
        uint32_t nd1 = ((d1 << 1) | x) & 0xffff;
        x = (d1 >> 15) & 1;
        d1 = nd1;
    }
    *d0o = d1;
    *d1o = d1;
}

void text_blit_fwd(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t a3,
                     uint32_t d2w, uint32_t d3w, uint32_t d7w) {
    uint32_t a5 = A_BLIT_SCRATCH;
    uint32_t outer = (d2w - 1) & 0xffff;
    uint32_t inner = (d3w - 1) & 0xffff;
    uint32_t d7 = d7w & 0xffff;
    for (;;) {
        uint32_t a2 = a0, a4 = a1, d5 = 0;
        if (d7 & 8)
            a2 += 1;
        if (d7 & 4)
            d5 = 4;
        uint32_t ic = inner;
        for (;;) {
            uint32_t d6 = be16(img + a4);
            movep_write_w(img, a5, d6);
            d6 = ((d6 << 4) | (d6 >> 12)) & 0xffff;   /* rol.w #4 */
            movep_write_w(img, a5 + 1, d6);
            d6 = read_long(img, a5);
            d6 = ror32(d6, d5);
            uint32_t d4 = d6, d1 = d6;
            d1 = swap_words(d1);
            d4 |= d1;
            d1 = rol32(d1, 8);
            d4 |= d1;
            d1 = swap_words(d1);
            d4 |= d1;
            d4 &= 0xf0f0f0f0u;
            d4 = ror32(d4, d5);
            d1 = ~d4;
            d6 &= d4;
            uint32_t d0 = movep_read_l(img, a2);
            d0 &= d1;
            d0 |= d6;
            movep_write_l(img, a2, d0);
            a2 += 0xa0;
            a4 += a3;
            ic = (ic - 1) & 0xffff;
            if (ic == 0xffff)
                break;
        }
        a1 += 2;
        d7 = (d7 + 4) & 0xf;
        if (d7 == 0)
            a0 += 8;
        outer = (outer - 1) & 0xffff;
        if (outer == 0xffff)
            break;
    }
}

void text_blit_rev(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t a3,
                     uint32_t d2w, uint32_t d3w, uint32_t d7w) {
    uint32_t a5 = A_BLIT_SCRATCH;
    uint32_t outer = (d2w - 1) & 0xffff;
    uint32_t inner = (d3w - 1) & 0xffff;
    uint32_t d7 = d7w & 0xffff;
    uint32_t d1 = 0;   /* only feeds FUN_00011196, whose result is independent of it */
    for (;;) {
        uint32_t a2 = a0, a4 = a1, d5 = 0;
        if (d7 & 8)
            a2 += 1;
        if (d7 & 4)
            d5 = 4;
        uint32_t ic = inner;
        for (;;) {
            uint32_t d0, t1;
            bit_reverse_word(be16(img + a4), d1, &d0, &t1);
            d1 = t1;
            movep_write_w(img, a5, d0);
            d0 = ((d0 << 4) | (d0 >> 12)) & 0xffff;   /* rol.w #4 */
            movep_write_w(img, a5 + 1, d0);
            uint32_t d2 = read_long(img, a5);
            d2 = ror32(d2, d5);
            uint32_t d4 = d2, d1m = d2;
            d1m = swap_words(d1m);
            d4 |= d1m;
            d1m = rol32(d1m, 8);
            d4 |= d1m;
            d1m = swap_words(d1m);
            d4 |= d1m;
            d4 &= 0xf0f0f0f0u;
            d4 = ror32(d4, d5);
            d1m = ~d4;
            d2 &= d4;
            uint32_t d0b = movep_read_l(img, a2);
            d0b &= d1m;
            d0b |= d2;
            movep_write_l(img, a2, d0b);
            a2 += 0xa0;
            a4 += a3;
            ic = (ic - 1) & 0xffff;
            if (ic == 0xffff)
                break;
        }
        a1 += 2;
        if (d7 < 4) {
            a0 -= 8;
            d7 = 0xc;
        } else {
            d7 -= 4;
        }
        outer = (outer - 1) & 0xffff;
        if (outer == 0xffff)
            break;
    }
}

/* FUN_00014138 @ 0x14138: process one script/text entry at A6.
 *
 * A6[0] selects a D4-entry skip in the text table at 0x41fb0; the entry's first two
 * bytes are read (D3/D2), A6[1] is the character/position and A6[2] its flags. The
 * function then computes the screen address (0x10fec) and dispatches to the forward
 * blitter (FUN_00011050) or the bit-reversed one (FUN_00011102) depending on A6[1]
 * bit 7.
 *
 * The `_ex` form also reports the registers the original leaves for its caller:
 * A0 (the 0x10fec screen address, before the -0x8000 back-buffer shift), D2/D3
 * (the entry's columns/rows) and D7 (the pixel phase).  The original preserves
 * them across the blit with the `movem` at 0x141b0/0x141d6, and FUN_000141dc
 * (0x141dc) feeds them straight into FUN_0001215e, so the reconstruction has to
 * hand them back. */
void run_text_entry_ex(uint8_t *img, uint32_t a6,
                    uint32_t *a0o, uint32_t *d2o, uint32_t *d3o, uint32_t *d7o) {
    uint32_t d4 = img[a6] & 0xff;
    uint32_t a1 = advance_text_entry(img, A_TEXT_TABLE, d4);

    uint32_t d0 = img[a6 + 1] & 0xff;
    uint32_t d1 = img[a6 + 2] & 0xff;
    uint32_t d3 = img[a1++] & 0xff;
    uint32_t d2 = img[a1++] & 0xff;
    if (d0 & 0x80) {
        d0 = (d0 + d2 - 1) & 0xff;
        d0 &= 0x7f;
    }
    uint32_t d5 = (d2 << 1) & 0xffff;
    if (d1 & 0x80) {
        uint32_t off = ((d5 & 0xff) * (d3 & 0xffff)) & 0xffffffff;
        a1 += off;
        a1 -= d5;
        d5 = (~d5 + 1) & 0xffffffff;
    }
    wr32(img + A_DAT_1A534, d5);
    d1 = (d1 << 1) & 0xff;
    if (img[a6] == 0x53) {                 /* 'S' */
        uint32_t w = be16(img + A_DAT_1A49E);
        wr16(img + A_DAT_1A49E, (w >> 1) & 0xffff);
        if (w & 1)
            d1 = (d1 - 8) & 0xffff;
    }
    uint32_t d7;
    uint32_t screen = screen_address(img, d0, d1, &d7);
    if (a0o) *a0o = screen;
    if (d2o) *d2o = d2;
    if (d3o) *d3o = d3;
    if (d7o) *d7o = d7;
    uint32_t a0 = screen - 0x8000;
    uint32_t a3 = be32(img + A_DAT_1A534);
    if (img[a6 + 1] & 0x80)
        text_blit_rev(img, a0, a1, a3, d2, d3, d7);
    else
        text_blit_fwd(img, a0, a1, a3, d2, d3, d7);
}

void run_text_entry(uint8_t *img, uint32_t a6) {
    run_text_entry_ex(img, a6, 0, 0, 0, 0);
}

void func_126e0(uint8_t *img, uint32_t d2, uint32_t d3, uint32_t d7, uint32_t a1);

/* FUN_00016dbc @ 0x16dbc: `a6 += 1`; calcula la direccion de pantalla de (D0,D1)
 * (0x10fec), avanza la entrada del banco A_TEXT_TABLE por D4 (0x10f8e), lee
 * [rows][cols], fija el stride 0x1a534 = cols*2 y dibuja el elemento (0x126e0).
 * Devuelve A0 (direccion de pantalla) y deja A5 = 0 (lo que los llamantes esperan:
 * 0x16abe/0x16d3c lo usan como offset del descriptor y lo incrementan). */
uint32_t func_16dbc(uint8_t *img, uint32_t *a6, uint32_t d0, uint32_t d1, uint32_t d4,
                     uint32_t *a5_out) {
    *a6 += 1;
    uint32_t d7;
    uint32_t a0 = screen_address(img, d0, d1, &d7);
    uint32_t a1 = advance_text_entry(img, A_TEXT_TABLE, d4);
    uint32_t d3 = img[a1++];
    uint32_t d2 = img[a1++];
    wr32(img + A_DAT_1A534, (d2 << 1) & 0xffff);
    func_126e0(img, d2, d3, d7, a1);
    *a5_out = 0;
    return a0;
}

void func_16b2e(uint8_t *img, uint32_t d2, uint32_t d3, uint32_t a3, uint32_t a5);
void func_16b76(uint8_t *img, uint32_t a3, uint32_t d2, uint32_t d3);
void wait_vbl_even(uint8_t *img);
void reveal_region(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3);

/* FUN_00016c6c @ 0x16c6c: cuerpo compartido del wipe de texto horizontal. A0 = base
 * de pantalla (de 0x16dbc), D2/D3 grupos/filas, A5 = offset del descriptor. Rota D3
 * filas y reblitea (0x16b76/0x16b2e) esperando una VBL por iteracion (D7+1 = 4, o 3
 * si A6[0] != 0); al final reblitea la ultima fila si A6[0] != 0 y A6 += 1. */
uint32_t func_16c6c(uint8_t *img, uint32_t a6, uint32_t a0, uint32_t d2, uint32_t d3,
                     uint32_t a5) {
    uint32_t a3 = a0;
    uint32_t d7 = 3;
    if (img[a6] != 0) {
        d3 = (d3 - 4) & 0xffff;
        d7 = (d7 - 1) & 0xffff;
    }
    uint32_t n = d7;
    for (;;) {
        wait_vbl_even(img);
        func_16b76(img, a3, d2, d3);
        a3 += 0xa0;
        d3 = (d3 - 1) & 0xffff;
        func_16b2e(img, d2, d3, a3, a5);
        if (n == 0) break;
        n--;
    }
    if (img[a6] != 0)
        func_16b76(img, a3, d2, d3);
    return a6 + 1;
}

/* FUN_00016c48 @ 0x16c48: wipe de texto (d4 = 3, x = 0x23, y = 0xad o 0x31 segun
 * A6[1]); cae en el cuerpo 0x16c6c con D2 = 2, D3 = 8. Devuelve A6 = entrada+2
 * (0x16dbc + el addq final de 0x16c6c): el VM original continua 2 bytes mas alla. */
uint32_t func_16c48(uint8_t *img, uint32_t a6) {
    uint32_t d1 = 0xad;
    if (img[a6 + 1] != 0)
        d1 += 4;
    uint32_t a5;
    uint32_t a0 = func_16dbc(img, &a6, 0x23, d1, 3, &a5);
    return func_16c6c(img, a6, a0, 2, 8, a5);
}

/* FUN_00016c00 @ 0x16c00: transicion de texto (d4 = 0x77); blitea un bloque y luego
 * prepara el revelado (0x12e2e) con x=0x1c,y=0x8b. Devuelve A6 = entrada+1 (0x16dbc). */
uint32_t func_16c00(uint8_t *img, uint32_t a6) {
    uint32_t a5;
    uint32_t a0 = func_16dbc(img, &a6, 0x20, 0x8b, 0x77, &a5);
    func_16b2e(img, 3, 0x14, a0 - 0x8000, a5);
    wr16(img + 0x1a56a, 1);
    img[0x1a577] = 0x11;
    reveal_region(img, 0x1c, 0x8b, 0x12, 0x28);
    return a6;
}

/* Bucle comun de los wipes con VBL (0x16ca6/0x16ce2/0x16d3c/0x16d78): una espera
 * (0x16d1e), copia (0x16b76), avance y OR con el descriptor (0x16b2e); D7+1 pasadas.
 * `mode`: 0 = A3 +0xa0 y D3-1 (0x16ca6), 1 = A3 -0xa0 y D3+1 (0x16ce2),
 * 2 = A5 +0x40 y D3-1 (0x16d3c), 3 = A5 -0x40 y D3+1 (0x16d78). */
static void wipe_vbl(uint8_t *img, uint32_t a0, uint32_t a5, uint32_t d2, uint32_t d3,
                     uint32_t d7, int mode) {
    uint32_t a3 = a0;
    uint32_t n = d7;
    for (;;) {
        wait_vbl_even(img);
        func_16b76(img, a3, d2, d3);
        if (mode == 0) {
            a3 += 0xa0;
            d3 = (d3 - 1) & 0xffff;
        } else if (mode == 1) {
            a3 -= 0xa0;
            d3 = (d3 + 1) & 0xffff;
        } else if (mode == 2) {
            a5 += 0x40;
            d3 = (d3 - 1) & 0xffff;
        } else {
            a5 -= 0x40;
            d3 = (d3 + 1) & 0xffff;
        }
        func_16b2e(img, d2, d3, a3, a5);
        if (n == 0) break;
        n--;
    }
}

/* FUN_00016ca6 @ 0x16ca6: wipe hacia abajo (34 VBL), D2 = 4, D3 0x43->...
 * Devuelve A6 = entrada+1 (0x16dbc). */
uint32_t func_16ca6(uint8_t *img, uint32_t a6) {
    uint32_t a5;
    uint32_t a0 = func_16dbc(img, &a6, 0x1c, 0x6a, 0x76, &a5);
    wipe_vbl(img, a0, a5, 4, 0x43, 0x21, 0);
    return a6;
}

/* FUN_00016ce2 @ 0x16ce2: wipe hacia arriba (68 VBL), D2 = 4, D3 0->...
 * Devuelve A6 = entrada+1 (0x16dbc). */
uint32_t func_16ce2(uint8_t *img, uint32_t a6) {
    uint32_t a5;
    uint32_t a0 = func_16dbc(img, &a6, 0x1c, 0xae, 0x76, &a5);
    wipe_vbl(img, a0, a5, 4, 0, 0x43, 1);
    return a6;
}

/* FUN_00016d3c @ 0x16d3c: wipe con A5 +0x40 por pasada (16 VBL), A3 fijo.
 * Devuelve A6 = entrada+1 (0x16dbc). */
uint32_t func_16d3c(uint8_t *img, uint32_t a6) {
    uint32_t a5;
    uint32_t a0 = func_16dbc(img, &a6, 0x23, 0x12, 0x60, &a5);
    wipe_vbl(img, a0, a5, 1, 0x10, 0x0f, 2);
    return a6;
}

/* FUN_00016d78 @ 0x16d78: wipe con A5 -0x40 por pasada (15 VBL) desde 0x3c0.
 * Devuelve A6 = entrada+1 (0x16dbc). */
uint32_t func_16d78(uint8_t *img, uint32_t a6) {
    uint32_t a5;
    uint32_t a0 = func_16dbc(img, &a6, 0x23, 0x12, 0x60, &a5);
    wipe_vbl(img, a0, 0x3c0, 1, 0, 0x0e, 3);
    return a6;
}

/* FUN_000141dc @ 0x141dc: run FUN_00014138 on A6+1, then the masked row blit
 * (FUN_0001215e) with the A0/D2/D3/D7 that FUN_00014138 left behind. */
void mask_restore_rows(uint8_t *img, uint32_t a0, uint32_t d2, uint32_t d3, uint32_t d7);
void func_141dc(uint8_t *img, uint32_t a6) {
    uint32_t a0, d2, d3, d7;
    run_text_entry_ex(img, a6 + 1, &a0, &d2, &d3, &d7);
    mask_restore_rows(img, a0, d2, d3, d7);
}

/* FUN_000141ea @ 0x141ea: run FUN_00014138 on the entry at A6+1 (then A6 += 3, unused). */
void run_text_entry_1(uint8_t *img, uint32_t a6) {
    run_text_entry(img, a6 + 1);
}

/* FUN_00011014 @ 0x11014: a bare `rts` (no-op stub in the original). */
void noop_11014(uint8_t *img) {
    (void)img;
}

/* FUN_00011016 @ 0x11016 (A4 = 0x8000) y FUN_00011020 @ 0x11020 (A4 = 0), cuerpo comun
 * FUN_00011026: avanza la entrada de texto de 0x28ad2 D4 posiciones, lee [rows][cols] y
 * dibuja con el blit de texto hacia delante (stride cols*2) restando A4 a la dirección. */
void func_11026(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d4, uint32_t a4off) {
    uint32_t a1 = advance_text_entry(img, 0x28ad2, d4);
    uint32_t d7;
    uint32_t a0 = screen_address(img, d0, d1, &d7);
    a0 -= a4off;
    uint32_t d3 = img[a1++];
    uint32_t d2 = img[a1++];
    uint32_t a3 = (d2 << 1) & 0xffff;
    wr32(img + 0x1a534, a3);
    text_blit_fwd(img, a0, a1, a3, d2, d3, d7);
}

void func_11016(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d4) {
    func_11026(img, d0, d1, d4, 0x8000);
}

void func_11020(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d4) {
    func_11026(img, d0, d1, d4, 0);
}

/* FUN_00011030 @ 0x11030: cuerpo de 0x11026 sin el avance de entrada (A1 ya apunta a
 * [rows][cols]); direccion de pantalla menos A4 y blit hacia delante. */
void func_11030(uint8_t *img, uint32_t a1, uint32_t d0, uint32_t d1, uint32_t a4off) {
    uint32_t d7;
    uint32_t a0 = screen_address(img, d0, d1, &d7);
    a0 -= a4off;
    uint32_t d3 = img[a1++];
    uint32_t d2 = img[a1++];
    uint32_t a3 = (d2 << 1) & 0xffff;
    wr32(img + 0x1a534, a3);
    text_blit_fwd(img, a0, a1, a3, d2, d3, d7);
}

/* FUN_000110da @ 0x110da: como 0x11026 pero con el blit inverso (0x11102) y SIN
 * restar A4; D4 = indice de la entrada de texto en 0x28ad2. */
void func_110da(uint8_t *img, uint32_t d4, uint32_t d0, uint32_t d1) {
    uint32_t a1 = advance_text_entry(img, 0x28ad2, d4);
    uint32_t d7;
    uint32_t a0 = screen_address(img, d0, d1, &d7);
    uint32_t d3 = img[a1++];
    uint32_t d2 = img[a1++];
    uint32_t a3 = (d2 << 1) & 0xffff;
    wr32(img + 0x1a534, a3);
    text_blit_rev(img, a0, a1, a3, d2, d3, d7);
}

/* FUN_00010768 @ 0x10768: `rts` pelado (destino del modo sin overlay, $1acc6 == 0x87). */
void func_10768(uint8_t *img) { (void)img; }

/* FUN_000122ee @ 0x122ee: espera un frame (0x13594) y dibuja el prompt parpadeante con el
 * overlay 0x11020 en (0x4b, 0x9b); alterna el flag 0x1a74e (el color 0x16/0x17). */
void wait_vbl(uint8_t *img);
void func_11020(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d4);
void func_122ee(uint8_t *img) {
    if (img[0x1acc6] == 0x87)
        return;
    wait_vbl(img);
    uint32_t d4 = img[0x1a74e] ? 0x17 : 0x16;
    func_11020(img, 0x4b, 0x9b, d4);
    img[0x1a74e] = (uint8_t)~img[0x1a74e];
}

/* FUN_00012292 @ 0x12292: espera un clic (bit 2 de $1a456) leyendo frames con 0x13698 y
 * parpadeando el prompt (0x122ee) cada 16 frames. En modo $1acc6 == 0x87 (sin overlay)
 * sale por GEMDOS Cnecin (trap #1/$8). A5/A6 los preserva el `movem.l` de entrada. */
void wait_frame(uint8_t *img);
void func_122ee(uint8_t *img);
void prompt_wait(uint8_t *img) {
    if (img[0x1acc6] == 0x87) {
        uint32_t key;
        os_cnecin(img, &key);                        /* trap #1/$8 */
        return;
    }
    img[0x1a74e] = 0;
    for (;;) {                                       /* 0x122a8: soltar el botón */
        wait_frame(img);
        if (!(img[A_INPUT_BUTTONS] & 0x04))
            break;                                   /* bit2 == 0 -> continuar */
    }
    while (!(img[A_INPUT_BUTTONS] & 0x04)) {         /* 0x122b8 */
        if ((img[A_DAT_1ACD4] & 0x0f) == 0)
            func_122ee(img);                        /* parpadeo cada 16 frames */
        wait_frame(img);
    }
    if (img[0x1a74e] != 0)
        func_122ee(img);
}

/* FUN_00016abe @ 0x16abe: rejilla de texto sobre la caja ($1ab40..43): 16dbc prepara
 * (d4=0x50) y por cada banda de 0x1e filas (d3 = alto/0x1e) blitea D2+1 grupos de
 * 0x1d filas con 0x16b2e. Devuelve A6 (avanzado por 16dbc). */
uint32_t func_16dbc(uint8_t *img, uint32_t *a6, uint32_t d0, uint32_t d1, uint32_t d4,
                     uint32_t *a5_out);

uint32_t func_16abe(uint8_t *img, uint32_t a6) {
    uint32_t d0 = img[A_DAT_1AB40];
    uint32_t d1 = img[A_DAT_1AB42];
    uint32_t a5;
    uint32_t a0 = func_16dbc(img, &a6, d0, d1, 0x50, &a5);
    uint32_t d2 = (((uint32_t)img[A_DAT_1AB41] - d0) & 0xff) >> 2;
    uint32_t d3 = ((uint32_t)img[A_DAT_1AB43] - d1) & 0xff;
    d3 = d3 / 0x1e;
    uint32_t a3 = a0;
    d2 = (d2 - 1) & 0xffff;
    d3 = (d3 - 1) & 0xffff;
    for (;;) {
        uint32_t base = a3;
        uint32_t cnt = d2;
        for (;;) {
            func_16b2e(img, 0, 0x1d, a3, a5);
            a3 += 8;
            if (cnt == 0)
                break;
            cnt--;
        }
        a3 = (base + 0x12c0) & 0xffffffffu;
        if (d3 == 0)
            break;
        d3--;
    }
    return a6;
}

/* FUN_00016b9c @ 0x16b9c: wipe grande de texto. Primer 0x16dbc (d4=3, x=0x23,
 * y=0xad) + 0x16b2e(2,7); segundo 0x16dbc (d4=0x77, x=0x20, y=0x5e) +
 * 0x16b2e(3,0x47); prompt (0x12292) y 72 pasadas {0x16d1e (VBL), 0x16b76 con
 * d3+1, 0x16b2e con d3, a3 += 0xa0}; cierra con 0x16b76(d3=0). Devuelve
 * A6 = entrada+1 (16dbc +1, subq -1, 16dbc +1). */
void prompt_wait(uint8_t *img);
void wait_vbl_even(uint8_t *img);
void func_16b76(uint8_t *img, uint32_t a3, uint32_t d2, uint32_t d3);

uint32_t func_16b9c(uint8_t *img, uint32_t a6) {
    uint32_t a5;
    uint32_t a0 = func_16dbc(img, &a6, 0x23, 0xad, 3, &a5);
    a6 = (a6 - 1) & 0xffffffff;
    func_16b2e(img, 2, 7, a0, a5);
    a0 = func_16dbc(img, &a6, 0x20, 0x5e, 0x77, &a5);
    uint32_t d3 = 0x47, d2 = 3, a3 = a0;
    func_16b2e(img, d2, d3, a3, a5);
    prompt_wait(img);
    for (;;) {
        wait_vbl_even(img);
        func_16b76(img, a3, d2, (d3 + 1) & 0xffff);
        a3 = (a3 + 0xa0) & 0xffffffff;
        func_16b2e(img, d2, d3, a3, a5);
        if (d3 == 0)
            break;
        d3 = (d3 - 1) & 0xffff;
    }
    func_16b76(img, a3, d2, 0);
    return a6;
}
