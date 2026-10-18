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
void print_char(uint8_t *img, uint32_t d0) {
    uint32_t ch = d0 & 0xff;
    uint32_t x = be16(img + A_CURSOR_TEXT_X);
    uint32_t y = be16(img + A_CURSOR_TEXT_Y);
    wr16(img + A_CURSOR_TEXT_X, (x + 1) & 0xffff);

    uint32_t d7;
    uint32_t a0 = screen_address(img, x, y, &d7) - be32(img + A_SCREEN_DRAW_BASE);

    uint32_t font = A_FONT_DATA + (ch * 6);
    uint32_t tbl = be32(img + A_COLOR_TABLE);
    uint32_t d1 = ror16(0x0fff, d7);
    uint32_t d3 = (~d1) & 0xffff;

    for (int row = 0; row < 6; row++) {
        uint32_t g = img[font++];
        uint32_t d2 = ror16(be16(img + tbl + g), d7);
        for (int w = 0; w < 4; w++) {
            uint32_t v = be16(img + a0);
            v = (v & d1) | (d3 & d2);
            wr16(img + a0, v & 0xffff);
            d2 = rol16(d2, 4);
            a0 += 2;
        }
        a0 += 0x98;
    }
}

/* 0x108a2 — imprime la palabra de A5 hasta cur_str_end. */
uint32_t print_word(uint8_t *img, uint32_t a5, uint32_t *a5_out) {
    uint32_t end = be32(img + A_CUR_STR_END);
    if ((img[a5] & 0x3f) == 0)
        goto L_c4;
L_ac:
    if (a5 == end) {
        img[A_TEXT_END_FLAG] = 0xff;
        *a5_out = a5;
        return 1;
    }
    if ((img[a5] & 0x3f) == 0)
        goto L_ee;
    print_char(img, img[a5] & 0x3f);
L_c4:
    {
        uint32_t d1 = img[a5] & 0xc0;
        a5++;
        if (d1 == 0)
            goto L_ac;
        if (d1 == 0x80) { print_char(img, 0x25); goto L_ac; }   /* añade 'E' */
        if (d1 == 0x40) { *a5_out = a5; return 0; }             /* separador: para */
        print_char(img, 0x21);                                  /* añade 'A' */
        goto L_ac;
    }
L_ee:
    if ((img[a5] & 0xc0) == 0)
        a5++;
    *a5_out = a5;
    return 0;
}

/* 0x10fc4 — position_text. */
uint32_t position_text(uint8_t *img, uint32_t a5, uint32_t d4) {
    wr16(img + A_TEXT_INDEX, d4 & 0xffff);
    uint32_t n = (d4 - 5) & 0xffff;
    if ((d4 & 0xffff) >= 5) {
        do {
            a5 += img[a5];
            n = (n - 1) & 0xffff;
        } while (n != 0xffff);
    }
    uint32_t a2 = a5;
    a5 += img[a5];
    wr32(img + A_CUR_STR_END, a5);
    return a2 + 1;
}

/* 0x10f8e — avanza D4 entradas [word len] y fija cur_str_end. */
uint32_t advance_text_entry(uint8_t *img, uint32_t a1, uint32_t d4) {
    uint32_t n = d4 & 0xffff;
    if (n != 0) {
        n = (n - 1) & 0xffff;                       /* subq.w #1,d4 */
        do {
            a1 += be16(img + a1);                   /* move.w (a1),d5; adda.l d5,a1 */
            n = (n - 1) & 0xffff;                   /* dbf d4 */
        } while (n != 0xffff);
    }
    uint32_t end = a1 + be16(img + a1);
    wr32(img + A_CUR_STR_END, end);
    return a1 + 2;
}

uint32_t measure_word(uint8_t *img, uint32_t a5, uint32_t *a4_out);

/* 0x1090e — imprime las palabras que caben en A_TEXT_BOX_W. */
uint32_t print_words_fit(uint8_t *img, uint32_t a5, uint32_t d0,
                         uint32_t *a5_out, uint32_t *d6_out) {
    uint32_t d6 = be16(img + A_TEXT_BOX_W);
    for (;;) {
        uint32_t a4 = 0;
        uint32_t w = measure_word(img, a5, &a4);
        if (d6 < w) break;                          /* bcs: la palabra no cabe */
        d6 = (d6 - w) & 0xffff;
        uint32_t s = print_word(img, a5, &a5);
        if (s != 0) break;                          /* bne: alcanzado cur_str_end */
        if (d6 == 0) break;
        d6 = (d6 - 1) & 0xffff;
        print_char(img, 0);                         /* separador (espacio) */
    }
    *a5_out = a5;
    *d6_out = d6;
    return d0 & 0xffff;
}

void measure_words(uint8_t *img, uint32_t a5, uint32_t *d5_out, uint32_t *d6_out);
void print_spaces(uint8_t *img, uint32_t d6);

/* 0x10b24 — mide la línea, ajusta el ancho y la imprime rellenando. */
uint32_t print_line(uint8_t *img, uint32_t a5) {
    uint32_t d5, d6;
    measure_words(img, a5, &d5, &d6);
    d6 = (d6 + 2) & 0xffff;
    if (d6 >= be16(img + A_TEXT_BOX_W))
        wr16(img + A_TEXT_BOX_W, d6);
    uint32_t a5o, left;
    print_words_fit(img, a5, 0, &a5o, &left);
    if (left != 0)
        print_spaces(img, (left - 1) & 0xffff);
    return a5o;
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
void wrap_line(uint8_t *img, uint32_t a5, uint32_t *d5_out, uint32_t *d6_out) {
    uint32_t end = be32(img + A_CUR_STR_END);
    uint32_t a4 = a5;
    uint32_t d5 = 1;
    uint32_t d6 = be16(img + A_TEXT_BOX_W);
    for (;;) {
        a5 = a4;
        if (a5 == end) break;
        uint32_t d0 = measure_word(img, a5, &a4);
        if (d6 < d0) {
            d5 += 1;
            d6 = (be16(img + A_TEXT_BOX_W) - d0 - 1) & 0xffff;
            continue;
        }
        d6 = (d6 - d0) & 0xffff;
        if (d6 == 0) {
            d5 += 1;
            d6 = (be16(img + A_TEXT_BOX_W) - d0 - 1) & 0xffff;
            continue;
        }
        d6 = (d6 - 1) & 0xffff;
    }
    *d5_out = d5 & 0xffff;
    *d6_out = d6 & 0xffff;
}

/* 0x10fb4 — position_text tomando el índice de A6[1] si D4 < 4. */
uint32_t text_index_arg(uint8_t *img, uint32_t a5, uint32_t d4, uint32_t *a6) {
    if ((d4 & 0xffff) < 4) {
        uint32_t lo = (rol16(d4 & 0xffff, 8) & 0xff00u) | (img[*a6 + 1] & 0xff);
        d4 = (d4 & 0xffff0000u) | lo;
        *a6 += 1;
    }
    return position_text(img, a5, d4);
}

/* 0x10aee — centra la línea de A5 y cae en print_line. */
uint32_t print_line_center(uint8_t *img, uint32_t a5) {
    uint32_t a5_in = a5;
    uint32_t total = 0, a4 = 0;
    do {
        uint32_t w = measure_word(img, a5, &a4);
        total = (total + w) & 0xffff;
        a5 = a4;
    } while (a4 != be32(img + A_CUR_STR_END));
    a5 = a5_in;
    uint32_t d7 = (((be16(img + A_TEXT_BOX_W) - total) & 0xffff) >> 1) & 0xffff;
    if (d7 != 0) {
        wr16(img + A_TEXT_BOX_W, (be16(img + A_TEXT_BOX_W) - d7) & 0xffff);
        print_spaces(img, (d7 - 1) & 0xffff);
    }
    return print_line(img, a5);
}

/* 0x109fe — overlay de texto A (banco 0x41b76). */
void overlay_a_draw(uint8_t *img, uint32_t a6) {
    if (img[A_MODE_FLAG] == 0x87)
        return;
    img[A_TEXT_ROW] = img[A_TEXT_COL];
    wr32(img + A_SCREEN_DRAW_BASE, 0x8000);
    wr16(img + A_CURSOR_TEXT_X, be16(img + A_OVERLAY_X));
    wr16(img + A_CURSOR_TEXT_Y, be16(img + A_OVERLAY_Y));
    wr32(img + A_COLOR_TABLE, 0x1d3c2);
    uint32_t a5 = text_index_arg(img, 0x41b76, img[A_TEXT_SEL_A], &a6);
    print_line_center(img, a5);
}

/* 0x10a88 — overlay de texto B (banco 0x41586), posición fija. */
void overlay_b_draw(uint8_t *img, uint32_t a6) {
    img[A_TEXT_ROW] = 0x20;
    wr32(img + A_SCREEN_DRAW_BASE, 0x8000);
    wr16(img + A_CURSOR_TEXT_X, 0x2f);
    wr16(img + A_CURSOR_TEXT_Y, 0xc1);
    wr32(img + A_COLOR_TABLE, 0x1d3c2);
    uint32_t a5 = text_index_arg(img, 0x41586, img[A_TEXT_SEL_B], &a6);
    print_line_center(img, a5);
}

/* 0x10a54 — marco del overlay A. */
void overlay_a_frame(uint8_t *img) {
    if (img[A_MODE_FLAG] == 0x87)
        return;
    uint32_t d0 = (be16(img + A_OVERLAY_X) - 1) & 0xffff;
    uint32_t d1 = (be16(img + A_OVERLAY_Y) - 2) & 0xffff;
    uint32_t d7;
    uint32_t a0 = screen_address(img, d0, d1, &d7);
    uint32_t d2 = (img[A_TEXT_COL] + 2) & 0xffff;
    mask_restore_rows(img, a0, d2, 9, d7);
}

/* 0x10acc — marco del overlay B, posición fija. */
void overlay_b_frame(uint8_t *img) {
    uint32_t d7;
    uint32_t a0 = screen_address(img, (0x2f - 1) & 0xffff, (0xc1 - 2) & 0xffff, &d7);
    mask_restore_rows(img, a0, (0x20 + 2) & 0xffff, 9, d7);
}
