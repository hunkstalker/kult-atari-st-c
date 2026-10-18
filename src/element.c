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

void func_126e0(uint8_t *img, uint32_t d2w, uint32_t d3w, uint32_t d7w, uint32_t a1) {
    uint32_t a3 = be32(img + A_DAT_1A534);
    uint32_t a0 = A_SAVE_DESC;
    for (int i = 0; i < 0x627; i++)
        wr32(img + a0 + (uint32_t)i * 4, 0);

    blit_mask_setup(img, d2w, d7w);

    a0 = A_SAVE_DESC;
    uint32_t outer = (d2w - 1) & 0xffff;
    uint32_t rows = (d3w - 1) & 0xffff;
    uint32_t d7 = d7w & 0xffff;
    for (;;) {
        uint16_t d4 = rotate_right16(0xf000, d7);
        uint32_t base = a0;
        uint32_t src_base = a1;
        uint32_t d5 = rows;
        for (;;) {
            uint16_t d2 = rotate_right16(be16(img + a1), d7);
            for (int k = 0; k < 4; k++) {
                wr16(img + a0, (uint16_t)(be16(img + a0) | (d2 & d4)));
                a0 += 2;
                d2 = rotate_left16(d2, 4);
            }
            a0 += 0x38;
            a1 += a3;
            d5 = (d5 - 1) & 0xffff;
            if (d5 == 0xffff)
                break;
        }
        a0 = base;
        a1 = src_base + 2;
        d7 = (d7 + 4) & 0xf;
        if (d7 == 0)
            a0 += 8;
        outer = (outer - 1) & 0xffff;
        if (outer == 0xffff)
            break;
    }
}

void func_1276c(uint8_t *img, uint32_t d2w, uint32_t d3w, uint32_t d7w,
                 uint32_t d6w, uint32_t a0) {
    uint32_t outer = (d2w - 1) & 0xffff;
    uint32_t rows = (d3w - 1) & 0xffff;
    uint32_t d7 = d7w & 0xffff;
    for (;;) {
        uint16_t d1 = rotate_right16(0x0fff, d7);
        uint16_t d4 = (uint16_t)~d1;
        uint32_t base = a0;
        uint32_t d5 = rows;
        for (;;) {
            uint16_t d2 = rotate_right16((uint16_t)d6w, d7);
            for (int k = 0; k < 4; k++) {
                wr16(img + a0, (uint16_t)((be16(img + a0) & d1) | (d2 & d4)));
                a0 += 2;
                d2 = rotate_left16(d2, 4);
            }
            a0 += 0x98;
            d5 = (d5 - 1) & 0xffff;
            if (d5 == 0xffff)
                break;
        }
        a0 = base;
        d7 = (d7 + 4) & 0xf;
        if (d7 == 0)
            a0 += 8;
        outer = (outer - 1) & 0xffff;
        if (outer == 0xffff)
            break;
    }
    wait_vbl(img);
}

void func_11ece(uint8_t *img, uint32_t d1, uint32_t a0, uint32_t a1) {
    uint32_t d3 = 1;
    for (;;) {
        blit_mask_rows(img, a0, a1, d3);
        d3 = (d3 + 1) & 0xffff;
        a1 -= 0x40;
        d1 = (d1 - 1) & 0xffff;
        if (d1 == 0xffff)
            break;
    }
}

void func_11ee8(uint8_t *img, uint32_t d1, uint32_t a0, uint32_t a1) {
    uint32_t d3 = 1;
    for (;;) {
        blit_mask_rows(img, a0, a1, d3);
        a0 -= 0xa0;
        d3 = (d3 + 1) & 0xffff;
        d1 = (d1 - 1) & 0xffff;
        if (d1 == 0xffff)
            break;
    }
}

/* FUN_0001285a @ 0x1285a: busca en la tabla de 6 bytes 0x19e19 la entrada cuya clase
 * (byte0) coincide con (*0x1a772)[3] & 0xf8. Si la encuentra copia los bytes 4/5 a
 * 0x1ab4c/0x1ab4d, devuelve por out d0=byte2, d1=byte1, d2=0xff, d3=byte3, y marca
 * encontrado (el llamante 0x128a4 bifurca por el Z del `bsr`: encontrado -> Z=0).
 * Si no, cae en 0x10768 (rts) dejando los registros de entrada intactos. */
int func_1285a(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3,
                uint32_t *out) {
    uint32_t a0 = be32(img + A_ELEM_CLASS);
    uint32_t d2v = (d2 & 0xffffff00u) | (uint32_t)(img[a0 + 3] & 0xf8);
    uint32_t p = A_ELEM_TBL_19E19;
    for (;;) {
        if (img[p] == (uint8_t)(d2v & 0xff)) {
            out[1] = (d1 & 0xffffff00u) | img[p + 1];
            out[0] = (d0 & 0xffffff00u) | img[p + 2];
            out[3] = (d3 & 0xffffff00u) | img[p + 3];
            img[A_ELEM_1AB4C] = img[p + 4];
            img[A_ELEM_1AB4D] = img[p + 5];
            out[2] = (d2v & 0xffffff00u) | 0xff;
            return 1;
        }
        p += 6;
        if (p == A_ELEM_TBL_19EA9)
            break;
    }
    out[0] = d0; out[1] = d1; out[2] = d2v; out[3] = d3;
    return 0;
}

/* FUN_0001296c @ 0x1296c: construye un bitmap en 0x3b1ae a partir de la entrada
 * (*A5)*10 de la tabla 0x1a252. Cabecera [alto][ancho], luego una fila superior de
 * `ancho` words, (alto-2) filas [izq][medio x(ancho-2)][der] y una fila inferior de
 * `ancho` words. Fija el stride 0x1a534 = ancho*2. A5 avanza 1 (el indice). */
void func_1296c(uint8_t *img, uint32_t *a5_io) {
    uint32_t a5 = *a5_io;
    uint32_t idx = img[a5++];
    uint32_t a2 = A_ELEM_TBL_1A252 + (idx & 0xff) * 10;
    uint32_t a1 = A_ELEM_BUILD;

    uint32_t d2 = img[a2++];
    uint32_t d3 = img[a2++];
    d2 &= 0xff;
    d3 &= 0xff;
    wr32(img + A_DAT_1A534, (uint32_t)(d2 << 1));
    img[a1++] = (uint8_t)d3;
    img[a1++] = (uint8_t)d2;

    uint32_t top = be16(img + a2); a2 += 2;
    for (uint32_t d5 = (d2 - 1) & 0xffff;; d5 = (d5 - 1) & 0xffff) {
        wr16(img + a1, (uint16_t)top); a1 += 2;
        if (d5 == 0) break;
    }
    uint32_t mid = be16(img + a2); a2 += 2;
    uint32_t d2b = (d2 - 3) & 0xffff;
    for (uint32_t d3b = (d3 - 3) & 0xffff;; d3b = (d3b - 1) & 0xffff) {
        wr16(img + a1, be16(img + a2)); a1 += 2;
        for (uint32_t d5 = d2b;; d5 = (d5 - 1) & 0xffff) {
            wr16(img + a1, (uint16_t)mid); a1 += 2;
            if (d5 == 0) break;
        }
        wr16(img + a1, be16(img + a2 + 2)); a1 += 2;
        if (d3b == 0) break;
    }
    for (uint32_t d2c = (d2b + 2) & 0xffff;; d2c = (d2c - 1) & 0xffff) {
        wr16(img + a1, (uint16_t)top); a1 += 2;
        if (d2c == 0) break;
    }
    *a5_io = a5;
}

uint32_t walk_entry(uint8_t *img, uint32_t a4, uint32_t d1,
                      uint32_t *a4o, uint32_t *a5o);
uint32_t advance_text_entry(uint8_t *img, uint32_t a1, uint32_t d4);
void func_12ad6(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t d2, uint32_t d3,
                 uint32_t a3);
void func_12b04(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t d2, uint32_t d3,
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
uint32_t func_129c8(uint8_t *img, uint32_t d1) {
    uint32_t end, a5;
    walk_entry(img, A_ELEM_TBL_2751E, d1, &end, &a5);
    func_1296c(img, &a5);
    for (;;) {
        uint32_t n = img[a5];
        uint32_t a1 = advance_text_entry(img, A_ELEM_TBL_31072, n);
        uint32_t d3 = img[a1++];
        uint32_t d2 = img[a1++];
        uint32_t d0 = (((uint32_t)img[a5 + 2] << 8) | img[a5 + 1]) & 0x3fff;
        uint32_t a0 = A_ELEM_BUILD + 2 + d0 * 2;
        uint32_t str = be32(img + A_DAT_1A534);
        uint32_t d5 = str;
        if (img[a5 + 2] & 0x80) {
            a0 += (uint32_t)((str & 0xffff) * (d3 & 0xffff));
            a0 -= str;
            d5 = (uint32_t)(-str);
        }
        if (img[a5 + 2] & 0x40)
            func_12b04(img, a0, a1, d2, d3, d5);
        else
            func_12ad6(img, a0, a1, d2, d3, d5);
        a5 += 3;
        if (a5 == end)
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
uint32_t elem_command(uint8_t *img, uint32_t a6, uint32_t *a6o, uint32_t *d0o,
                        uint32_t *d1o) {
    uint32_t d0 = 0, d1 = 0, d3 = 0;
    d1 = img[a6 + 1];
    if (d1 == 0xff) {
        a6 += 2;
        if (img[A_DAT_1AD0A] != 0) {
            *a6o = a6; *d0o = d0; *d1o = d3;
            return a6;
        }
        draw_selection_frame(img);
        uint32_t out[4];
        if (!func_1285a(img, d0, d1, 0, d3, out)) {
            *a6o = a6; *d0o = d0; *d1o = d3;
            return a6;
        }
        d0 = out[0]; d1 = out[1]; d3 = out[3];
    } else {
        d0 = img[a6 + 2];
        d3 = img[a6 + 3];
        a6 += 4;
    }
    *a6o = a6; *d0o = d0; *d1o = d3;

    wr16(img + A_ELEM_1A532, d0 & 0xffff);
    wr16(img + A_ELEM_1A530, d3 & 0xffff);
    img[A_ELEM_1AB4E] = (uint8_t)(d1 & 0xff);

    uint32_t a1 = func_129c8(img, (d1 - 1) & 0xffff);
    uint32_t d2 = 0;
    a1 += 2;
    d2 = img[a1++];
    wr16(img + A_ELEM_1A52C, d2 & 0xffff);
    d2 = img[a1++];
    wr16(img + A_ELEM_1A52E, d2 & 0xffff);
    wr32(img + A_ELEM_1A528, a1);

    uint32_t x = be16(img + A_ELEM_1A532);
    uint32_t y = be16(img + A_ELEM_1A530);
    uint32_t d7;
    uint32_t a0 = screen_address(img, x, y, &d7);
    uint32_t dd3 = be16(img + A_ELEM_1A52C);
    uint32_t dd2 = be16(img + A_ELEM_1A52E);
    hint_slot_alloc(img, 2, a0, d7, x, y, dd2, dd3);
    return a6;
}

uint32_t func_128a4(uint8_t *img, uint32_t a6) {
    uint32_t a6o, d0o, d1o;
    return elem_command(img, a6, &a6o, &d0o, &d1o);
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
uint32_t func_12a4a(uint8_t *img, uint32_t d1) {
    uint32_t end, a5;
    walk_entry(img, A_ELEM_TBL_23E9C, d1, &end, &a5);
    for (;;) {
        uint32_t t = img[a5];
        uint32_t a1 = advance_text_entry(img, A_TEXT_TABLE, t & 0x7f);
        uint32_t d3 = img[a1++];
        uint32_t d2 = img[a1++];
        uint32_t d0 = (((uint32_t)img[a5 + 2] << 8) | img[a5 + 1]) & 0xffff;
        uint32_t a0 = A_ELEM_BUILD2 + d0 * 2;
        if (t & 0x80)
            func_12b04(img, a0, a1, d2, d3, 0x28);
        else
            func_12ad6(img, a0, a1, d2, d3, 0x28);
        a5 += 3;
        if (a5 == end)
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
void func_12d96(uint8_t *img) {
    uint32_t a1 = func_12a4a(img, 0);
    uint32_t d7;
    uint32_t a0 = screen_address(img, 0x10, be16(img + A_ELEM_1A530), &d7);
    blit_rect(img, a0, a1 + 0x12, 0xb, 0x3b, d7, 0x28);

    a1 = func_12a4a(img, 1);
    a0 = screen_address(img, 0x2d, be16(img + A_ELEM_1A530), &d7);
    blit_rect(img, a0, a1, 0xb, 0x3b, d7, 0x28);
}

/* FUN_00012dd8 @ 0x12dd8: como 0x12d96 pero con 0x124da (que espera VBL) y x=0x10/0x24,
 * 0x14 columnas. */
void func_12dd8(uint8_t *img) {
    uint32_t a1 = func_12a4a(img, 0);
    uint32_t d7;
    uint32_t a0 = screen_address(img, 0x10, be16(img + A_ELEM_1A530), &d7);
    blit_rect_vbl(img, a0, a1, 0x14, 0x3b, d7);

    a1 = func_12a4a(img, 1);
    a0 = screen_address(img, 0x24, be16(img + A_ELEM_1A530), &d7);
    blit_rect_vbl(img, a0, a1, 0x14, 0x3b, d7);
}

/* FUN_00012ea2 @ 0x12ea2: lee la caja 0x1ab40..0x1ab43, fija 0x1a56a=1, 0x1a577=0x11,
 * A6 += 1 y cae en 0x12e2e. */
uint32_t func_12ea2(uint8_t *img, uint32_t a6) {
    uint32_t d0 = img[A_DAT_1AB40];
    uint32_t d1 = img[A_DAT_1AB42];
    uint32_t d2 = (img[A_DAT_1AB41] - d0) & 0xffff;
    uint32_t d3 = (img[A_DAT_1AB43] - d1) & 0xffff;
    wr16(img + A_ANIM_1A56A, 1);
    img[A_ANIM_1A577] = 0x11;
    reveal_region(img, d0, d1, d2, d3);
    return a6 + 1;
}

/* FUN_00012e08 @ 0x12e08: si 0x1a570 != 0 cae en 0x1222e; si no, selecciona el icono
 * A6[1] (0x120e4), fija 0x1a577=0x11, A6 += 2 y 0x1a56a=0x64 y cae en 0x12e2e. */
uint32_t func_12e08(uint8_t *img, uint32_t a6) {
    if (img[A_SKIP_TRANSITION] != 0) {
        uint32_t a6o;
        hint_blit_entry(img, a6, &a6o);
        return a6o;
    }
    uint32_t o[8];
    hint_select_off(img, img[a6 + 1], o);
    img[A_ANIM_1A577] = 0x11;
    a6 += 2;
    wr16(img + A_ANIM_1A56A, 0x64);
    reveal_region(img, o[3], o[4], o[5], o[6]);
    return a6;
}



/* FUN_000129d6 @ 0x129d6: recorre la lista de bloques de 3 bytes en [A5,A4): cada
 * bloque elige la entrada (byte 0 & 0x7f) de 0x31072 (0x10f8e), lee [filas][cols] y
 * un offset de 14 bits (bytes 2 y 1, mascarado 0x3fff; bit7 de +2 invierte, bit6
 * reblitea hacia atras 0x12b04) y blitea en 0x3b1b0+offset*2 con stride 0x1a534. */
uint32_t advance_text_entry(uint8_t *img, uint32_t a1, uint32_t d4);
void func_12ad6(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t d2, uint32_t d3,
                 uint32_t a3);
void func_12b04(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t d2, uint32_t d3,
                 uint32_t a3);

void func_129d6(uint8_t *img, uint32_t a5, uint32_t a4) {
    for (;;) {
        uint32_t d4 = img[a5];
        uint32_t a1 = advance_text_entry(img, 0x31072, d4);
        uint32_t d3 = img[a1++];
        uint32_t d2 = img[a1++];
        uint32_t d0 = (((uint32_t)img[a5 + 2] << 8) | img[a5 + 1]) & 0x3fffu;
        uint32_t a0 = 0x3b1b0 + d0 * 2;
        uint32_t d5 = be32(img + A_DAT_1A534);
        if (img[a5 + 2] & 0x80) {
            uint32_t d4m = (d5 * d3) & 0xffffffffu;
            a0 = (a0 + d4m - d5) & 0xffffffffu;
            d5 = ((~d5) + 1) & 0xffffffffu;
        }
        if (img[a5 + 2] & 0x40)
            func_12b04(img, a0, a1, d2, d3, d5);
        else
            func_12ad6(img, a0, a1, d2, d3, d5);
        a5 += 3;
        if (a5 == a4)
            break;
    }
}

/* FUN_00012ce4 @ 0x12ce4: ciclo de "modo/vista" $1acca (0..3). Cada rama fija
 * $1acc6, corre 0x14496 y pinta con los blitters 111b6/114ba/117fe/11afa (o sus
 * envoltorios). Los casos 1/3 guardan el registro final de los blitters en los
 * bloques 0x1a82e/0x1a83e (movem.l d0/d2/a0-a1), que los casos 0/2 recargan.
 * Frontera: 0x12ce4..0x12d94/0x12c06/0x12c6c/0x12ce2; el rts de la rama 3 es
 * 0x12d94. Devuelve A6 = entry+1. */
void func_18f8e(uint8_t *img, uint32_t d4);
void func_18fa4(uint8_t *img);
uint32_t func_12a4a(uint8_t *img, uint32_t d1);
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

uint32_t view_mode_cycle(uint8_t *img, uint32_t a6) {
    func_18f8e(img, 0x1d);
    uint32_t a6ret = a6 + 1;
    uint32_t d0 = (img[0x1acca] + 1) & 3;
    img[0x1acca] = (uint8_t)d0;

    if (d0 == 1) {
        uint32_t d1 = 0x09;
        if (img[0x1acc6] != 0x18)
            d1 = 0x66;
        img[0x1acc6] = (uint8_t)d1;
        uint32_t a4o, a5o;
        scene_mode_load(img, &a4o, &a5o);

        uint32_t a1 = func_12a4a(img, 0);
        a1 = (a1 + be32(img + 0x1a534) - 2) & 0xffffffffu;
        wr16(img + 0x1a532, 0x10);
        uint32_t d7;
        uint32_t a0 = screen_address(img, 0x10, 0x20, &d7);
        blit_rect_vbl(img, a0, a1, 1, 0x3b, d7);
        a1 = (a1 - 2) & 0xffffffffu;
        /* el original llega con D0=0x07fd0000|fase y D2=0xffff0001 (valores de
         * 14496/12a4a/124da, independientes del contenido de pantalla) */
        uint32_t d0in = 0x07fd0000u | (d7 & 0xffff);
        uint32_t d2in = 0xffff0001u;
        overlay_compose_planes_wrap(img, d0in, 0x0a, d2in, 0x3b, a0, a1);
        {
            uint32_t c = 0, k = 0x27;
            for (;;) {
                if ((k & 3) == 0) c++;
                if (k == 0) break;
                k = (k - 1) & 0xffff;
            }
            wr32(img + 0x1a82e, d0in);
            wr32(img + 0x1a832, (d2in & 0xffff0000u) | ((5 + 0x28) & 0xffff));
            wr32(img + 0x1a836, a0);
            wr32(img + 0x1a83a, (a1 - 2 * c) & 0xffffffffu);
        }

        a1 = func_12a4a(img, 1);
        wr16(img + 0x1a532, 0x37);
        a0 = screen_address(img, 0x37, 0x20, &d7);
        blit_rect_vbl(img, a0, a1, 1, 0x3b, d7);
        if (d7 == 0) {
            d7 = 0x0f;
            a0 = (a0 - 8) & 0xffffffffu;
        } else {
            d7 = (d7 - 1) & 0xffff;
        }
        a1 = (a1 + 2) & 0xffffffffu;
        /* D0=0x45610000|fase2, D2=0xffff0001; D6=0x05010506 (constantes medidas) */
        uint32_t d0b = 0x45610000u | (d7 & 0xffff);
        uint32_t d2b = 0xffff0001u;
        overlay_compose_planes32_wrap(img, d0b, 0x0a, d2b, 0x3b, 0x05010506u, a0, a1);
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
            wr32(img + 0x1a846, (a0 - 8 * wraps) & 0xffffffffu);
            wr32(img + 0x1a84a, (a1 + 2 * c) & 0xffffffffu);
        }
        func_18fa4(img);
        return a6ret;
    }
    if (d0 == 2) {
        uint32_t d1 = 0x5f;
        if (img[0x1acc6] != 0x09)
            d1 = 0x67;
        img[0x1acc6] = (uint8_t)d1;
        uint32_t a4o, a5o;
        scene_mode_load(img, &a4o, &a5o);

        (void)func_12a4a(img, 0);
        uint32_t b0 = be32(img + 0x1a82e), b2 = be32(img + 0x1a832);
        uint32_t ba0 = be32(img + 0x1a836), ba1 = be32(img + 0x1a83a);
        overlay_compose_planes(img, b0, 0x23, b2, 0x3b, ba0, ba1);

        (void)func_12a4a(img, 1);
        b0 = be32(img + 0x1a83e); b2 = be32(img + 0x1a842);
        ba0 = be32(img + 0x1a846); ba1 = be32(img + 0x1a84a);
        overlay_compose_planes32(img, b0, 0x23, b2, 0x3b, 0xff34u, ba0, ba1);

        func_18f8e(img, 0x1e);
        return a6ret;
    }
    if (d0 == 0) {
        uint32_t d1 = 0x18;
        if (img[0x1acc6] != 0x09)
            d1 = 0x1e;
        img[0x1acc6] = (uint8_t)d1;
        uint32_t a4o, a5o;
        scene_mode_load(img, &a4o, &a5o);
        uint32_t b0 = be32(img + 0x1a83e), b2 = be32(img + 0x1a842);
        uint32_t ba0 = be32(img + 0x1a846), ba1 = be32(img + 0x1a84a);
        overlay_compose_masked(img, b0, 0x2b, b2, 0x3b, ba0, ba1);
        mask_restore_rows(img, ba0, 0x14, 0x3b, b0 & 0xffff);
        b0 = be32(img + 0x1a82e); b2 = be32(img + 0x1a832);
        ba0 = be32(img + 0x1a836); ba1 = be32(img + 0x1a83a);
        overlay_compose_shift(img, b0, 0x2b, b2, 0x3b, ba0, ba1);
        mask_restore_rows(img, ba0, 0x14, 0x3b, b0 & 0xffff);
        func_18fa4(img);
        return a6ret;
    }
    if (d0 == 3) {
        uint32_t d1 = 0x09;
        if (img[0x1acc6] != 0x5f)
            d1 = 0x66;
        img[0x1acc6] = (uint8_t)d1;
        uint32_t a4o, a5o;
        scene_mode_load(img, &a4o, &a5o);
        wr32(img + 0x1a534, 0x28);

        uint32_t d7;
        uint32_t a0a = screen_address(img, 0x24, 0x20, &d7);
        overlay_compose_masked_wrap(img, d7, 0x09, 0x14, 0x3b, a0a);
        /* registro final de 117de: D0=(d0&0xf+0x24)&0xf, D2=0x50-0x24,
         * A0/A1 con +8 por cada vuelta del nibble (2), A1 = A0-0x8000 */
        wr32(img + 0x1a83e, (((d7 & 0xf) + 0x24) & 0xf) | (d7 & 0xffffffF0u));
        wr32(img + 0x1a842, (((0x14 << 2) - 0x24) & 0xffff));
        wr32(img + 0x1a846, (a0a + 0x10) & 0xffffffffu);
        wr32(img + 0x1a84a, ((a0a - 0x8000) + 0x10) & 0xffffffffu);

        uint32_t a0b = screen_address(img, 0x10, 0x20, &d7);
        overlay_compose_shift_wrap(img, d7, 0x09, 0x14, 0x3b, a0b);
        wr32(img + 0x1a82e, d7 & 0xffffffffu);
        wr32(img + 0x1a832, (((0x14 << 2) - 0x24) & 0xffff));
        wr32(img + 0x1a836, a0b);
        wr32(img + 0x1a83a, (a0b - 0x8000) & 0xffffffffu);
        func_18fa4(img);
        return a6ret;
    }
    return a6ret;
}
