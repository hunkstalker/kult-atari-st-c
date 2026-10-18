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

void box_hline(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t);
void box_vline(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t);
uint32_t print_line(uint8_t *, uint32_t);
void print_char(uint8_t *, uint32_t);

/* 0x10e66 — marco + palabras + borde inferior de la caja de texto */
void text_box_body(uint8_t *img, uint32_t str) {
    wr32(img + A_COLOR_TABLE, 0x1d3a2);
    wr32(img + A_SCREEN_DRAW_BASE, 0);

    uint32_t x = (be16(img + A_TEXT_BOX_W) + 2) & 0xffff;
    x = (x << 2) & 0xffff;
    x = (x - 2) & 0xffff;
    uint32_t len = x;
    x = be16(img + A_TEXT_BOX_X);
    uint32_t y = be16(img + A_TEXT_BOX_Y);
    x = ((x << 2) + 2) & 0xffff;
    uint32_t color = 6;
    uint32_t span = len;
    wr16(img + A_TEXT_BOX_SPAN, span);
    len = (len - 2) & 0xffff;
    box_hline(img, x, y, color, len);                 /* borde superior */

    x = (x - 1) & 0xffff;
    color = 0xd;
    for (uint32_t i = 2; ; i = (i - 1) & 0xffff) {
        y = (y + 1) & 0xffff;
        len = span;
        box_hline(img, x, y, color, len);
        if (i == 0) break;
    }

    y = (y - 1) & 0xffff;
    x = (x - 1) & 0xffff;
    color = 6;
    len = 2;
    box_vline(img, x, y, color, len);                 /* esquina sup. izq. */
    y = (y - 1) & 0xffff;
    x = (x + 1) & 0xffff;
    len = 1;
    box_vline(img, x, y, color, len);
    x = (x + span - 1) & 0xffff;
    len = 1;
    box_vline(img, x, y, color, len);
    x = (x + 1) & 0xffff;
    y = (y + 1) & 0xffff;
    len = 2;
    box_vline(img, x, y, color, len);                 /* esquina sup. der. */

    x = be16(img + A_TEXT_BOX_X);
    y = (be16(img + A_TEXT_BOX_Y) + 4) & 0xffff;
    wr16(img + A_CURSOR_TEXT_X, x);
    wr16(img + A_CURSOR_TEXT_Y, y);
    img[A_TEXT_END_FLAG] = 0;

    do {
        print_char(img, 0x3b);                      /* ';' */
        str = print_line(img, str);
        print_char(img, 0x3c);                      /* '<' */
        y = (y + 6) & 0xffff;
        wr16(img + A_CURSOR_TEXT_Y, y);
        wr16(img + A_CURSOR_TEXT_X, x);
    } while (img[A_TEXT_END_FLAG] == 0);

    span = be16(img + A_TEXT_BOX_SPAN);
    x = ((x << 2) + 1) & 0xffff;
    color = 0xd;
    len = span;
    box_hline(img, x, y, color, len);                 /* borde inferior */
    len = 1;
    color = 6;
    box_vline(img, x, y, color, len);
    y = (y + 1) & 0xffff;
    x = (x + 1) & 0xffff;
    len = (span - 2) & 0xffff;
    box_hline(img, x, y, color, len);
    y = (y - 1) & 0xffff;
    x = (x - 1) & 0xffff;
    len = 1;
    box_vline(img, x, y, color, len);
    x = (x + span - 1) & 0xffff;
    len = 1;
    box_vline(img, x, y, color, len);
}

/* FUN_00010440 @ 0x10440: orquestador del comando de caja de texto (nibble 0x9000).
 * position_text (índice = cmd & 0x3ff) sobre el banco 0x3ec8a; mide (0x10998), ajusta
 * el ancho (max 0x14 si hay >= 5 palabras), justifica (0x109bc), calcula la geometría a
 * partir del cursor (0x1a450/0x1a452), salva el rect (0x1258a), dibuja el cuerpo
 * (0x10e66), espera (0x12292) y restaura (0x124f2). */
uint32_t position_text(uint8_t *, uint32_t, uint32_t);
void measure_words(uint8_t *, uint32_t, uint32_t *, uint32_t *);
void wrap_line(uint8_t *, uint32_t, uint32_t *, uint32_t *);
uint32_t screen_address(uint8_t *, uint32_t, uint32_t, uint32_t *);
uint32_t save_rect(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t);
void text_box_body(uint8_t *, uint32_t);
void prompt_wait(uint8_t *);
void restore_rect(uint8_t *);

/* Cola comun de 0x1042c/0x10440: mide la caja (0x10998/0x109bc), fija 0x1a512/0x1a514,
 * calcula la posicion y dibuja (0x1258a + 0x10e66 + 0x12292 + 0x124f2). */
static void textbox_tail(uint8_t *img, uint32_t str) {
    uint32_t nwords, width;
    measure_words(img, str, &nwords, &width);
    width = (width + 2) & 0xffff;
    if ((nwords & 0xff) >= 5)                /* cmpi.b #$5,nwords */
        width = 0x14;
    wr16(img + A_TEXT_BOX_W, width);

    wrap_line(img, str, &nwords, &width);
    uint32_t h = (nwords * 6 + 7) & 0xffff;    /* lsl #2 + 2x add + 7 = nwords*6+7 */
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

    uint32_t phase;
    uint32_t addr = screen_address(img, be16(img + A_TEXT_BOX_X), y, &phase);
    uint32_t w = (be16(img + A_TEXT_BOX_W) + 2) & 0xffff;
    uint32_t h2 = be16(img + A_TEXT_BOX_H);
    save_rect(img, w, h2, addr, phase);

    text_box_body(img, str);
    prompt_wait(img);
    restore_rect(img);
}

/* 0x10440 — idem (otra ruta de entrada) */
void draw_text_box_variant(uint8_t *img, uint32_t the_command) {
    uint32_t index = the_command & 0x3ff;
    uint32_t str = position_text(img, 0x3ec8a, index);
    textbox_tail(img, str);
}

/* FUN_0001042c @ 0x1042c: variante que lee el indice de texto de A6[1] (via 0x10fb4,
 * que puede consumir un byte extra) y cae en la misma cola. Devuelve A6 avanzado. */
uint32_t text_index_arg(uint8_t *, uint32_t, uint32_t, uint32_t *);
/* 0x1042c — calcula geometria de caja de texto y la dibuja */
uint32_t draw_text_box(uint8_t *img, uint32_t script) {
    uint32_t index = img[script + 1];
    script += 1;
    uint32_t str = text_index_arg(img, 0x3ec8a, index, &script);
    script += 1;
    textbox_tail(img, str);
    return script;
}

/* FUN_00010dfa @ 0x10dfa (cmd idx 92): caja de texto con argumentos inline
 * [idx][x][y][ancho]: 0x10fb4 (posiciona el texto), 0x10e66 (dibuja el marco),
 * direccion de pantalla de (x,y) y entrada de icono en 0x120a8. Devuelve A6 + 4. */
void hint_slot_alloc(uint8_t *, uint32_t, uint32_t, uint32_t,
                 uint32_t, uint32_t, uint32_t, uint32_t);
/* 0x10dfa — text box icon */
uint32_t text_box_icon(uint8_t *img, uint32_t script) {
    script += 1;
    uint32_t index = img[script];
    uint32_t str = text_index_arg(img, 0x3ec8a, index, &script);
    wr16(img + A_TEXT_BOX_W, img[script + 3]);
    wr16(img + A_ELEM_1A532, img[script + 1]);
    wr16(img + A_ELEM_1A530, img[script + 2]);
    text_box_body(img, str);

    uint32_t x = be16(img + A_ELEM_1A532);
    uint32_t y = be16(img + A_ELEM_1A530);
    uint32_t xw = (x << 2) & 0xffff;
    uint32_t xoff = (xw & 0xfff0) >> 1;
    uint32_t phase = xw & 0xf;
    uint32_t yoff = (y * 0xa0) & 0xffff;
    uint32_t addr = addr_add(be32(img + A_SCREEN_PTR_A), xoff + yoff);
    uint32_t w = (be16(img + A_TEXT_BOX_W) + 2) & 0xffff;
    uint32_t h = (be16(img + A_CURSOR_TEXT_Y) - y + 8) & 0xffff;
    hint_slot_alloc(img, 3, addr, phase, xoff, yoff, w, h);
    return script + 4;
}

/* FUN_00010b6c @ 0x10b6c: dibuja un marco/ventana de texto (bordes segun los bits
 * 0xe0 de A6[3]) e imprime la cadena A5 con 0x10b24, y crea la entrada de icono.
 * A5 = puntero de la cadena; el envoltorio 0x10b52 devuelve A6 + 4. */
uint32_t advance_text_entry(uint8_t *, uint32_t, uint32_t);
void print_char(uint8_t *, uint32_t);
uint32_t print_line(uint8_t *, uint32_t);
void box_hline(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t);
void box_vline(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t);
void text_blit_ovl_b(uint8_t *, uint32_t, uint32_t, uint32_t);
void text_blit_rev_entry(uint8_t *, uint32_t, uint32_t, uint32_t);
void hint_slot_alloc(uint8_t *, uint32_t, uint32_t, uint32_t,
                 uint32_t, uint32_t, uint32_t, uint32_t);

/* 0x10b6c — ventana de texto con adorno/flecha (bits 0xe0) */
void text_window(uint8_t *img, uint32_t str, uint32_t script) {
    wr32(img + A_COLOR_TABLE, 0x1d382);
    wr32(img + A_SCREEN_DRAW_BASE, 0);
    uint32_t x = img[script + 3] & 0x1f;
    wr16(img + A_TEXT_BOX_W, x);
    x = (x + 2) & 0xffff;
    x = (x << 2) & 0xffff;
    uint32_t span = (x - 2) & 0xffff;
    x = img[script + 1];
    uint32_t y = img[script + 2];
    wr16(img + A_ELEM_1A532, x);
    wr16(img + A_ELEM_1A530, y);
    x = (x << 2) & 0xffff;
    x = (x + 2) & 0xffff;
    uint32_t color = 0;
    wr16(img + A_TEXT_BOX_SPAN, span);
    uint32_t len = (span - 2) & 0xffff;
    box_hline(img, x, y, color, len);
    x = (x - 1) & 0xffff;
    color = 0xf;
    for (uint32_t i = 2; i != 0xffff; i = (i - 1) & 0xffff) {
        y = (y + 1) & 0xffff;
        box_hline(img, x, y, color, span);
    }
    y = (y - 1) & 0xffff;
    x = (x - 1) & 0xffff;
    color = 0;
    len = 2;
    box_vline(img, x, y, color, len);
    y = (y - 1) & 0xffff;
    x = (x + 1) & 0xffff;
    len = 1;
    box_vline(img, x, y, color, len);
    x = (x + span) & 0xffff;
    x = (x - 1) & 0xffff;
    len = 1;
    box_vline(img, x, y, color, len);
    x = (x + 1) & 0xffff;
    y = (y + 1) & 0xffff;
    len = 2;
    box_vline(img, x, y, color, len);

    x = be16(img + A_ELEM_1A532);
    y = (be16(img + A_ELEM_1A530) + 4) & 0xffff;
    for (;;) {
        wr16(img + A_CURSOR_TEXT_X, x);
        wr16(img + A_CURSOR_TEXT_Y, y);
        img[A_TEXT_END_FLAG] = 0;
        uint32_t sx = x, sy = y;
        print_char(img, 0x3b);
        str = print_line(img, str);
        print_char(img, 0x3c);
        x = sx;
        y = (sy + 6) & 0xffff;
        wr16(img + A_CURSOR_TEXT_X, x);
        wr16(img + A_CURSOR_TEXT_Y, y);
        if (img[A_TEXT_END_FLAG] != 0)
            break;
    }

    uint32_t sp = be16(img + A_TEXT_BOX_SPAN);
    x = (x << 2) & 0xffff;
    x = (x + 1) & 0xffff;
    color = 0xf;
    len = sp;
    box_hline(img, x, y, color, len);
    len = 1;
    color = 0;
    box_vline(img, x, y, color, len);
    y = (y + 1) & 0xffff;
    x = (x + 1) & 0xffff;
    len = (sp - 2) & 0xffff;
    box_hline(img, x, y, color, len);
    y = (y - 1) & 0xffff;
    x = (x - 1) & 0xffff;
    len = 1;
    box_vline(img, x, y, color, len);
    x = (x + sp) & 0xffff;
    x = (x - 1) & 0xffff;
    len = 1;
    box_vline(img, x, y, color, len);

    uint32_t phase;
    uint32_t base = screen_address(img, be16(img + A_ELEM_1A532),
                                       be16(img + A_ELEM_1A530), &phase);
    wr32(img + A_ELEM_1A528, base);
    wr16(img + A_DAT_1A49E, phase);
    x = (be16(img + A_TEXT_BOX_W) + 2) & 0xffff;
    y = (be16(img + A_CURSOR_TEXT_Y) - be16(img + A_ELEM_1A530) + 2) & 0xffff;
    wr16(img + A_ELEM_1A532, x);
    wr16(img + A_ELEM_1A530, y);

    uint32_t bx = img[script + 1], by = img[script + 2];
    uint32_t b6 = img[script + 3] & 0xe0;
    /* El original deja d0/d1 en (bx,by) y solo las ramas de borde los reescriben;
     * para 0x40/0x60 (no hay rama) 0x120a8 recibe (bx,by). */
    x = bx;
    y = by;
    if (b6 == 0) {
        x = (bx + 1) & 0xffff;
        y = (by - 7) & 0xffff;
        text_blit_ovl_b(img, x, y, 0x12);
    } else if (b6 == 0x20) {
        x = (bx + be16(img + A_TEXT_BOX_W)) & 0xffff;
        y = (by - 7) & 0xffff;
        text_blit_rev_entry(img, 0x12, x, y);
        x = (x - 1) & 0xffff;
    } else if (b6 == 0x80) {
        x = (bx + be16(img + A_TEXT_BOX_W)) & 0xffff;
        y = (be16(img + A_CURSOR_TEXT_Y) + 1) & 0xffff;
        text_blit_rev_entry(img, 0x15, x, y);
        x = (x - 1) & 0xffff;
    } else if (b6 == 0xa0) {
        x = (bx + 1) & 0xffff;
        y = (be16(img + A_CURSOR_TEXT_Y) + 1) & 0xffff;
        text_blit_ovl_b(img, x, y, 0x15);
    } else if (b6 == 0xc0) {
        x = (bx + be16(img + A_TEXT_BOX_W)) & 0xffff;
        y = (be16(img + A_CURSOR_TEXT_Y) + 4) & 0xffff;
        text_blit_ovl_b(img, x, y, 0x14);
    } else if (b6 == 0xe0) {
        x = (bx + 1) & 0xffff;
        y = (be16(img + A_CURSOR_TEXT_Y) + 4) & 0xffff;
        text_blit_ovl_b(img, x, y, 0x13);
    }
    uint32_t fa0 = be32(img + A_ELEM_1A528);
    uint32_t fd7 = be16(img + A_DAT_1A49E);
    uint32_t fd2 = be16(img + A_ELEM_1A532);
    uint32_t fd3 = be16(img + A_ELEM_1A530);
    hint_slot_alloc(img, 1, fa0, fd7, x, y, fd2, fd3);
}

/* FUN_00010b52 @ 0x10b52 (cmd idx 78): text index de A6[1] sobre el banco 0x3c534,
 * copia 0x1a482 a 0x1a484 y dibuja la ventana (0x10b6c). Devuelve A6 + 5. */
uint32_t text_window_entry(uint8_t *img, uint32_t script) {
    script += 1;
    uint32_t str = text_index_arg(img, 0x3c534, img[script], &script);
    wr16(img + A_DAT_1A484, be16(img + A_TEXT_INDEX));
    text_window(img, str, script);
    return script + 4;
}

/* FUN_00013f86 @ 0x13f86: mide y dibuja una caja de texto autodimensionada.
 * 15944, walk_entry sobre la tabla 0x24982 (indice 0x1acc7-1), recorre bloques de
 * 3 bytes: los bytes 0x32..0x3c son marcadores (se guardan punteros -3 en 0x1a7b6),
 * el resto se blitea (0x14138) y actualiza la caja 0x1ab40..43 con comparaciones de
 * byte (x = a6[1], y*2 = a6[2]<<1, + d2/d3). Cierra con cursores, print_char y los
 * bordes 0x12fee/0x1320a. */
void copy_rows(uint8_t *);
void walk_entry(uint8_t *, uint32_t, uint32_t, uint32_t *,
                  uint32_t *);
void run_text_entry_ex(uint8_t *, uint32_t, uint32_t *, uint32_t *,
                    uint32_t *, uint32_t *, uint32_t *);
void print_char(uint8_t *, uint32_t);
void overlay_a_draw(uint8_t *, uint32_t);
void box_hline_back(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t);
void box_vline_back(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t);

/* 0x13f86 — caja/overlay autodimensionado de la escena */
void scene_text_box(uint8_t *img) {
    copy_rows(img);
    wr16(img + A_DAT_1A49E, img[0x1acf1]);
    uint32_t end, entry;
    walk_entry(img, 0x24982, (img[0x1acc7] - 1) & 0xffff, &end, &entry);
    wr32(img + A_DAT_1A4B2, end);
    uint32_t script = entry;
    wr32(img + 0x1ab40, 0xff00ff00);
    uint32_t marker = 0x1a7b6;
    img[0x1a751] = 0;
    for (;;) {
        uint32_t b = img[script];
        if (b >= 0x32 && b < 0x3d) {
            wr32(img + marker, script - 3);
            marker += 4;
            img[0x1a751] = (uint8_t)(img[0x1a751] + 1);
            uint32_t x = img[0x1a751];
            if (x == img[0x1acc9] ||
                (img[0x1acc7] == 0x20 && b == 0x3b && (img[0x1aced] & 8))) {
                script += 3;
                continue;
            }
        }
        uint32_t addr, w, h, phase, v2;
        run_text_entry_ex(img, script, &addr, &w, &h, &phase, &v2);
        /* D6 = A6[1] & 0x7f: 14138 lo guarda con bit7 y lo enmascara al ajustar
         * la posicion (0x1416c); sin la mascara el ancho de la caja se dispara. */
        uint32_t v = img[script + 1] & 0x7f;
        /* D5 = (A6[2]<<1)&0xff ya ajustado por 14138 (el -8 de 'S'); ver v2. */
        if (v < img[0x1ab40])
            img[0x1ab40] = (uint8_t)v;
        uint32_t t = (v + w) & 0xff;
        if (t >= img[0x1ab41])
            img[0x1ab41] = (uint8_t)t;
        if (v2 < img[0x1ab42])
            img[0x1ab42] = (uint8_t)v2;
        t = (v2 + h) & 0xff;
        if (t >= img[0x1ab43])
            img[0x1ab43] = (uint8_t)t;
        script += 3;
        if (script == end)
            break;
    }
    uint32_t x = (img[0x1ab41] - img[0x1ab40] - 2) & 0xff;
    img[0x1ab49] = (uint8_t)x;
    x = img[0x1ab40];
    wr16(img + A_CURSOR_TEXT_X, x & 0xffff);
    wr16(img + 0x1aa6a, (x + 1) & 0xffff);
    uint32_t y = (img[0x1ab43] + 2) & 0xff;
    wr16(img + 0x1aa6c, y & 0xffff);
    wr16(img + A_CURSOR_TEXT_Y, y & 0xffff);
    wr32(img + A_SCREEN_DRAW_BASE, 0x8000);
    wr32(img + A_COLOR_TABLE, 0x1d3c2);
    print_char(img, 0x3b);
    overlay_a_draw(img, script);
    print_char(img, 0x3c);
    x = be16(img + 0x1aa6a);
    y = be16(img + 0x1aa6c);
    x = ((x - 1) << 2) & 0xffff;
    y = (y - 2) & 0xffff;
    uint32_t w2 = ((img[0x1ab49] + 2) << 2) & 0xffff;
    box_hline_back(img, x, y, 7, w2);
    box_hline_back(img, x, (y + 1) & 0xffff, 6, w2);
    box_hline_back(img, x, (y + 8) & 0xffff, 7, w2);
    box_vline_back(img, x, y, 7, 9);
    box_vline_back(img, (x + w2 - 1) & 0xffff, y, 7, 9);
}
