/* text.c — capa de texto: fuente, medida, justificado y overlays.
 *
 *   print_char       @ 0x10838 : dibuja el glifo de D0 en (cursor_x, cursor_y) con la
 *                                fuente 6x4 en 0x1d202 y la tabla de color *(0x1a4d8);
 *                                avanza el cursor. La base es screen_address(x,y) menos
 *                                el draw base *(0x1a46a).
 *   print_word       @ 0x108a2 : dibuja la cadena en A5 hasta cur_str_end manejando los
 *                                flags 0x40/0x80/0xc0. Estado en D0 (1 = fin) y A5 por out.
 *   position_text    @ 0x10fc4 : salta (D4-4) entradas [u8 len] y fija cur_str_end.
 *   advance_text_entry @ 0x10f8e : avanza D4 entradas [word len] y fija cur_str_end.
 *   print_words_fit  @ 0x1090e : imprime desde A5 las palabras que caben en A_TEXT_BOX_W.
 *   measure_word     @ 0x10946 : mide una palabra del banco.
 *   measure_words    @ 0x10998 : cuenta palabras (D5) y la más ancha (D6).
 *   wrap_line        @ 0x109bc : word-wrap greedy al ancho de A_TEXT_BOX_W.
 *   print_line       @ 0x10b24 : mide/crece el ancho, imprime y rellena con espacios.
 *   print_spaces     @ 0x10b46 : imprime D6+1 espacios.
 *   text_index_arg   @ 0x10fb4 : position_text tomando el índice de A6[1] si D4 < 4.
 *   print_line_center@ 0x10aee : centra la línea de A5 y la imprime.
 *   overlay_a_draw   @ 0x109fe / overlay_b_draw @ 0x10a88 : overlays de texto A/B.
 *   overlay_a_frame  @ 0x10a54 / overlay_b_frame @ 0x10acc : marco de los overlays.
 *
 * Todo el efecto es la imagen (y los registros devueltos por out-param).
 */
#include <stdint.h>

#include "machine.h"
#include "addrs.h"
#include "kult_internal.h"

uint32_t screen_address(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t *d7);
void mask_restore_rows(uint8_t *img, uint32_t a0, uint32_t d2, uint32_t d3, uint32_t d7);

/* 0x10838 — dibuja el glifo de D0. */
void print_char(uint8_t *img, uint32_t code) {
    uint32_t ch = code & 0xff;
    uint32_t x = be16(img + A_CURSOR_TEXT_X);
    uint32_t y = be16(img + A_CURSOR_TEXT_Y);
    wr16(img + A_CURSOR_TEXT_X, (x + 1) & 0xffff);

    uint32_t phase;
    uint32_t dest = screen_address(img, x, y, &phase) - be32(img + A_SCREEN_DRAW_BASE);

    uint32_t font = A_FONT_DATA + (ch * 6);
    uint32_t tbl = be32(img + A_COLOR_TABLE);
    uint32_t mask = ror16(0x0fff, phase);
    uint32_t inv = (~mask) & 0xffff;

    for (int row = 0; row < 6; row++) {
        uint32_t g = img[font++];
        uint32_t bits = ror16(be16(img + tbl + g), phase);
        for (int w = 0; w < 4; w++) {
            uint32_t v = be16(img + dest);
            v = (v & mask) | (inv & bits);
            wr16(img + dest, v & 0xffff);
            bits = rol16(bits, 4);
            dest += 2;
        }
        dest += 0x98;
    }
}

/* 0x108a2 — imprime la palabra de A5 hasta cur_str_end. */
uint32_t print_word(uint8_t *img, uint32_t str, uint32_t *str_out) {
    uint32_t end = be32(img + A_CUR_STR_END);
    if ((img[str] & 0x3f) == 0)
        goto L_c4;
L_ac:
    if (str == end) {
        img[A_TEXT_END_FLAG] = 0xff;
        *str_out = str;
        return 1;
    }
    if ((img[str] & 0x3f) == 0)
        goto L_ee;
    print_char(img, img[str] & 0x3f);
L_c4:
    {
        uint32_t flags = img[str] & 0xc0;
        str++;
        if (flags == 0)
            goto L_ac;
        if (flags == 0x80) { print_char(img, 0x25); goto L_ac; }   /* añade 'E' */
        if (flags == 0x40) { *str_out = str; return 0; }             /* separador: para */
        print_char(img, 0x21);                                  /* añade 'A' */
        goto L_ac;
    }
L_ee:
    if ((img[str] & 0xc0) == 0)
        str++;
    *str_out = str;
    return 0;
}

/* 0x10fc4 — position_text. */
uint32_t position_text(uint8_t *img, uint32_t str, uint32_t index) {
    wr16(img + A_TEXT_INDEX, index & 0xffff);
    uint32_t n = (index - 5) & 0xffff;
    if ((index & 0xffff) >= 5) {
        do {
            str += img[str];
            n = (n - 1) & 0xffff;
        } while (n != 0xffff);
    }
    uint32_t entry = str;
    str += img[str];
    wr32(img + A_CUR_STR_END, str);
    return entry + 1;
}

/* 0x10f8e — avanza D4 entradas [word len] y fija cur_str_end. */
uint32_t advance_text_entry(uint8_t *img, uint32_t table, uint32_t count) {
    uint32_t n = count & 0xffff;
    if (n != 0) {
        n = (n - 1) & 0xffff;                       /* subq.w #1,count */
        do {
            table += be16(img + table);                   /* move.w (table),d5; adda.l d5,table */
            n = (n - 1) & 0xffff;                   /* dbf count */
        } while (n != 0xffff);
    }
    uint32_t end = table + be16(img + table);
    wr32(img + A_CUR_STR_END, end);
    return table + 2;
}

uint32_t measure_word(uint8_t *img, uint32_t str, uint32_t *end_out);

/* 0x1090e — imprime las palabras que caben en A_TEXT_BOX_W. */
uint32_t print_words_fit(uint8_t *img, uint32_t str, uint32_t ret,
                         uint32_t *str_out, uint32_t *width_out) {
    uint32_t width = be16(img + A_TEXT_BOX_W);
    for (;;) {
        uint32_t word_end = 0;
        uint32_t w = measure_word(img, str, &word_end);
        if (width < w) break;                          /* bcs: la palabra no cabe */
        width = (width - w) & 0xffff;
        uint32_t s = print_word(img, str, &str);
        if (s != 0) break;                          /* bne: alcanzado cur_str_end */
        if (width == 0) break;
        width = (width - 1) & 0xffff;
        print_char(img, 0);                         /* separador (espacio) */
    }
    *str_out = str;
    *width_out = width;
    return ret & 0xffff;
}

void measure_words(uint8_t *img, uint32_t str, uint32_t *count_out, uint32_t *width_out);
void print_spaces(uint8_t *img, uint32_t count);

/* 0x10b24 — mide la línea, ajusta el ancho y la imprime rellenando. */
uint32_t print_line(uint8_t *img, uint32_t str) {
    uint32_t nwords, width;
    measure_words(img, str, &nwords, &width);
    width = (width + 2) & 0xffff;
    if (width >= be16(img + A_TEXT_BOX_W))
        wr16(img + A_TEXT_BOX_W, width);
    uint32_t str_out, left;
    print_words_fit(img, str, 0, &str_out, &left);
    if (left != 0)
        print_spaces(img, (left - 1) & 0xffff);
    return str_out;
}

/* 0x10b46 — imprime D6+1 espacios. */
void print_spaces(uint8_t *img, uint32_t d6) {
    do {
        print_char(img, 0);
        d6 = (d6 - 1) & 0xffff;
    } while (d6 != 0xffff);
}

/* 0x10946 — mide una palabra. */
uint32_t measure_word(uint8_t *img, uint32_t a5, uint32_t *a4_out) {
    uint32_t end = be32(img + A_CUR_STR_END);
    uint32_t a4 = a5;
    uint32_t d0 = 0;
    if ((img[a4] & 0x3f) == 0) {
        d0 = 1;
        a4 += 1;
        if (a4 == end) { *a4_out = a4; return 1; }
    }
    for (;;) {
        if ((img[a4] & 0x3f) == 0) { *a4_out = a4; return d0; }
        d0 += 1;
        uint32_t c0 = img[a4] & 0xc0;
        if (c0 == 0x40) { a4 += 1; *a4_out = a4; return d0; }
        if (c0 != 0) d0 += 1;
        a4 += 1;
        if (a4 == end) { *a4_out = a4; return d0; }
    }
}

/* 0x10998 — cuenta palabras (D5) y la más ancha (D6). */
void measure_words(uint8_t *img, uint32_t a5, uint32_t *d5_out, uint32_t *d6_out) {
    uint32_t end = be32(img + A_CUR_STR_END);
    uint32_t d5 = 0, d6 = 0, a4 = a5;
    do {
        uint32_t d0 = measure_word(img, a5, &a4);
        d5 += 1;
        if (d6 < d0) d6 = d0;
        a5 = a4;
    } while (a4 != end);
    *d5_out = d5 & 0xffff;
    *d6_out = d6 & 0xffff;
}

/* 0x109bc — word-wrap greedy al ancho A_TEXT_BOX_W. */
void wrap_line(uint8_t *img, uint32_t str, uint32_t *lines_out, uint32_t *rem_out) {
    uint32_t end = be32(img + A_CUR_STR_END);
    uint32_t p = str;
    uint32_t lines = 1;
    uint32_t rem = be16(img + A_TEXT_BOX_W);
    for (;;) {
        str = p;
        if (str == end) break;
        uint32_t w = measure_word(img, str, &p);
        if (rem < w) {
            lines += 1;
            rem = (be16(img + A_TEXT_BOX_W) - w - 1) & 0xffff;
            continue;
        }
        rem = (rem - w) & 0xffff;
        if (rem == 0) {
            lines += 1;
            rem = (be16(img + A_TEXT_BOX_W) - w - 1) & 0xffff;
            continue;
        }
        rem = (rem - 1) & 0xffff;
    }
    *lines_out = lines & 0xffff;
    *rem_out = rem & 0xffff;
}

/* 0x10fb4 — position_text tomando el índice de A6[1] si D4 < 4. */
uint32_t text_index_arg(uint8_t *img, uint32_t str, uint32_t index, uint32_t *script) {
    if ((index & 0xffff) < 4) {
        uint32_t lo = (rol16(index & 0xffff, 8) & 0xff00u) | (img[*script + 1] & 0xff);
        index = (index & 0xffff0000u) | lo;
        *script += 1;
    }
    return position_text(img, str, index);
}

/* 0x10aee — centra la línea de A5 y cae en print_line. */
uint32_t print_line_center(uint8_t *img, uint32_t str) {
    uint32_t start = str;
    uint32_t total = 0, p = 0;
    do {
        uint32_t w = measure_word(img, str, &p);
        total = (total + w) & 0xffff;
        str = p;
    } while (p != be32(img + A_CUR_STR_END));
    str = start;
    uint32_t pad = (((be16(img + A_TEXT_BOX_W) - total) & 0xffff) >> 1) & 0xffff;
    if (pad != 0) {
        wr16(img + A_TEXT_BOX_W, (be16(img + A_TEXT_BOX_W) - pad) & 0xffff);
        print_spaces(img, (pad - 1) & 0xffff);
    }
    return print_line(img, str);
}

/* 0x109fe — overlay de texto A (banco 0x41b76). */
void overlay_a_draw(uint8_t *img, uint32_t script) {
    if (img[A_MODE_FLAG] == 0x87)
        return;
    img[A_TEXT_ROW] = img[A_TEXT_COL];
    wr32(img + A_SCREEN_DRAW_BASE, 0x8000);
    wr16(img + A_CURSOR_TEXT_X, be16(img + A_OVERLAY_X));
    wr16(img + A_CURSOR_TEXT_Y, be16(img + A_OVERLAY_Y));
    wr32(img + A_COLOR_TABLE, 0x1d3c2);
    uint32_t str = text_index_arg(img, 0x41b76, img[A_TEXT_SEL_A], &script);
    print_line_center(img, str);
}

/* 0x10a88 — overlay de texto B (banco 0x41586), posición fija. */
void overlay_b_draw(uint8_t *img, uint32_t script) {
    img[A_TEXT_ROW] = 0x20;
    wr32(img + A_SCREEN_DRAW_BASE, 0x8000);
    wr16(img + A_CURSOR_TEXT_X, 0x2f);
    wr16(img + A_CURSOR_TEXT_Y, 0xc1);
    wr32(img + A_COLOR_TABLE, 0x1d3c2);
    uint32_t str = text_index_arg(img, 0x41586, img[A_TEXT_SEL_B], &script);
    print_line_center(img, str);
}

/* 0x10a54 — marco del overlay A. */
void overlay_a_frame(uint8_t *img) {
    if (img[A_MODE_FLAG] == 0x87)
        return;
    uint32_t x = (be16(img + A_OVERLAY_X) - 1) & 0xffff;
    uint32_t y = (be16(img + A_OVERLAY_Y) - 2) & 0xffff;
    uint32_t phase;
    uint32_t dest = screen_address(img, x, y, &phase);
    uint32_t width = (img[A_TEXT_COL] + 2) & 0xffff;
    mask_restore_rows(img, dest, width, 9, phase);
}

/* 0x10acc — marco del overlay B, posición fija. */
void overlay_b_frame(uint8_t *img) {
    uint32_t phase;
    uint32_t dest = screen_address(img, (0x2f - 1) & 0xffff, (0xc1 - 2) & 0xffff, &phase);
    mask_restore_rows(img, dest, (0x20 + 2) & 0xffff, 9, phase);
}
