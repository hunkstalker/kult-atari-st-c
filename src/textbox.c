/* textbox.c — el cuerpo de la caja de texto.
 *
 * FUN_00010e66 @ 0x10e66: dibuja el marco (0x12ffa/0x13216), coloca el cursor en la
 * primera línea y, para cada línea que quepa, imprime ';' + la línea (0x10b24) + '<'
 * hasta que print_word señale el final de la cadena ($1a508 = 0xff). Después pinta el
 * borde inferior. A5 lleva el puntero al banco de texto (lo fija position_text).
 */
#include <stdint.h>

#include "machine.h"
#include "addrs.h"

void box_hline(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3);
void box_vline(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3);
uint32_t print_line(uint8_t *img, uint32_t a5);
void print_char(uint8_t *img, uint32_t d0);

void text_box_body(uint8_t *img, uint32_t a5) {
    wr32(img + A_COLOR_TABLE, 0x1d3a2);
    wr32(img + A_SCREEN_DRAW_BASE, 0);

    uint32_t d0 = (be16(img + A_TEXT_BOX_W) + 2) & 0xffff;
    d0 = (d0 << 2) & 0xffff;
    d0 = (d0 - 2) & 0xffff;
    uint32_t d3 = d0;
    d0 = be16(img + A_TEXT_BOX_X);
    uint32_t d1 = be16(img + A_TEXT_BOX_Y);
    d0 = ((d0 << 2) + 2) & 0xffff;
    uint32_t d2 = 6;
    uint32_t d7 = d3;
    wr16(img + A_TEXT_BOX_SPAN, d7);
    d3 = (d3 - 2) & 0xffff;
    box_hline(img, d0, d1, d2, d3);                 /* borde superior */

    d0 = (d0 - 1) & 0xffff;
    d2 = 0xd;
    for (uint32_t d6 = 2; ; d6 = (d6 - 1) & 0xffff) {
        d1 = (d1 + 1) & 0xffff;
        d3 = d7;
        box_hline(img, d0, d1, d2, d3);
        if (d6 == 0) break;
    }

    d1 = (d1 - 1) & 0xffff;
    d0 = (d0 - 1) & 0xffff;
    d2 = 6;
    d3 = 2;
    box_vline(img, d0, d1, d2, d3);                 /* esquina sup. izq. */
    d1 = (d1 - 1) & 0xffff;
    d0 = (d0 + 1) & 0xffff;
    d3 = 1;
    box_vline(img, d0, d1, d2, d3);
    d0 = (d0 + d7 - 1) & 0xffff;
    d3 = 1;
    box_vline(img, d0, d1, d2, d3);
    d0 = (d0 + 1) & 0xffff;
    d1 = (d1 + 1) & 0xffff;
    d3 = 2;
    box_vline(img, d0, d1, d2, d3);                 /* esquina sup. der. */

    d0 = be16(img + A_TEXT_BOX_X);
    d1 = (be16(img + A_TEXT_BOX_Y) + 4) & 0xffff;
    wr16(img + A_CURSOR_TEXT_X, d0);
    wr16(img + A_CURSOR_TEXT_Y, d1);
    img[A_TEXT_END_FLAG] = 0;

    do {
        print_char(img, 0x3b);                      /* ';' */
        a5 = print_line(img, a5);
        print_char(img, 0x3c);                      /* '<' */
        d1 = (d1 + 6) & 0xffff;
        wr16(img + A_CURSOR_TEXT_Y, d1);
        wr16(img + A_CURSOR_TEXT_X, d0);
    } while (img[A_TEXT_END_FLAG] == 0);

    d7 = be16(img + A_TEXT_BOX_SPAN);
    d0 = ((d0 << 2) + 1) & 0xffff;
    d2 = 0xd;
    d3 = d7;
    box_hline(img, d0, d1, d2, d3);                 /* borde inferior */
    d3 = 1;
    d2 = 6;
    box_vline(img, d0, d1, d2, d3);
    d1 = (d1 + 1) & 0xffff;
    d0 = (d0 + 1) & 0xffff;
    d3 = (d7 - 2) & 0xffff;
    box_hline(img, d0, d1, d2, d3);
    d1 = (d1 - 1) & 0xffff;
    d0 = (d0 - 1) & 0xffff;
    d3 = 1;
    box_vline(img, d0, d1, d2, d3);
    d0 = (d0 + d7 - 1) & 0xffff;
    d3 = 1;
    box_vline(img, d0, d1, d2, d3);
}

/* FUN_00010440 @ 0x10440: orquestador del comando de caja de texto (nibble 0x9000).
 * position_text (índice = cmd & 0x3ff) sobre el banco 0x3ec8a; mide (0x10998), ajusta
 * el ancho (max 0x14 si hay >= 5 palabras), justifica (0x109bc), calcula la geometría a
 * partir del cursor (0x1a450/0x1a452), salva el rect (0x1258a), dibuja el cuerpo
 * (0x10e66), espera (0x12292) y restaura (0x124f2). */
uint32_t position_text(uint8_t *img, uint32_t a5, uint32_t d4);
void measure_words(uint8_t *img, uint32_t a5, uint32_t *d5_out, uint32_t *d6_out);
void wrap_line(uint8_t *img, uint32_t a5, uint32_t *d5_out, uint32_t *d6_out);
uint32_t screen_address(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t *d7out);
uint32_t save_rect(uint8_t *img, uint32_t d2, uint32_t d3, uint32_t a0, uint32_t d7);
void text_box_body(uint8_t *img, uint32_t a5);
void prompt_wait(uint8_t *img);
void restore_rect(uint8_t *img);

/* Cola comun de 0x1042c/0x10440: mide la caja (0x10998/0x109bc), fija 0x1a512/0x1a514,
 * calcula la posicion y dibuja (0x1258a + 0x10e66 + 0x12292 + 0x124f2). */
static void textbox_tail(uint8_t *img, uint32_t a5) {
    uint32_t d5, d6;
    measure_words(img, a5, &d5, &d6);
    d6 = (d6 + 2) & 0xffff;
    if ((d5 & 0xff) >= 5)                /* cmpi.b #$5,d5 */
        d6 = 0x14;
    wr16(img + A_TEXT_BOX_W, d6);

    wrap_line(img, a5, &d5, &d6);
    uint32_t h = (d5 * 6 + 7) & 0xffff;    /* lsl #2 + 2x add + 7 = d5*6+7 */
    wr16(img + A_TEXT_BOX_H, h);

    uint32_t x = (be16(img + A_CURSOR_X) >> 2) & 0xffff;
    if (x < 9) x = 9;
    uint32_t x_over = (x + be16(img + A_TEXT_BOX_W) + 2) & 0xffff;
    if (x_over >= 0x49)                  /* bcc -> no resta */
        x = (x - (x_over - 0x49)) & 0xffff;
    wr16(img + A_TEXT_BOX_X, x);

    uint32_t y = be16(img + A_CURSOR_Y);
    uint32_t y_over = (y + be16(img + A_TEXT_BOX_H)) & 0xffff;
    if (y_over >= 0xc8)
        y = (y - (y_over - 0xc8)) & 0xffff;
    wr16(img + A_TEXT_BOX_Y, y);

    uint32_t d7;
    uint32_t a0 = screen_address(img, be16(img + A_TEXT_BOX_X), y, &d7);
    uint32_t d2 = (be16(img + A_TEXT_BOX_W) + 2) & 0xffff;
    uint32_t d3 = be16(img + A_TEXT_BOX_H);
    save_rect(img, d2, d3, a0, d7);

    text_box_body(img, a5);
    prompt_wait(img);
    restore_rect(img);
}

void draw_text_box_variant(uint8_t *img, uint32_t the_command) {
    uint32_t d4 = the_command & 0x3ff;
    uint32_t a5 = position_text(img, 0x3ec8a, d4);
    textbox_tail(img, a5);
}

/* FUN_0001042c @ 0x1042c: variante que lee el indice de texto de A6[1] (via 0x10fb4,
 * que puede consumir un byte extra) y cae en la misma cola. Devuelve A6 avanzado. */
uint32_t text_index_arg(uint8_t *img, uint32_t a5, uint32_t d4, uint32_t *a6);
uint32_t draw_text_box(uint8_t *img, uint32_t a6) {
    uint32_t d4 = img[a6 + 1];
    a6 += 1;
    uint32_t a5 = text_index_arg(img, 0x3ec8a, d4, &a6);
    a6 += 1;
    textbox_tail(img, a5);
    return a6;
}

/* FUN_00010dfa @ 0x10dfa (cmd idx 92): caja de texto con argumentos inline
 * [idx][x][y][ancho]: 0x10fb4 (posiciona el texto), 0x10e66 (dibuja el marco),
 * direccion de pantalla de (x,y) y entrada de icono en 0x120a8. Devuelve A6 + 4. */
void hint_slot_alloc(uint8_t *img, uint32_t d4, uint32_t a0, uint32_t d7,
                 uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3);
uint32_t func_10dfa(uint8_t *img, uint32_t a6) {
    a6 += 1;
    uint32_t d4 = img[a6];
    uint32_t a5 = text_index_arg(img, 0x3ec8a, d4, &a6);
    wr16(img + A_TEXT_BOX_W, img[a6 + 3]);
    wr16(img + A_ELEM_1A532, img[a6 + 1]);
    wr16(img + A_ELEM_1A530, img[a6 + 2]);
    text_box_body(img, a5);

    uint32_t x = be16(img + A_ELEM_1A532);
    uint32_t y = be16(img + A_ELEM_1A530);
    uint32_t xw = (x << 2) & 0xffff;
    uint32_t d0 = (xw & 0xfff0) >> 1;
    uint32_t d7 = xw & 0xf;
    uint32_t d1 = (y * 0xa0) & 0xffff;
    uint32_t a0 = addr_add(be32(img + A_SCREEN_PTR_A), d0 + d1);
    uint32_t d2 = (be16(img + A_TEXT_BOX_W) + 2) & 0xffff;
    uint32_t d3 = (be16(img + A_CURSOR_TEXT_Y) - y + 8) & 0xffff;
    hint_slot_alloc(img, 3, a0, d7, d0, d1, d2, d3);
    return a6 + 4;
}

/* FUN_00010b6c @ 0x10b6c: dibuja un marco/ventana de texto (bordes segun los bits
 * 0xe0 de A6[3]) e imprime la cadena A5 con 0x10b24, y crea la entrada de icono.
 * A5 = puntero de la cadena; el envoltorio 0x10b52 devuelve A6 + 4. */
uint32_t advance_text_entry(uint8_t *img, uint32_t a5, uint32_t d4);
void print_char(uint8_t *img, uint32_t d0);
uint32_t print_line(uint8_t *img, uint32_t a5);
void box_hline(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3);
void box_vline(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3);
void func_11020(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d4);
void func_110da(uint8_t *img, uint32_t d4, uint32_t d0, uint32_t d1);
void hint_slot_alloc(uint8_t *img, uint32_t d4, uint32_t a0, uint32_t d7,
                 uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3);

void text_window(uint8_t *img, uint32_t a5, uint32_t a6) {
    wr32(img + A_COLOR_TABLE, 0x1d382);
    wr32(img + A_SCREEN_DRAW_BASE, 0);
    uint32_t d0 = img[a6 + 3] & 0x1f;
    wr16(img + A_TEXT_BOX_W, d0);
    d0 = (d0 + 2) & 0xffff;
    d0 = (d0 << 2) & 0xffff;
    uint32_t span = (d0 - 2) & 0xffff;
    d0 = img[a6 + 1];
    uint32_t d1 = img[a6 + 2];
    wr16(img + A_ELEM_1A532, d0);
    wr16(img + A_ELEM_1A530, d1);
    d0 = (d0 << 2) & 0xffff;
    d0 = (d0 + 2) & 0xffff;
    uint32_t d2 = 0;
    wr16(img + A_TEXT_BOX_SPAN, span);
    uint32_t d3 = (span - 2) & 0xffff;
    box_hline(img, d0, d1, d2, d3);
    d0 = (d0 - 1) & 0xffff;
    d2 = 0xf;
    for (uint32_t d6 = 2; d6 != 0xffff; d6 = (d6 - 1) & 0xffff) {
        d1 = (d1 + 1) & 0xffff;
        box_hline(img, d0, d1, d2, span);
    }
    d1 = (d1 - 1) & 0xffff;
    d0 = (d0 - 1) & 0xffff;
    d2 = 0;
    d3 = 2;
    box_vline(img, d0, d1, d2, d3);
    d1 = (d1 - 1) & 0xffff;
    d0 = (d0 + 1) & 0xffff;
    d3 = 1;
    box_vline(img, d0, d1, d2, d3);
    d0 = (d0 + span) & 0xffff;
    d0 = (d0 - 1) & 0xffff;
    d3 = 1;
    box_vline(img, d0, d1, d2, d3);
    d0 = (d0 + 1) & 0xffff;
    d1 = (d1 + 1) & 0xffff;
    d3 = 2;
    box_vline(img, d0, d1, d2, d3);

    d0 = be16(img + A_ELEM_1A532);
    d1 = (be16(img + A_ELEM_1A530) + 4) & 0xffff;
    for (;;) {
        wr16(img + A_CURSOR_TEXT_X, d0);
        wr16(img + A_CURSOR_TEXT_Y, d1);
        img[A_TEXT_END_FLAG] = 0;
        uint32_t sx = d0, sy = d1;
        print_char(img, 0x3b);
        a5 = print_line(img, a5);
        print_char(img, 0x3c);
        d0 = sx;
        d1 = (sy + 6) & 0xffff;
        wr16(img + A_CURSOR_TEXT_X, d0);
        wr16(img + A_CURSOR_TEXT_Y, d1);
        if (img[A_TEXT_END_FLAG] != 0)
            break;
    }

    uint32_t sp = be16(img + A_TEXT_BOX_SPAN);
    d0 = (d0 << 2) & 0xffff;
    d0 = (d0 + 1) & 0xffff;
    d2 = 0xf;
    d3 = sp;
    box_hline(img, d0, d1, d2, d3);
    d3 = 1;
    d2 = 0;
    box_vline(img, d0, d1, d2, d3);
    d1 = (d1 + 1) & 0xffff;
    d0 = (d0 + 1) & 0xffff;
    d3 = (sp - 2) & 0xffff;
    box_hline(img, d0, d1, d2, d3);
    d1 = (d1 - 1) & 0xffff;
    d0 = (d0 - 1) & 0xffff;
    d3 = 1;
    box_vline(img, d0, d1, d2, d3);
    d0 = (d0 + sp) & 0xffff;
    d0 = (d0 - 1) & 0xffff;
    d3 = 1;
    box_vline(img, d0, d1, d2, d3);

    uint32_t phase;
    uint32_t base = screen_address(img, be16(img + A_ELEM_1A532),
                                       be16(img + A_ELEM_1A530), &phase);
    wr32(img + A_ELEM_1A528, base);
    wr16(img + A_DAT_1A49E, phase);
    d0 = (be16(img + A_TEXT_BOX_W) + 2) & 0xffff;
    d1 = (be16(img + A_CURSOR_TEXT_Y) - be16(img + A_ELEM_1A530) + 2) & 0xffff;
    wr16(img + A_ELEM_1A532, d0);
    wr16(img + A_ELEM_1A530, d1);

    uint32_t bx = img[a6 + 1], by = img[a6 + 2];
    uint32_t b6 = img[a6 + 3] & 0xe0;
    if (b6 == 0) {
        d0 = (bx + 1) & 0xffff;
        d1 = (by - 7) & 0xffff;
        func_11020(img, d0, d1, 0x12);
    } else if (b6 == 0x20) {
        d0 = (bx + be16(img + A_TEXT_BOX_W)) & 0xffff;
        d1 = (by - 7) & 0xffff;
        func_110da(img, 0x12, d0, d1);
        d0 = (d0 - 1) & 0xffff;
    } else if (b6 == 0x80) {
        d0 = (bx + be16(img + A_TEXT_BOX_W)) & 0xffff;
        d1 = (be16(img + A_CURSOR_TEXT_Y) + 1) & 0xffff;
        func_110da(img, 0x15, d0, d1);
        d0 = (d0 - 1) & 0xffff;
    } else if (b6 == 0xa0) {
        d0 = (bx + 1) & 0xffff;
        d1 = (be16(img + A_CURSOR_TEXT_Y) + 1) & 0xffff;
        func_11020(img, d0, d1, 0x15);
    } else if (b6 == 0xc0) {
        d0 = (bx + be16(img + A_TEXT_BOX_W)) & 0xffff;
        d1 = (be16(img + A_CURSOR_TEXT_Y) + 4) & 0xffff;
        func_11020(img, d0, d1, 0x14);
    } else if (b6 == 0xe0) {
        d0 = (bx + 1) & 0xffff;
        d1 = (be16(img + A_CURSOR_TEXT_Y) + 4) & 0xffff;
        func_11020(img, d0, d1, 0x13);
    }
    uint32_t fa0 = be32(img + A_ELEM_1A528);
    uint32_t fd7 = be16(img + A_DAT_1A49E);
    uint32_t fd2 = be16(img + A_ELEM_1A532);
    uint32_t fd3 = be16(img + A_ELEM_1A530);
    hint_slot_alloc(img, 1, fa0, fd7, d0, d1, fd2, fd3);
}

/* FUN_00010b52 @ 0x10b52 (cmd idx 78): text index de A6[1] sobre el banco 0x3c534,
 * copia 0x1a482 a 0x1a484 y dibuja la ventana (0x10b6c). Devuelve A6 + 5. */
uint32_t func_10b52(uint8_t *img, uint32_t a6) {
    a6 += 1;
    uint32_t a5 = text_index_arg(img, 0x3c534, img[a6], &a6);
    wr16(img + A_DAT_1A484, be16(img + A_TEXT_INDEX));
    text_window(img, a5, a6);
    return a6 + 4;
}

/* FUN_00013f86 @ 0x13f86: mide y dibuja una caja de texto autodimensionada.
 * 15944, walk_entry sobre la tabla 0x24982 (indice 0x1acc7-1), recorre bloques de
 * 3 bytes: los bytes 0x32..0x3c son marcadores (se guardan punteros -3 en 0x1a7b6),
 * el resto se blitea (0x14138) y actualiza la caja 0x1ab40..43 con comparaciones de
 * byte (x = a6[1], y*2 = a6[2]<<1, + d2/d3). Cierra con cursores, print_char y los
 * bordes 0x12fee/0x1320a. */
void func_15944(uint8_t *img);
void walk_entry(uint8_t *img, uint32_t a4, uint32_t d1, uint32_t *a4o,
                  uint32_t *a5o);
void run_text_entry_ex(uint8_t *img, uint32_t a6, uint32_t *a0o, uint32_t *d2o,
                    uint32_t *d3o, uint32_t *d7o);
void print_char(uint8_t *img, uint32_t d0);
void overlay_a_draw(uint8_t *img, uint32_t a6);
void box_hline_back(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3);
void box_vline_back(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3);

void scene_text_box(uint8_t *img) {
    func_15944(img);
    wr16(img + A_DAT_1A49E, img[0x1acf1]);
    uint32_t a4o, a5o;
    walk_entry(img, 0x24982, (img[0x1acc7] - 1) & 0xffff, &a4o, &a5o);
    wr32(img + A_DAT_1A4B2, a4o);
    uint32_t a6 = a5o;
    wr32(img + 0x1ab40, 0xff00ff00);
    uint32_t a4 = 0x1a7b6;
    img[0x1a751] = 0;
    for (;;) {
        uint32_t d4 = img[a6];
        if (d4 >= 0x32 && d4 < 0x3d) {
            wr32(img + a4, a6 - 3);
            a4 += 4;
            img[0x1a751] = (uint8_t)(img[0x1a751] + 1);
            uint32_t d0 = img[0x1a751];
            if (d0 == img[0x1acc9] ||
                (img[0x1acc7] == 0x20 && d4 == 0x3b && (img[0x1aced] & 8))) {
                a6 += 3;
                continue;
            }
        }
        uint32_t a0o, d2, d3, d7;
        run_text_entry_ex(img, a6, &a0o, &d2, &d3, &d7);
        /* D6 = A6[1] & 0x7f: 14138 lo guarda con bit7 y lo enmascara al ajustar
         * la posicion (0x1416c); sin la mascara el ancho de la caja se dispara. */
        uint32_t d6 = img[a6 + 1] & 0x7f;
        /* D5 = (A6[2] << 1) & 0xff: 14138 hace `lsl.b` (byte), no `lsl.w`. */
        uint32_t d5 = (img[a6 + 2] << 1) & 0xff;
        if (d6 < img[0x1ab40])
            img[0x1ab40] = (uint8_t)d6;
        uint32_t t = (d6 + d2) & 0xff;
        if (t >= img[0x1ab41])
            img[0x1ab41] = (uint8_t)t;
        if (d5 < img[0x1ab42])
            img[0x1ab42] = (uint8_t)d5;
        t = (d5 + d3) & 0xff;
        if (t >= img[0x1ab43])
            img[0x1ab43] = (uint8_t)t;
        a6 += 3;
        if (a6 == a4o)
            break;
    }
    uint32_t d0 = (img[0x1ab41] - img[0x1ab40] - 2) & 0xff;
    img[0x1ab49] = (uint8_t)d0;
    d0 = img[0x1ab40];
    wr16(img + A_CURSOR_TEXT_X, d0 & 0xffff);
    wr16(img + 0x1aa6a, (d0 + 1) & 0xffff);
    uint32_t d1 = (img[0x1ab43] + 2) & 0xff;
    wr16(img + 0x1aa6c, d1 & 0xffff);
    wr16(img + A_CURSOR_TEXT_Y, d1 & 0xffff);
    wr32(img + A_SCREEN_DRAW_BASE, 0x8000);
    wr32(img + A_COLOR_TABLE, 0x1d3c2);
    print_char(img, 0x3b);
    overlay_a_draw(img, a6);
    print_char(img, 0x3c);
    d0 = be16(img + 0x1aa6a);
    d1 = be16(img + 0x1aa6c);
    d0 = ((d0 - 1) << 2) & 0xffff;
    d1 = (d1 - 2) & 0xffff;
    uint32_t d6v = ((img[0x1ab49] + 2) << 2) & 0xffff;
    box_hline_back(img, d0, d1, 7, d6v);
    box_hline_back(img, d0, (d1 + 1) & 0xffff, 6, d6v);
    box_hline_back(img, d0, (d1 + 8) & 0xffff, 7, d6v);
    box_vline_back(img, d0, d1, 7, 9);
    box_vline_back(img, (d0 + d6v - 1) & 0xffff, d1, 7, 9);
}
