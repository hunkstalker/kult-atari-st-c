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
#include "kult_internal.h"

uint32_t advance_text_entry(uint8_t *, uint32_t, uint32_t);
uint32_t screen_address(uint8_t *, uint32_t, uint32_t, uint32_t *);

/* the 0x55b02 scratch is read back with `move.l (a5)`, four consecutive bytes */
/* movep: bytes alternos (alto en +0, bajo en +2, ...) */
/* 0x11196 — permutacion de palabra (resultado en D0/D1) */
void bit_reverse_word(uint32_t w0, uint32_t w1, uint32_t *w0_out, uint32_t *w1_out) {
    /* Las ops `.b`/`.w` del original preservan la palabra alta de D0/D1, y el
     * final `move.w d1,d0` solo sustituye la baja. */
    uint32_t hi0 = w0 & 0xffff0000u;
    uint32_t hi1 = w1 & 0xffff0000u;
    w0 &= 0xffff;
    w1 &= 0xffff;
    uint32_t x;
    uint32_t byte = w0 & 0xff;
    x = (byte >> 3) & 1;
    w0 = (w0 & 0xff00) | (((byte >> 4) | (byte << 4)) & 0xff);
    x = (w0 >> 7) & 1;
    w0 = ((w0 >> 8) | (w0 << 8)) & 0xffff;
    byte = w0 & 0xff;
    x = (byte >> 3) & 1;
    w0 = (w0 & 0xff00) | (((byte >> 4) | (byte << 4)) & 0xff);
    for (int i = 0; i < 16; i++) {
        uint32_t n0 = ((x << 15) | (w0 >> 1)) & 0xffff;
        x = w0 & 1;
        w0 = n0;
        uint32_t n1 = ((w1 << 1) | x) & 0xffff;
        x = (w1 >> 15) & 1;
        w1 = n1;
    }
    *w0_out = hi0 | w1;
    *w1_out = hi1 | w1;
}

/* 0x11050 — blit de texto directo (A0/A1/A3/D2/D3/D7) */
void text_blit_fwd(uint8_t *img, uint32_t dest, uint32_t src, uint32_t stride,
                     uint32_t cols, uint32_t rows, uint32_t phase_in) {
    uint32_t scratch = A_BLIT_SCRATCH;
    uint32_t col = (cols - 1) & 0xffff;
    uint32_t row = (rows - 1) & 0xffff;
    uint32_t phase = phase_in & 0xffff;
    for (;;) {
        uint32_t dst = dest, s = src, shift = 0;
        if (phase & 8)
            dst += 1;
        if (phase & 4)
            shift = 4;
        uint32_t n = row;
        for (;;) {
            uint32_t word = be16(img + s);
            movep_write_w(img, scratch, word);
            word = ((word << 4) | (word >> 12)) & 0xffff;   /* rol.w #4 */
            movep_write_w(img, scratch + 1, word);
            word = read_long(img, scratch);
            word = ror32(word, shift);
            uint32_t mask = word, inv = word;
            inv = swap_words(inv);
            mask |= inv;
            inv = rol32(inv, 8);
            mask |= inv;
            inv = swap_words(inv);
            mask |= inv;
            mask &= 0xf0f0f0f0u;
            mask = ror32(mask, shift);
            inv = ~mask;
            word &= mask;
            uint32_t out = movep_read_l(img, dst);
            out &= inv;
            out |= word;
            movep_write_l(img, dst, out);
            dst += 0xa0;
            s += stride;
            n = (n - 1) & 0xffff;
            if (n == 0xffff)
                break;
        }
        src += 2;
        phase = (phase + 4) & 0xf;
        if (phase == 0)
            dest += 8;
        col = (col - 1) & 0xffff;
        if (col == 0xffff)
            break;
    }
}

/* 0x11102 — idem con palabra invertida por FUN_00011196 */
void text_blit_rev(uint8_t *img, uint32_t dest, uint32_t src, uint32_t stride,
                     uint32_t cols, uint32_t rows, uint32_t phase_in) {
    uint32_t scratch = A_BLIT_SCRATCH;
    uint32_t col = (cols - 1) & 0xffff;
    uint32_t row = (rows - 1) & 0xffff;
    uint32_t phase = phase_in & 0xffff;
    uint32_t rev_state = 0;   /* only feeds FUN_00011196, whose result is independent of it */
    for (;;) {
        uint32_t dst = dest, s = src, shift = 0;
        if (phase & 8)
            dst += 1;
        if (phase & 4)
            shift = 4;
        uint32_t n = row;
        for (;;) {
            uint32_t rev, rev_next;
            bit_reverse_word(be16(img + s), rev_state, &rev, &rev_next);
            rev_state = rev_next;
            movep_write_w(img, scratch, rev);
            rev = ((rev << 4) | (rev >> 12)) & 0xffff;   /* rol.w #4 */
            movep_write_w(img, scratch + 1, rev);
            uint32_t word = read_long(img, scratch);
            word = ror32(word, shift);
            uint32_t mask = word, inv = word;
            inv = swap_words(inv);
            mask |= inv;
            inv = rol32(inv, 8);
            mask |= inv;
            inv = swap_words(inv);
            mask |= inv;
            mask &= 0xf0f0f0f0u;
            mask = ror32(mask, shift);
            inv = ~mask;
            word &= mask;
            uint32_t out = movep_read_l(img, dst);
            out &= inv;
            out |= word;
            movep_write_l(img, dst, out);
            dst += 0xa0;
            s += stride;
            n = (n - 1) & 0xffff;
            if (n == 0xffff)
                break;
        }
        src += 2;
        if (phase < 4) {
            dest -= 8;
            phase = 0xc;
        } else {
            phase -= 4;
        }
        col = (col - 1) & 0xffff;
        if (col == 0xffff)
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
void run_text_entry_ex(uint8_t *img, uint32_t entry, uint32_t *addr_out,
                    uint32_t *cols_out, uint32_t *rows_out, uint32_t *phase_out,
                    uint32_t *yflags_out) {
    uint32_t count = img[entry] & 0xff;
    uint32_t table = advance_text_entry(img, A_TEXT_TABLE, count);

    uint32_t ch = img[entry + 1] & 0xff;
    uint32_t flags = img[entry + 2] & 0xff;
    uint32_t rows = img[table++] & 0xff;
    uint32_t cols = img[table++] & 0xff;
    if (ch & 0x80) {
        ch = (ch + cols - 1) & 0xff;
        ch &= 0x7f;
    }
    uint32_t stride = (cols << 1) & 0xffff;
    if (flags & 0x80) {
        uint32_t off = ((stride & 0xff) * (rows & 0xffff)) & 0xffffffff;
        table += off;
        table -= stride;
        stride = (~stride + 1) & 0xffffffff;
    }
    wr32(img + A_DAT_1A534, stride);
    flags = (flags << 1) & 0xff;
    if (img[entry] == 0x53) {                 /* 'S' */
        uint32_t w = be16(img + A_DAT_1A49E);
        wr16(img + A_DAT_1A49E, (w >> 1) & 0xffff);
        if (w & 1)
            flags = (flags - 8) & 0xffff;
    }
    uint32_t phase;
    uint32_t screen = screen_address(img, ch, flags, &phase);
    if (addr_out) *addr_out = screen;
    if (cols_out) *cols_out = cols;
    if (rows_out) *rows_out = rows;
    if (phase_out) *phase_out = phase;
    /* D5 = D1 ya ajustado (`lsl.b` y el -8 de 'S') con `move.w d1,d5`, que solo
     * sustituye la palabra BAJA: la alta conserva la de D5, que es la del stride
     * (0xffff si el bit7 lo nego). Lo consume 0x13f86 para la caja. */
    if (yflags_out) *yflags_out = (stride & 0xffff0000u) | (flags & 0xffffu);
    uint32_t addr = screen - 0x8000;
    uint32_t stride2 = be32(img + A_DAT_1A534);
    if (img[entry + 1] & 0x80)
        text_blit_rev(img, addr, table, stride2, cols, rows, phase);
    else
        text_blit_fwd(img, addr, table, stride2, cols, rows, phase);
}

/* 0x14138 — lee entrada de script A6, calcula pantalla y blitea texto */
void run_text_entry(uint8_t *img, uint32_t entry) {
    run_text_entry_ex(img, entry, 0, 0, 0, 0, 0);
}

void grid_fill(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t);

/* FUN_00016dbc @ 0x16dbc: `a6 += 1`; calcula la direccion de pantalla de (D0,D1)
 * (0x10fec), avanza la entrada del banco A_TEXT_TABLE por D4 (0x10f8e), lee
 * [rows][cols], fija el stride 0x1a534 = cols*2 y dibuja el elemento (0x126e0).
 * Devuelve A0 (direccion de pantalla) y deja A5 = 0 (lo que los llamantes esperan:
 * 0x16abe/0x16d3c lo usan como offset del descriptor y lo incrementan). */
uint32_t text_transition(uint8_t *img, uint32_t *entry, uint32_t x, uint32_t y, uint32_t index,
                     uint32_t *offset_out) {
    *entry += 1;
    uint32_t phase;
    uint32_t addr = screen_address(img, x, y, &phase);
    uint32_t table = advance_text_entry(img, A_TEXT_TABLE, index);
    uint32_t rows = img[table++];
    uint32_t cols = img[table++];
    wr32(img + A_DAT_1A534, (cols << 1) & 0xffff);
    grid_fill(img, cols, rows, phase, table);
    *offset_out = 0;
    return addr;
}

void or_rows(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t);
void row_blit(uint8_t *, uint32_t, uint32_t, uint32_t);
void wait_vbl_even(uint8_t *);
void reveal_region(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t);

/* FUN_00016c6c @ 0x16c6c: cuerpo compartido del wipe de texto horizontal. A0 = base
 * de pantalla (de 0x16dbc), D2/D3 grupos/filas, A5 = offset del descriptor. Rota D3
 * filas y reblitea (0x16b76/0x16b2e) esperando una VBL por iteracion (D7+1 = 4, o 3
 * si A6[0] != 0); al final reblitea la ultima fila si A6[0] != 0 y A6 += 1. */
uint32_t wipe_rows(uint8_t *img, uint32_t entry, uint32_t base, uint32_t groups, uint32_t rows,
                     uint32_t offset) {
    uint32_t p = base;
    uint32_t passes = 3;
    if (img[entry] != 0) {
        rows = (rows - 4) & 0xffff;
        passes = (passes - 1) & 0xffff;
    }
    uint32_t n = passes;
    for (;;) {
        wait_vbl_even(img);
        row_blit(img, p, groups, rows);
        p += 0xa0;
        rows = (rows - 1) & 0xffff;
        or_rows(img, groups, rows, p, offset);
        if (n == 0) break;
        n--;
    }
    if (img[entry] != 0)
        row_blit(img, p, groups, rows);
    return entry + 1;
}

/* FUN_00016c48 @ 0x16c48: wipe de texto (d4 = 3, x = 0x23, y = 0xad o 0xb1 segun
 * A6[1]); cae en el cuerpo 0x16c6c con D2 = 2, D3 = 8. Devuelve A6 = entrada+2
 * (0x16dbc + el addq final de 0x16c6c): el VM original continua 2 bytes mas alla. */
uint32_t wipe_b(uint8_t *img, uint32_t entry) {
    uint32_t y = 0xad;
    if (img[entry + 1] != 0)
        y += 4;
    uint32_t offset;
    uint32_t addr = text_transition(img, &entry, 0x23, y, 3, &offset);
    return wipe_rows(img, entry, addr, 2, 8, offset);
}

/* FUN_00016c00 @ 0x16c00: transicion de texto (d4 = 0x77); blitea un bloque y luego
 * prepara el revelado (0x12e2e) con x=0x1c,y=0x8b. Devuelve A6 = entrada+1 (0x16dbc). */
uint32_t wipe_a(uint8_t *img, uint32_t entry) {
    uint32_t offset;
    uint32_t addr = text_transition(img, &entry, 0x20, 0x8b, 0x77, &offset);
    or_rows(img, 3, 0x14, addr - 0x8000, offset);
    wr16(img + 0x1a56a, 1);
    img[0x1a577] = 0x11;
    reveal_region(img, 0x1c, 0x8b, 0x12, 0x28);
    return entry;
}

/* Bucle comun de los wipes con VBL (0x16ca6/0x16ce2/0x16d3c/0x16d78): una espera
 * (0x16d1e), copia (0x16b76), avance y OR con el descriptor (0x16b2e); D7+1 pasadas.
 * `mode`: 0 = A3 +0xa0 y D3-1 (0x16ca6), 1 = A3 -0xa0 y D3+1 (0x16ce2),
 * 2 = A5 +0x40 y D3-1 (0x16d3c), 3 = A5 -0x40 y D3+1 (0x16d78). */
static void wipe_vbl(uint8_t *img, uint32_t base, uint32_t offset, uint32_t groups, uint32_t rows,
                     uint32_t passes, int mode) {
    uint32_t p = base;
    uint32_t n = passes;
    for (;;) {
        wait_vbl_even(img);
        row_blit(img, p, groups, rows);
        if (mode == 0) {
            p += 0xa0;
            rows = (rows - 1) & 0xffff;
        } else if (mode == 1) {
            p -= 0xa0;
            rows = (rows + 1) & 0xffff;
        } else if (mode == 2) {
            offset += 0x40;
            rows = (rows - 1) & 0xffff;
        } else {
            offset -= 0x40;
            rows = (rows + 1) & 0xffff;
        }
        or_rows(img, groups, rows, p, offset);
        if (n == 0) break;
        n--;
    }
}

/* FUN_00016ca6 @ 0x16ca6: wipe hacia abajo (34 VBL), D2 = 4, D3 0x43->...
 * Devuelve A6 = entrada+1 (0x16dbc). */
uint32_t wipe_c(uint8_t *img, uint32_t entry) {
    uint32_t offset;
    uint32_t addr = text_transition(img, &entry, 0x1c, 0x6a, 0x76, &offset);
    wipe_vbl(img, addr, offset, 4, 0x43, 0x21, 0);
    return entry;
}

/* FUN_00016ce2 @ 0x16ce2: wipe hacia arriba (68 VBL), D2 = 4, D3 0->...
 * Devuelve A6 = entrada+1 (0x16dbc). */
uint32_t wipe_d(uint8_t *img, uint32_t entry) {
    uint32_t offset;
    uint32_t addr = text_transition(img, &entry, 0x1c, 0xae, 0x76, &offset);
    wipe_vbl(img, addr, offset, 4, 0, 0x43, 1);
    return entry;
}

/* FUN_00016d3c @ 0x16d3c: wipe con A5 +0x40 por pasada (16 VBL), A3 fijo.
 * Devuelve A6 = entrada+1 (0x16dbc). */
uint32_t wipe_e(uint8_t *img, uint32_t entry) {
    uint32_t offset;
    uint32_t addr = text_transition(img, &entry, 0x23, 0x12, 0x60, &offset);
    wipe_vbl(img, addr, offset, 1, 0x10, 0x0f, 2);
    return entry;
}

/* FUN_00016d78 @ 0x16d78: wipe con A5 -0x40 por pasada (15 VBL) desde 0x3c0.
 * Devuelve A6 = entrada+1 (0x16dbc). */
uint32_t wipe_f(uint8_t *img, uint32_t entry) {
    uint32_t offset;
    uint32_t addr = text_transition(img, &entry, 0x23, 0x12, 0x60, &offset);
    wipe_vbl(img, addr, 0x3c0, 1, 0, 0x0e, 3);
    return entry;
}

/* FUN_000141dc @ 0x141dc: run FUN_00014138 on A6+1, then the masked row blit
 * (FUN_0001215e) with the A0/D2/D3/D7 that FUN_00014138 left behind. */
void mask_restore_rows(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t);
/* 0x141dc — dispatch.c — el despacho de comandos y la VM de bytes del script. run_command @ 0x103a8 / run_command_keep_sp @ 0x103ae: leen `the_command` ($1a50e) y despac... */
void text_entry_blit(uint8_t *img, uint32_t entry) {
    uint32_t addr, w, h, phase;
    run_text_entry_ex(img, entry + 1, &addr, &w, &h, &phase, 0);
    mask_restore_rows(img, addr, w, h, phase);
}

/* FUN_000141ea @ 0x141ea: run FUN_00014138 on the entry at A6+1 (then A6 += 3, unused). */
void run_text_entry_1(uint8_t *img, uint32_t entry) {
    run_text_entry(img, entry + 1);
}

/* FUN_00011014 @ 0x11014: a bare `rts` (no-op stub in the original). */
void noop_11014(uint8_t *img) {
    (void)img;
}

/* FUN_00011016 @ 0x11016 (A4 = 0x8000) y FUN_00011020 @ 0x11020 (A4 = 0), cuerpo comun
 * FUN_00011026: avanza la entrada de texto de 0x28ad2 D4 posiciones, lee [rows][cols] y
 * dibuja con el blit de texto hacia delante (stride cols*2) restando A4 a la dirección. */
void text_blit_entry(uint8_t *img, uint32_t x, uint32_t y, uint32_t index, uint32_t base_off) {
    uint32_t entry = advance_text_entry(img, 0x28ad2, index);
    uint32_t phase;
    uint32_t addr = screen_address(img, x, y, &phase);
    addr -= base_off;
    uint32_t rows = img[entry++];
    uint32_t cols = img[entry++];
    uint32_t stride = (cols << 1) & 0xffff;
    wr32(img + 0x1a534, stride);
    text_blit_fwd(img, addr, entry, stride, cols, rows, phase);
}

/* 0x11016 — text blit ovl a */
void text_blit_ovl_a(uint8_t *img, uint32_t x, uint32_t y, uint32_t index) {
    text_blit_entry(img, x, y, index, 0x8000);
}

/* 0x11020 — wrapper: A4 = 0 (base de pantalla) y continua en 0x11026 (blit de la
 * tabla A_ANIM_TBL_28AD2). La busqueda en la tabla 0x1a8f0 es 0x1857c, no esta. */
void text_blit_ovl_b(uint8_t *img, uint32_t x, uint32_t y, uint32_t index) {
    text_blit_entry(img, x, y, index, 0);
}

/* FUN_00011030 @ 0x11030: cuerpo de 0x11026 sin el avance de entrada (A1 ya apunta a
 * [rows][cols]); direccion de pantalla menos A4 y blit hacia delante. */
void text_blit_body_a(uint8_t *img, uint32_t entry, uint32_t x, uint32_t y, uint32_t base_off) {
    uint32_t phase;
    uint32_t addr = screen_address(img, x, y, &phase);
    addr -= base_off;
    uint32_t rows = img[entry++];
    uint32_t cols = img[entry++];
    uint32_t stride = (cols << 1) & 0xffff;
    wr32(img + 0x1a534, stride);
    text_blit_fwd(img, addr, entry, stride, cols, rows, phase);
}

/* FUN_000110da @ 0x110da: como 0x11026 pero con el blit inverso (0x11102) y SIN
 * restar A4; D4 = indice de la entrada de texto en 0x28ad2. */
void text_blit_rev_entry(uint8_t *img, uint32_t index, uint32_t x, uint32_t y) {
    uint32_t entry = advance_text_entry(img, 0x28ad2, index);
    uint32_t phase;
    uint32_t addr = screen_address(img, x, y, &phase);
    uint32_t rows = img[entry++];
    uint32_t cols = img[entry++];
    uint32_t stride = (cols << 1) & 0xffff;
    wr32(img + 0x1a534, stride);
    text_blit_rev(img, addr, entry, stride, cols, rows, phase);
}

/* FUN_00010768 @ 0x10768: `rts` pelado (destino del modo sin overlay, $1acc6 == 0x87). */
void text_end(uint8_t *img) { (void)img; }

/* FUN_000122ee @ 0x122ee: espera un frame (0x13594) y dibuja el prompt parpadeante con el
 * overlay 0x11020 en (0x4b, 0x9b); alterna el flag 0x1a74e (el color 0x16/0x17). */
void wait_vbl(uint8_t *);
void text_blit_ovl_b(uint8_t *, uint32_t, uint32_t, uint32_t);
/* 0x122ee — prompt */
void prompt(uint8_t *img) {
    if (img[0x1acc6] == 0x87)
        return;
    wait_vbl(img);
    uint32_t color = img[0x1a74e] ? 0x17 : 0x16;
    text_blit_ovl_b(img, 0x4b, 0x9b, color);
    img[0x1a74e] = (uint8_t)~img[0x1a74e];
}

/* FUN_00012292 @ 0x12292: espera un clic (bit 2 de $1a456) leyendo frames con 0x13698 y
 * parpadeando el prompt (0x122ee) cada 16 frames. En modo $1acc6 == 0x87 (sin overlay)
 * sale por GEMDOS Cnecin (trap #1/$8). A5/A6 los preserva el `movem.l` de entrada. */
void wait_frame(uint8_t *);
void prompt(uint8_t *);
/* 0x12292 — prompt_wait */
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
            prompt(img);                        /* parpadeo cada 16 frames */
        wait_frame(img);
    }
    if (img[0x1a74e] != 0)
        prompt(img);
}

/* FUN_00016abe @ 0x16abe: rejilla de texto sobre la caja ($1ab40..43): 16dbc prepara
 * (d4=0x50) y por cada banda de 0x1e filas (d3 = alto/0x1e) blitea D2+1 grupos de
 * 0x1d filas con 0x16b2e. Devuelve A6 (avanzado por 16dbc). */
uint32_t text_transition(uint8_t *, uint32_t *, uint32_t, uint32_t, uint32_t,
                     uint32_t *);

/* 0x16abe — (cmd idx 10): rejilla de texto 0x16abe + espera de clic (0x167be) + repintados, y despacho por la tabla de objetos o por el umbral (0x9005) en la cola 0x15022. */
uint32_t grid_bands(uint8_t *img, uint32_t entry) {
    uint32_t x = img[A_DAT_1AB40];
    uint32_t y = img[A_DAT_1AB42];
    uint32_t offset;
    uint32_t addr = text_transition(img, &entry, x, y, 0x50, &offset);
    uint32_t groups = (((uint32_t)img[A_DAT_1AB41] - x) & 0xff) >> 2;
    uint32_t bands = ((uint32_t)img[A_DAT_1AB43] - y) & 0xff;
    bands = bands / 0x1e;
    uint32_t p = addr;
    groups = (groups - 1) & 0xffff;
    bands = (bands - 1) & 0xffff;
    for (;;) {
        uint32_t base = p;
        uint32_t cnt = groups;
        for (;;) {
            or_rows(img, 0, 0x1d, p, offset);
            p += 8;
            if (cnt == 0)
                break;
            cnt--;
        }
        p = (base + 0x12c0) & 0xffffffffu;
        if (bands == 0)
            break;
        bands--;
    }
    return entry;
}

/* FUN_00016b9c @ 0x16b9c: wipe grande de texto. Primer 0x16dbc (d4=3, x=0x23,
 * y=0xad) + 0x16b2e(2,7); segundo 0x16dbc (d4=0x77, x=0x20, y=0x5e) +
 * 0x16b2e(3,0x47); prompt (0x12292) y 72 pasadas {0x16d1e (VBL), 0x16b76 con
 * d3+1, 0x16b2e con d3, a3 += 0xa0}; cierra con 0x16b76(d3=0). Devuelve
 * A6 = entrada+1 (16dbc +1, subq -1, 16dbc +1). */
void prompt_wait(uint8_t *);
void wait_vbl_even(uint8_t *);
void row_blit(uint8_t *, uint32_t, uint32_t, uint32_t);

/* 0x16b9c — wipe 72 */
uint32_t wipe_72(uint8_t *img, uint32_t entry) {
    uint32_t offset;
    uint32_t addr = text_transition(img, &entry, 0x23, 0xad, 3, &offset);
    entry = (entry - 1) & 0xffffffff;
    or_rows(img, 2, 7, addr, offset);
    addr = text_transition(img, &entry, 0x20, 0x5e, 0x77, &offset);
    uint32_t rows = 0x47, groups = 3, p = addr;
    or_rows(img, groups, rows, p, offset);
    prompt_wait(img);
    for (;;) {
        wait_vbl_even(img);
        row_blit(img, p, groups, (rows + 1) & 0xffff);
        p = (p + 0xa0) & 0xffffffff;
        or_rows(img, groups, rows, p, offset);
        if (rows == 0)
            break;
        rows = (rows - 1) & 0xffff;
    }
    row_blit(img, p, groups, 0);
    return entry;
}
