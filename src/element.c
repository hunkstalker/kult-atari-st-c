/* element.c — the element blit helpers around the hint-icon machinery.
 *
 * FUN_000126e0 @ 0x126e0: compone una rejilla de words en el descriptor 0xd8f0.
 * Borra los 0x627 longs del descriptor, precalcula las mascaras (0x12698) y luego,
 * por cada columna (D2) y fila (D3), lee una palabra del origen A1 (avanzando por
 * el stride 0x1a534), la rota por la fase D7 y la despliega en cuatro words que se
 * OR-ean en el descriptor. La fase avanza de 4 en 4 (nibble); al envolver, la base
 * del descriptor avanza 8 bytes. La funcion restaura A0/A1/D3/D7, asi que solo el
 * descriptor (y los globals de 0x12698) son efectos observables.
 *
 * FUN_0001276c @ 0x1276c: relleno de patron. Como el anterior pero el word origen
 * es D6 (constante) y escribe directamente en A0 con and/or (no acumula con OR).
 * Avanza 0xa0 por fila; A1 no se usa. Cierra con una espera de VBL (0x13594).
 *
 * FUN_00011ece @ 0x11ece y FUN_00011ee8 @ 0x11ee8: repiten D1+1 veces el blit de
 * filas 0x12466 incrementando cada vez el numero de filas D3 (empieza en 1) y
 * retrasando A1 0x40 (la primera) o A0 0xa0 (la segunda) por iteracion.
 */
#include <stdint.h>

#include "machine.h"
#include "addrs.h"

void blit_mask_setup(uint8_t *img, uint32_t d2, uint32_t d7);
void wait_vbl(uint8_t *img);
void blit_mask_rows(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t d3);

/* 0x126e0 — grid fill */
void grid_fill(uint8_t *img, uint32_t cols_in, uint32_t rows_in, uint32_t phase_in, uint32_t src) {
    uint32_t stride = be32(img + A_DAT_1A534);
    uint32_t dest = A_SAVE_DESC;
    for (int i = 0; i < 0x627; i++)
        wr32(img + dest + (uint32_t)i * 4, 0);

    blit_mask_setup(img, cols_in, phase_in);

    dest = A_SAVE_DESC;
    uint32_t col = (cols_in - 1) & 0xffff;
    uint32_t rows = (rows_in - 1) & 0xffff;
    uint32_t phase = phase_in & 0xffff;
    for (;;) {
        uint16_t mask = rotate_right16(0xf000, phase);
        uint32_t base = dest;
        uint32_t src_base = src;
        uint32_t r = rows;
        for (;;) {
            uint16_t word = rotate_right16(be16(img + src), phase);
            for (int k = 0; k < 4; k++) {
                wr16(img + dest, (uint16_t)(be16(img + dest) | (word & mask)));
                dest += 2;
                word = rotate_left16(word, 4);
            }
            dest += 0x38;
            src += stride;
            r = (r - 1) & 0xffff;
            if (r == 0xffff)
                break;
        }
        dest = base;
        src = src_base + 2;
        phase = (phase + 4) & 0xf;
        if (phase == 0)
            dest += 8;
        col = (col - 1) & 0xffff;
        if (col == 0xffff)
            break;
    }
}

/* 0x1276c — pattern fill */
void pattern_fill(uint8_t *img, uint32_t cols_in, uint32_t rows_in, uint32_t phase_in,
                 uint32_t pattern, uint32_t dest) {
    uint32_t col = (cols_in - 1) & 0xffff;
    uint32_t rows = (rows_in - 1) & 0xffff;
    uint32_t phase = phase_in & 0xffff;
    for (;;) {
        uint16_t mask = rotate_right16(0x0fff, phase);
        uint16_t inv = (uint16_t)~mask;
        uint32_t base = dest;
        uint32_t r = rows;
        for (;;) {
            uint16_t word = rotate_right16((uint16_t)pattern, phase);
            for (int k = 0; k < 4; k++) {
                wr16(img + dest, (uint16_t)((be16(img + dest) & mask) | (word & inv)));
                dest += 2;
                word = rotate_left16(word, 4);
            }
            dest += 0x98;
            r = (r - 1) & 0xffff;
            if (r == 0xffff)
                break;
        }
        dest = base;
        phase = (phase + 4) & 0xf;
        if (phase == 0)
            dest += 8;
        col = (col - 1) & 0xffff;
        if (col == 0xffff)
            break;
    }
    wait_vbl(img);
}

/* 0x11ece — row blit a */
void row_blit_a(uint8_t *img, uint32_t count, uint32_t dest, uint32_t src) {
    uint32_t rows = 1;
    for (;;) {
        blit_mask_rows(img, dest, src, rows);
        rows = (rows + 1) & 0xffff;
        src -= 0x40;
        count = (count - 1) & 0xffff;
        if (count == 0xffff)
            break;
    }
}

/* 0x11ee8 — row blit b */
void row_blit_b(uint8_t *img, uint32_t count, uint32_t dest, uint32_t src) {
    uint32_t rows = 1;
    for (;;) {
        blit_mask_rows(img, dest, src, rows);
        dest -= 0xa0;
        rows = (rows + 1) & 0xffff;
        count = (count - 1) & 0xffff;
        if (count == 0xffff)
            break;
    }
}

/* FUN_0001285a @ 0x1285a: busca en la tabla de 6 bytes 0x19e19 la entrada cuya clase
 * (byte0) coincide con (*0x1a772)[3] & 0xf8. Si la encuentra copia los bytes 4/5 a
 * 0x1ab4c/0x1ab4d, devuelve por out d0=byte2, d1=byte1, d2=0xff, d3=byte3, y marca
 * encontrado (el llamante 0x128a4 bifurca por el Z del `bsr`: encontrado -> Z=0).
 * Si no, cae en 0x10768 (rts) dejando los registros de entrada intactos. */
int elem_class_find(uint8_t *img, uint32_t ret0, uint32_t ret1, uint32_t ret2, uint32_t ret3,
                uint32_t *out) {
    uint32_t obj = be32(img + A_ELEM_CLASS);
    uint32_t cls = (ret2 & 0xffffff00u) | (uint32_t)(img[obj + 3] & 0xf8);
    uint32_t p = A_ELEM_TBL_19E19;
    for (;;) {
        if (img[p] == (uint8_t)(cls & 0xff)) {
            out[1] = (ret1 & 0xffffff00u) | img[p + 1];
            out[0] = (ret0 & 0xffffff00u) | img[p + 2];
            out[3] = (ret3 & 0xffffff00u) | img[p + 3];
            img[A_ELEM_1AB4C] = img[p + 4];
            img[A_ELEM_1AB4D] = img[p + 5];
            out[2] = (cls & 0xffffff00u) | 0xff;
            return 1;
        }
        p += 6;
        if (p == A_ELEM_TBL_19EA9)
            break;
    }
    out[0] = ret0; out[1] = ret1; out[2] = cls; out[3] = ret3;
    return 0;
}

/* FUN_0001296c @ 0x1296c: construye un bitmap en 0x3b1ae a partir de la entrada
 * (*A5)*10 de la tabla 0x1a252. Cabecera [alto][ancho], luego una fila superior de
 * `ancho` words, (alto-2) filas [izq][medio x(ancho-2)][der] y una fila inferior de
 * `ancho` words. Fija el stride 0x1a534 = ancho*2. A5 avanza 1 (el indice). */
void elem_bitmap_build(uint8_t *img, uint32_t *stream_io) {
    uint32_t stream = *stream_io;
    uint32_t idx = img[stream++];
    uint32_t p = A_ELEM_TBL_1A252 + (idx & 0xff) * 10;
    uint32_t dest = A_ELEM_BUILD;

    uint32_t width = img[p++];
    uint32_t height = img[p++];
    width &= 0xff;
    height &= 0xff;
    wr32(img + A_DAT_1A534, (uint32_t)(width << 1));
    img[dest++] = (uint8_t)height;
    img[dest++] = (uint8_t)width;

    uint32_t top = be16(img + p); p += 2;
    for (uint32_t i = (width - 1) & 0xffff;; i = (i - 1) & 0xffff) {
        wr16(img + dest, (uint16_t)top); dest += 2;
        if (i == 0) break;
    }
    uint32_t mid = be16(img + p); p += 2;
    uint32_t inner = (width - 3) & 0xffff;
    for (uint32_t row = (height - 3) & 0xffff;; row = (row - 1) & 0xffff) {
        wr16(img + dest, be16(img + p)); dest += 2;
        for (uint32_t i = inner;; i = (i - 1) & 0xffff) {
            wr16(img + dest, (uint16_t)mid); dest += 2;
            if (i == 0) break;
        }
        wr16(img + dest, be16(img + p + 2)); dest += 2;
        if (row == 0) break;
    }
    for (uint32_t i2 = (inner + 2) & 0xffff;; i2 = (i2 - 1) & 0xffff) {
        wr16(img + dest, (uint16_t)top); dest += 2;
        if (i2 == 0) break;
    }
    *stream_io = stream;
}

uint32_t walk_entry(uint8_t *img, uint32_t a4, uint32_t d1,
                      uint32_t *a4o, uint32_t *a5o);
uint32_t advance_text_entry(uint8_t *img, uint32_t a1, uint32_t d4);
void elem_blit(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t d2, uint32_t d3,
                 uint32_t a3);
void elem_blit_rev(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t d2, uint32_t d3,
                 uint32_t a3);
void hint_slot_alloc(uint8_t *img, uint32_t d4, uint32_t a0, uint32_t d7,
                 uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3);
uint32_t screen_address(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t *d7out);
void draw_selection_frame(uint8_t *img);

/* FUN_000129c8 @ 0x129c8: recorre la lista 0x2751e (via walk_entry con D1) y, por cada
 * bloque de 3 bytes [n][b1][b2] tras el indice, compone el bitmap 0x3b1ae con la fuente
 * de texto desplazada D4*(n) entradas (0x10f8e) y lo blitea en 0x3b1b0+b0*2 con 0x12ad6
 * o 0x12b04 segun b2 (bit7: stride negativo; bit6: variante invertida). Devuelve A1 =
 * 0x3b1ac (constante que el llamante usa para leer la cabecera). */
uint32_t elem_list_build(uint8_t *img, uint32_t index) {
    uint32_t end, p;
    walk_entry(img, A_ELEM_TBL_2751E, index, &end, &p);
    elem_bitmap_build(img, &p);
    for (;;) {
        uint32_t n = img[p];
        uint32_t str = advance_text_entry(img, A_ELEM_TBL_31072, n);
        uint32_t rows = img[str++];
        uint32_t cols = img[str++];
        uint32_t off = (((uint32_t)img[p + 2] << 8) | img[p + 1]) & 0x3fff;
        uint32_t addr = A_ELEM_BUILD + 2 + off * 2;
        uint32_t stride = be32(img + A_DAT_1A534);
        uint32_t stride2 = stride;
        if (img[p + 2] & 0x80) {
            addr += (uint32_t)((stride & 0xffff) * (rows & 0xffff));
            addr -= stride;
            stride2 = (uint32_t)(-stride);
        }
        if (img[p + 2] & 0x40)
            elem_blit_rev(img, addr, str, cols, rows, stride2);
        else
            elem_blit(img, addr, str, cols, rows, stride2);
        p += 3;
        if (p == end)
            break;
    }
    return 0x3b1ac;
}

/* FUN_000128a4 @ 0x128a4: handler de comando de elemento. A6 apunta a la entrada.
 * Si A6[1] == 0xff busca la clase por 0x1285a (con el marco 0x18d28 y el flag 0x1ad0a);
 * si no, toma x=A6[2], y=A6[3] y avanza 4. Luego fija 0x1a532/0x1a530/0x1ab4e, llama a
 * 0x129c8 para construir el bitmap, copia sus dos bytes de cabecera y crea la entrada de
 * icono (0x120a8) con la direccion de pantalla de (x,y). Devuelve A6 avanzado. */
/* Exposicion con las salidas de registro (A6/D0/D1) que 0x18a98 necesita: el
 * original deja D0/D1 = x/y del script (o los de 0x1285a en la rama 0xff) y A6
 * avanzado. El envoltorio clasico devuelve solo A6. */
uint32_t elem_command(uint8_t *img, uint32_t script, uint32_t *script_out, uint32_t *x_out,
                        uint32_t *y_out) {
    uint32_t px = 0, py = 0, h = 0;
    py = img[script + 1];
    if (py == 0xff) {
        script += 2;
        if (img[A_DAT_1AD0A] != 0) {
            *script_out = script; *x_out = px; *y_out = h;
            return script;
        }
        draw_selection_frame(img);
        uint32_t out[4];
        if (!elem_class_find(img, px, py, 0, h, out)) {
            *script_out = script; *x_out = px; *y_out = h;
            return script;
        }
        px = out[0]; py = out[1]; h = out[3];
    } else {
        px = img[script + 2];
        h = img[script + 3];
        script += 4;
    }
    *script_out = script; *x_out = px; *y_out = h;

    wr16(img + A_ELEM_1A532, px & 0xffff);
    wr16(img + A_ELEM_1A530, h & 0xffff);
    img[A_ELEM_1AB4E] = (uint8_t)(py & 0xff);

    uint32_t hdr = elem_list_build(img, (py - 1) & 0xffff);
    uint32_t v = 0;
    hdr += 2;
    v = img[hdr++];
    wr16(img + A_ELEM_1A52C, v & 0xffff);
    v = img[hdr++];
    wr16(img + A_ELEM_1A52E, v & 0xffff);
    wr32(img + A_ELEM_1A528, hdr);

    uint32_t x = be16(img + A_ELEM_1A532);
    uint32_t y = be16(img + A_ELEM_1A530);
    uint32_t d7;
    uint32_t addr = screen_address(img, x, y, &d7);
    uint32_t h2 = be16(img + A_ELEM_1A52C);
    uint32_t w2 = be16(img + A_ELEM_1A52E);
    hint_slot_alloc(img, 2, addr, d7, x, y, w2, h2);
    return script;
}

/* 0x128a4 — ---- handlers de comando 0x11xxx (encadenan la salida de 0x128a4) ---- 0x128a4 deja en el fichero de registros lo que sus llamantes encadenan: d2 = ancho (0x... */
uint32_t elem_command_wrap(uint8_t *img, uint32_t script) {
    uint32_t script_out, x_out, y_out;
    return elem_command(img, script, &script_out, &x_out, &y_out);
}

void reveal_region(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3);
void hint_select_off(uint8_t *img, uint32_t d0, uint32_t *o);
void hint_blit_entry(uint8_t *img, uint32_t a6, uint32_t *a6_out);
void blit_rect(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t d2, uint32_t d3,
                 uint32_t d7, uint32_t a3);
void blit_rect_vbl(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t d2, uint32_t d3,
                 uint32_t d7);

/* FUN_00012a4a @ 0x12a4a: construye un bitmap en 0x3b1ac desde la lista 0x23e9c
 * (walk_entry D1) y la tabla de texto 0x41fb0, bliteando cada bloque de 3 bytes
 * [n][b1][b2] en 0x3b1ac+b0*2 con 0x12ad6/0x12b04 (bit7) y stride fijo 0x28. Fija
 * 0x1a534=0x28, 0x1a530=0x20, 0x1a52e=0x14, 0x1a52c=0x3b y devuelve A1=0x3b1ac. */
uint32_t elem_bitmap_build2(uint8_t *img, uint32_t index) {
    uint32_t end, p;
    walk_entry(img, A_ELEM_TBL_23E9C, index, &end, &p);
    for (;;) {
        uint32_t t = img[p];
        uint32_t str = advance_text_entry(img, A_TEXT_TABLE, t & 0x7f);
        uint32_t rows = img[str++];
        uint32_t cols = img[str++];
        uint32_t off = (((uint32_t)img[p + 2] << 8) | img[p + 1]) & 0xffff;
        uint32_t addr = A_ELEM_BUILD2 + off * 2;
        if (t & 0x80)
            elem_blit_rev(img, addr, str, cols, rows, 0x28);
        else
            elem_blit(img, addr, str, cols, rows, 0x28);
        p += 3;
        if (p == end)
            break;
    }
    wr32(img + A_DAT_1A534, 0x28);
    wr16(img + A_ELEM_1A530, 0x20);
    wr16(img + A_ELEM_1A52E, 0x14);
    wr16(img + A_ELEM_1A52C, 0x3b);
    return A_ELEM_BUILD2;
}

/* FUN_00012d96 @ 0x12d96: dos blits de texto (0x12a4a + 0x10fec + 0x12510) a y=0x20,
 * en x=0x10 (a1+0x12) y x=0x2d, con 11 columnas y stride 0x28. */
void elem_blit_pair(uint8_t *img) {
    uint32_t bitmap = elem_bitmap_build2(img, 0);
    uint32_t phase;
    uint32_t addr = screen_address(img, 0x10, be16(img + A_ELEM_1A530), &phase);
    blit_rect(img, addr, bitmap + 0x12, 0xb, 0x3b, phase, 0x28);

    bitmap = elem_bitmap_build2(img, 1);
    addr = screen_address(img, 0x2d, be16(img + A_ELEM_1A530), &phase);
    blit_rect(img, addr, bitmap, 0xb, 0x3b, phase, 0x28);
}

/* FUN_00012dd8 @ 0x12dd8: como 0x12d96 pero con 0x124da (que espera VBL) y x=0x10/0x24,
 * 0x14 columnas. */
void elem_blit_pair_vbl(uint8_t *img) {
    uint32_t bitmap = elem_bitmap_build2(img, 0);
    uint32_t phase;
    uint32_t addr = screen_address(img, 0x10, be16(img + A_ELEM_1A530), &phase);
    blit_rect_vbl(img, addr, bitmap, 0x14, 0x3b, phase);

    bitmap = elem_bitmap_build2(img, 1);
    addr = screen_address(img, 0x24, be16(img + A_ELEM_1A530), &phase);
    blit_rect_vbl(img, addr, bitmap, 0x14, 0x3b, phase);
}

/* FUN_00012ea2 @ 0x12ea2: lee la caja 0x1ab40..0x1ab43, fija 0x1a56a=1, 0x1a577=0x11,
 * A6 += 1 y cae en 0x12e2e. */
uint32_t reveal_box_b(uint8_t *img, uint32_t script) {
    uint32_t x = img[A_DAT_1AB40];
    uint32_t y = img[A_DAT_1AB42];
    uint32_t w = (img[A_DAT_1AB41] - x) & 0xffff;
    uint32_t h = (img[A_DAT_1AB43] - y) & 0xffff;
    wr16(img + A_ANIM_1A56A, 1);
    img[A_ANIM_1A577] = 0x11;
    reveal_region(img, x, y, w, h);
    return script + 1;
}

/* FUN_00012e08 @ 0x12e08: si 0x1a570 != 0 cae en 0x1222e; si no, selecciona el icono
 * A6[1] (0x120e4), fija 0x1a577=0x11, A6 += 2 y 0x1a56a=0x64 y cae en 0x12e2e. */
uint32_t reveal_box_a(uint8_t *img, uint32_t script) {
    if (img[A_SKIP_TRANSITION] != 0) {
        uint32_t script_out;
        hint_blit_entry(img, script, &script_out);
        return script_out;
    }
    uint32_t out[8];
    hint_select_off(img, img[script + 1], out);
    img[A_ANIM_1A577] = 0x11;
    script += 2;
    wr16(img + A_ANIM_1A56A, 0x64);
    reveal_region(img, out[3], out[4], out[5], out[6]);
    return script;
}



/* FUN_000129d6 @ 0x129d6: recorre la lista de bloques de 3 bytes en [A5,A4): cada
 * bloque elige la entrada (byte 0 & 0x7f) de 0x31072 (0x10f8e), lee [filas][cols] y
 * un offset de 14 bits (bytes 2 y 1, mascarado 0x3fff; bit7 de +2 invierte, bit6
 * reblitea hacia atras 0x12b04) y blitea en 0x3b1b0+offset*2 con stride 0x1a534. */
uint32_t advance_text_entry(uint8_t *img, uint32_t a1, uint32_t d4);
void elem_blit(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t d2, uint32_t d3,
                 uint32_t a3);
void elem_blit_rev(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t d2, uint32_t d3,
                 uint32_t a3);

/* 0x129d6 — text block build */
void text_block_build(uint8_t *img, uint32_t p, uint32_t end) {
    for (;;) {
        uint32_t n = img[p];
        uint32_t str = advance_text_entry(img, 0x31072, n);
        uint32_t rows = img[str++];
        uint32_t cols = img[str++];
        uint32_t off = (((uint32_t)img[p + 2] << 8) | img[p + 1]) & 0x3fffu;
        uint32_t addr = 0x3b1b0 + off * 2;
        uint32_t stride = be32(img + A_DAT_1A534);
        if (img[p + 2] & 0x80) {
            uint32_t tmp = (stride * rows) & 0xffffffffu;
            addr = (addr + tmp - stride) & 0xffffffffu;
            stride = ((~stride) + 1) & 0xffffffffu;
        }
        if (img[p + 2] & 0x40)
            elem_blit_rev(img, addr, str, cols, rows, stride);
        else
            elem_blit(img, addr, str, cols, rows, stride);
        p += 3;
        if (p == end)
            break;
    }
}

/* FUN_00012ce4 @ 0x12ce4: ciclo de "modo/vista" $1acca (0..3). Cada rama fija
 * $1acc6, corre 0x14496 y pinta con los blitters 111b6/114ba/117fe/11afa (o sus
 * envoltorios). Los casos 1/3 guardan el registro final de los blitters en los
 * bloques 0x1a82e/0x1a83e (movem.l d0/d2/a0-a1), que los casos 0/2 recargan.
 * Frontera: 0x12ce4..0x12d94/0x12c06/0x12c6c/0x12ce2; el rts de la rama 3 es
 * 0x12d94. Devuelve A6 = entry+1. */
void sound_play_a(uint8_t *img, uint32_t d4);
void sound_stop(uint8_t *img);
uint32_t elem_bitmap_build2(uint8_t *img, uint32_t d1);
void blit_rect_vbl(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t d2, uint32_t d3,
                 uint32_t d7);
void overlay_compose_planes_wrap(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3,
                 uint32_t a0, uint32_t a1);
void overlay_compose_planes32_wrap(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3,
                 uint32_t d6, uint32_t a0, uint32_t a1);
void overlay_compose_planes(uint8_t *img, uint32_t d0in, uint32_t d1in, uint32_t d2in,
                 uint32_t d3in, uint32_t a0in, uint32_t a1in);
void overlay_compose_planes32(uint8_t *img, uint32_t d0in, uint32_t d1in, uint32_t d2in,
                 uint32_t d3in, uint32_t d6in, uint32_t a0in, uint32_t a1in);
void mask_restore_rows(uint8_t *img, uint32_t a0, uint32_t d2, uint32_t d3, uint32_t d7);
void overlay_compose_masked(uint8_t *img, uint32_t d0in, uint32_t d1in, uint32_t d2in,
                 uint32_t d3in, uint32_t a0in, uint32_t a1in);
void overlay_compose_shift(uint8_t *img, uint32_t d0in, uint32_t d1in, uint32_t d2in,
                 uint32_t d3in, uint32_t a0in, uint32_t a1in);
void scene_mode_load(uint8_t *img, uint32_t *a4_out, uint32_t *a5_out);
void overlay_compose_masked_wrap(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3,
                 uint32_t a0);
void overlay_compose_shift_wrap(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3,
                 uint32_t a0);
uint32_t scene_mode_load_wrap(uint8_t *img);

/* 0x12ce4 — ciclo de modo/vista $1acca (overlays del retrato) */
uint32_t view_mode_cycle(uint8_t *img, uint32_t script) {
    sound_play_a(img, 0x1d);
    uint32_t ret = script + 1;
    uint32_t mode = (img[0x1acca] + 1) & 3;
    img[0x1acca] = (uint8_t)mode;

    if (mode == 1) {
        uint32_t m = 0x09;
        if (img[0x1acc6] != 0x18)
            m = 0x66;
        img[0x1acc6] = (uint8_t)m;
        uint32_t end, p;
        scene_mode_load(img, &end, &p);

        uint32_t bitmap = elem_bitmap_build2(img, 0);
        bitmap = (bitmap + be32(img + 0x1a534) - 2) & 0xffffffffu;
        wr16(img + 0x1a532, 0x10);
        uint32_t phase;
        uint32_t addr = screen_address(img, 0x10, 0x20, &phase);
        blit_rect_vbl(img, addr, bitmap, 1, 0x3b, phase);
        bitmap = (bitmap - 2) & 0xffffffffu;
        /* el original llega con D0=0x07fd0000|fase y D2=0xffff0001 (valores de
         * 14496/12a4a/124da, independientes del contenido de pantalla) */
        uint32_t d0in = 0x07fd0000u | (phase & 0xffff);
        uint32_t d2in = 0xffff0001u;
        overlay_compose_planes_wrap(img, d0in, 0x0a, d2in, 0x3b, addr, bitmap);
        {
            uint32_t c = 0, k = 0x27;
            for (;;) {
                if ((k & 3) == 0) c++;
                if (k == 0) break;
                k = (k - 1) & 0xffff;
            }
            wr32(img + 0x1a82e, d0in);
            wr32(img + 0x1a832, (d2in & 0xffff0000u) | ((5 + 0x28) & 0xffff));
            wr32(img + 0x1a836, addr);
            wr32(img + 0x1a83a, (bitmap - 2 * c) & 0xffffffffu);
        }

        bitmap = elem_bitmap_build2(img, 1);
        wr16(img + 0x1a532, 0x37);
        addr = screen_address(img, 0x37, 0x20, &phase);
        blit_rect_vbl(img, addr, bitmap, 1, 0x3b, phase);
        if (phase == 0) {
            phase = 0x0f;
            addr = (addr - 8) & 0xffffffffu;
        } else {
            phase = (phase - 1) & 0xffff;
        }
        bitmap = (bitmap + 2) & 0xffffffffu;
        /* D0=0x45610000|fase2, D2=0xffff0001; D6=0x05010506 (constantes medidas) */
        uint32_t d0b = 0x45610000u | (phase & 0xffff);
        uint32_t d2b = 0xffff0001u;
        overlay_compose_planes32_wrap(img, d0b, 0x0a, d2b, 0x3b, 0x05010506u, addr, bitmap);
        {
            uint32_t lo = d0b & 0xffff, wraps = 0;
            for (uint32_t k = 0; k < 0x28; k++) {
                if (lo == 0) { lo = 0xf; wraps++; }
                else lo = (lo - 1) & 0xffff;
            }
            uint32_t c = 0, k = 0x27;
            for (;;) {
                if ((k & 3) == 0) c++;
                if (k == 0) break;
                k = (k - 1) & 0xffff;
            }
            wr32(img + 0x1a83e, (d0b & 0xffff0000u) | lo);
            wr32(img + 0x1a842, (d2b & 0xffff0000u) | ((5 + 0x28) & 0xffff));
            wr32(img + 0x1a846, (addr - 8 * wraps) & 0xffffffffu);
            wr32(img + 0x1a84a, (bitmap + 2 * c) & 0xffffffffu);
        }
        sound_stop(img);
        return ret;
    }
    if (mode == 2) {
        uint32_t m = 0x5f;
        if (img[0x1acc6] != 0x09)
            m = 0x67;
        img[0x1acc6] = (uint8_t)m;
        uint32_t end, p;
        scene_mode_load(img, &end, &p);

        (void)elem_bitmap_build2(img, 0);
        uint32_t b0 = be32(img + 0x1a82e), b2 = be32(img + 0x1a832);
        uint32_t ba0 = be32(img + 0x1a836), ba1 = be32(img + 0x1a83a);
        overlay_compose_planes(img, b0, 0x23, b2, 0x3b, ba0, ba1);

        (void)elem_bitmap_build2(img, 1);
        b0 = be32(img + 0x1a83e); b2 = be32(img + 0x1a842);
        ba0 = be32(img + 0x1a846); ba1 = be32(img + 0x1a84a);
        overlay_compose_planes32(img, b0, 0x23, b2, 0x3b, 0xff34u, ba0, ba1);

        sound_play_a(img, 0x1e);
        return ret;
    }
    if (mode == 0) {
        uint32_t m = 0x18;
        if (img[0x1acc6] != 0x09)
            m = 0x1e;
        img[0x1acc6] = (uint8_t)m;
        uint32_t end, p;
        scene_mode_load(img, &end, &p);
        uint32_t b0 = be32(img + 0x1a83e), b2 = be32(img + 0x1a842);
        uint32_t ba0 = be32(img + 0x1a846), ba1 = be32(img + 0x1a84a);
        overlay_compose_masked(img, b0, 0x2b, b2, 0x3b, ba0, ba1);
        mask_restore_rows(img, ba0, 0x14, 0x3b, b0 & 0xffff);
        b0 = be32(img + 0x1a82e); b2 = be32(img + 0x1a832);
        ba0 = be32(img + 0x1a836); ba1 = be32(img + 0x1a83a);
        overlay_compose_shift(img, b0, 0x2b, b2, 0x3b, ba0, ba1);
        mask_restore_rows(img, ba0, 0x14, 0x3b, b0 & 0xffff);
        sound_stop(img);
        return ret;
    }
    if (mode == 3) {
        uint32_t m = 0x09;
        if (img[0x1acc6] != 0x5f)
            m = 0x66;
        img[0x1acc6] = (uint8_t)m;
        uint32_t end, p;
        scene_mode_load(img, &end, &p);
        wr32(img + 0x1a534, 0x28);

        uint32_t phase;
        uint32_t a0a = screen_address(img, 0x24, 0x20, &phase);
        overlay_compose_masked_wrap(img, phase, 0x09, 0x14, 0x3b, a0a);
        /* registro final de 117de: D0=(mode&0xf+0x24)&0xf, D2=0x50-0x24,
         * A0/A1 con +8 por cada vuelta del nibble (2), A1 = A0-0x8000 */
        wr32(img + 0x1a83e, (((phase & 0xf) + 0x24) & 0xf) | (phase & 0xffffffF0u));
        wr32(img + 0x1a842, (((0x14 << 2) - 0x24) & 0xffff));
        wr32(img + 0x1a846, (a0a + 0x10) & 0xffffffffu);
        wr32(img + 0x1a84a, ((a0a - 0x8000) + 0x10) & 0xffffffffu);

        uint32_t a0b = screen_address(img, 0x10, 0x20, &phase);
        overlay_compose_shift_wrap(img, phase, 0x09, 0x14, 0x3b, a0b);
        wr32(img + 0x1a82e, phase & 0xffffffffu);
        wr32(img + 0x1a832, (((0x14 << 2) - 0x24) & 0xffff));
        wr32(img + 0x1a836, a0b);
        wr32(img + 0x1a83a, (a0b - 0x8000) & 0xffffffffu);
        sound_stop(img);
        return ret;
    }
    return ret;
}
