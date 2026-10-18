/* screens.c — buffers de pantalla y direccionamiento planar.
 *
 *   copy_screen       @ 0x121c6 : copia screen_ptr_b -> screen_ptr_a (8000 longs).
 *   copy_screen_a6    @ 0x121c4 : `addq.l #1,a6` y cae en copy_screen.
 *   swap_screens      @ 0x121de : intercambia los dos buffers long a long.
 *   screen_address    @ 0x10fec : dirección planar de (x,y) sobre screen_ptr_a (d0<<2).
 *   screen_address_raw@ 0x10fee : idem sin el `lsl.w #2` (d0 ya escalado).
 *   screen_offset     @ 0x10ff4 : cuerpo común; suma el offset a A0 (dado por el llamante).
 *
 * Todo el efecto es la imagen (o el valor devuelto): sin hardware, verificable
 * por el diff de bytes. Las `screen_address*` devuelven A0 (dirección) y entregan
 * D7 (fase de píxel dentro de la palabra) por out-param, según PLAN §4.1
 * (primario = retorno, resto = out-param).
 */
#include <stdint.h>

#include "machine.h"
#include "addrs.h"

#define SCREEN_LONGS 8000   /* 0x1f40 longs = 32000 B (framebuffer baja resolución) */
#define SCREEN_ROW   0xa0   /* bytes por fila (160 B) */

/* 0x121c6 — copia 0x1f40 longs de screen_ptr_b a screen_ptr_a. */
void copy_screen(uint8_t *img) {
    uint32_t src = be32(img + A_SCREEN_PTR_B);
    uint32_t dst = be32(img + A_SCREEN_PTR_A);
    for (uint32_t i = 0; i < SCREEN_LONGS; i++)
        wr32(img + dst + i * 4, be32(img + src + i * 4));
}

/* 0x121c4 — `addq.l #1,a6` y cae en copy_screen. Devuelve A6 avanzado. */
uint32_t copy_screen_a6(uint8_t *img, uint32_t script) {
    script += 1;
    copy_screen(img);
    return script;
}

/* 0x121de — intercambia screen_ptr_a y screen_ptr_b long a long. */
void swap_screens(uint8_t *img) {
    uint32_t front = be32(img + A_SCREEN_PTR_A);
    uint32_t back = be32(img + A_SCREEN_PTR_B);
    for (uint32_t i = 0; i < SCREEN_LONGS; i++) {
        uint32_t tmp = be32(img + front);
        wr32(img + front, be32(img + back));
        wr32(img + back, tmp);
        front += 4;
        back += 4;
    }
}

/* 0x10ff4 — cuerpo común: A0 += ((d0 & 0xfff0) >> 1) + (d1 & 0xffff)*0xa0;
 * D7 = d0 & 0xf. Devuelve A0 y entrega D7. */
uint32_t screen_offset(uint8_t *img, uint32_t base, uint32_t x, uint32_t y, uint32_t *phase) {
    (void)img;
    x &= 0xffff;
    y &= 0xffff;
    *phase = x & 0xf;
    x &= 0xfff0;
    x >>= 1;
    return base + x + y * SCREEN_ROW;
}

/* 0x10fee — screen_address_raw: A0 = screen_ptr_a y cae en screen_offset. */
uint32_t screen_address_raw(uint8_t *img, uint32_t x, uint32_t y, uint32_t *phase) {
    return screen_offset(img, be32(img + A_SCREEN_PTR_A), x, y, phase);
}

/* 0x10fec — screen_address: `lsl.w #2,d0` y cae en screen_address_raw. */
uint32_t screen_address(uint8_t *img, uint32_t x, uint32_t y, uint32_t *phase) {
    return screen_address_raw(img, (x << 2) & 0xffff, y, phase);
}


/* ===== funciones restantes importadas de screens.c (viejo) ===== */
/* screens.c — the screen-buffer copy/swap family.
 *
 * copy_screen @ 0x121c6: 8000 longs from screen_ptr_b (back) to screen_ptr_a (front).
 * swap_screens @ 0x121de: exchange the two 32000-byte buffers longword by longword.
 *
 * Both read their operands from image globals and touch no hardware, so a differential
 * case pokes the two pointers and a couple of sentinel longs into the buffers.
 */

#include "os.h"

#define SCREEN_LONGS 8000



/* FUN_000121c4 @ 0x121c4: `addq.l #1,a6` then falls into copy_screen; the copy is identical. */


/* screen_address @ 0x10fec: ST planar address of (x, y) on screen_ptr_a.
 * d0 = x pixels, d1 = y rows; returns the byte address in a0 (d7 = pixel-in-word). */
uint32_t screen_address_off(uint8_t *img, uint32_t x, uint32_t y) {
    uint32_t base = be32(img + A_SCREEN_PTR_A);
    x = (x << 2) & 0xffff;
    y = y & 0xffff;
    x &= 0xfff0;          /* word-aligned part (d7 = x & 0xf is the bit position) */
    x >>= 1;
    return base + x + y * 0xa0u;
}

/* screen_address @ 0x10fec, exposición completa: como screen_address_off pero devuelve
 * también d7 = (d0<<2) & 0xf (la fase de píxel dentro de la palabra), que el texto
 * proporcional necesita. */


/* screen_address_raw @ 0x10fee: como screen_address pero sin el `lsl.w #2` (el
 * llamante ya pasa d0 listo). Devuelve a0 y deja el bit de pixel en d7. */


/* FUN_00010ff4 @ 0x10ff4: la misma aritmetica de 0x10fec/0x10fee pero SIN cargar
 * A0 (lo aporta el llamante); suma el offset de (d0,d1) a A0 y deja d7 = d0 & 0xf. */




/* FUN_000159bc @ 0x159bc: copy D3+1 rows of 16 bytes (4 longs) from A1, advancing
 * A1 by 16 a row, into A2 with a 0xa0 destination stride.  Pure image code. */
void row_palette(uint8_t *img, uint32_t src, uint32_t dest, uint32_t rows) {
    uint32_t dst = dest;
    uint32_t n = rows & 0xffff;
    do {
        for (int i = 0; i < 4; i++) {
            wr32(img + dst, be32(img + src));
            dst += 4;
            src += 4;
        }
        dst += 0x90;
        n = (n - 1) & 0xffff;
    } while (n != 0xffff);
}

/* FUN_0001350c @ 0x1350c: salta a la plantilla número (a6[2]<<8 | a6[1]): a6 = TEMPL+d0.
 * El word leído es el destino (no se usa aquí). Devuelve el A6 nuevo por out-param. */
void script_push(uint8_t *img, uint32_t *a6) {
    uint32_t target = ((img[*a6 + 2] << 8) | img[*a6 + 1]) & 0xffff;
    *a6 = A_TEMPL_DATA + target;
}

/* FUN_00013522 @ 0x13522: como 0x1350c pero antes apila el A6 de retorno (a6+3) y el fin
 * de script actual ($1a4e0) en la pila de llamadas en 0x1a7d6. */
void script_pop(uint8_t *img, uint32_t *a6) {
    uint32_t stack = be32(img + A_CALL_STACK);
    uint32_t target = ((img[*a6 + 2] << 8) | img[*a6 + 1]) & 0xffff;
    wr32(img + stack - 4, *a6 + 3);
    wr32(img + stack - 8, be32(img + A_TEMPL_PTR));
    wr32(img + A_CALL_STACK, stack - 8);
    *a6 = A_TEMPL_DATA + target;
}

/* FUN_00013552 @ 0x13552: desapila de 0x1a7d6: restaura $1a4e0 y A6 (vuelve de 0x13522). */
void script_call(uint8_t *img, uint32_t *a6) {
    uint32_t stack = be32(img + A_CALL_STACK);
    wr32(img + A_TEMPL_PTR, be32(img + stack));
    *a6 = be32(img + stack + 4);
    wr32(img + A_CALL_STACK, stack + 8);
}

/* load_fond @ 0x13568: fija el nombre 0x1a41e y carga una pantalla de 0x7d22 bytes en
 * screen_ptr_b-0x22 (cae en load_named_screen). FUN_00013572 @ 0x13572: solo estas dos
 * últimas asignaciones (el nombre lo trae ya quien llame). */
void load_resource(uint8_t *img);
/* 0x13568 — carga FOND.PI1 (fondo de sala) en screen_ptr_b */
void load_fond(uint8_t *img) {
    wr32(img + A_CUR_FILENAME, 0x1a41e);
    wr32(img + A_LOAD_BUFFER, (be32(img + A_SCREEN_PTR_B) - 0x22) & 0xffffffffu);
    wr32(img + A_LOAD_LENGTH, 0x7d22);
    load_resource(img);
}

/* 0x13572 — prepara buffer (len 0x7d22) y llama a load_resource */
void load_named_screen(uint8_t *img) {
    wr32(img + A_LOAD_BUFFER, (be32(img + A_SCREEN_PTR_B) - 0x22) & 0xffffffffu);
    wr32(img + A_LOAD_LENGTH, 0x7d22);
    load_resource(img);
}

/* FUN_000121b6 @ 0x121b6: copia 0x1f40 longs de screen_ptr_a a screen_ptr_b (la inversa
 * de copy_screen / 0x121c6). */
void copy_screen_rev(uint8_t *img) {
    uint32_t back = be32(img + A_SCREEN_PTR_B);
    uint32_t front = be32(img + A_SCREEN_PTR_A);
    for (uint32_t n = 0x1f40; n != 0; n--) {
        wr32(img + back, be32(img + front));
        front += 4;
        back += 4;
    }
}

/* FUN_00012190 @ 0x12190: restaura D3 filas desde la capa delantera (screen_ptr_a) a la
 * trasera: precalcula máscaras (0x12698) y blitea cada fila (0x123c0) con a0 = origen-0x8000
 * y a1 = origen. */
void blit_mask_setup(uint8_t *img, uint32_t d2, uint32_t d7);
void blit_mask_row(uint8_t *img, uint32_t a0, uint32_t a1);
/* 0x12190 — mask row blit */
void mask_row_blit(uint8_t *img, uint32_t src, uint32_t cols, uint32_t rows, uint32_t phase) {
    blit_mask_setup(img, cols, phase);
    uint32_t n = (rows - 1) & 0xffff;
    src = (src - 0x8000) & 0xffffffffu;
    for (;;) {
        blit_mask_row(img, src, src + 0x8000);
        src += 0xa0;
        n = (n - 1) & 0xffff;
        if (n == 0xffff) break;
    }
}

/* FUN_00016b76 @ 0x16b76: copia D3+1 filas de (D2+1)*8 bytes desde A3-0x8000 a A3, con
 * stride de destino 0xa0. */
void row_blit(uint8_t *img, uint32_t dest, uint32_t groups, uint32_t rows) {
    uint32_t dst = dest;
    for (uint32_t row = rows & 0xffff;; row = (row - 1) & 0xffff) {
        uint32_t dst2 = dst;
        uint32_t src = (dst2 - 0x8000) & 0xffffffffu;
        for (uint32_t col = groups & 0xffff;; col = (col - 1) & 0xffff) {
            wr32(img + dst2, be32(img + src)); dst2 += 4; src += 4;
            wr32(img + dst2, be32(img + src)); dst2 += 4; src += 4;
            if (col == 0) break;
        }
        dst += 0xa0;
        if (row == 0) break;
    }
}

/* FUN_00016b2e @ 0x16b2e: OR de una fila de (D2+1) grupos de 4 words desde el
 * descriptor 0xd8f0+A5 sobre el destino A3: por grupo lee 4 words (movem.w), calcula
 * la mascara ~(d4|d5|d6|d7) y escribe ((dst & mask) | word) en 4 words consecutivos.
 * Destino avanza 0xa0 por fila (D3+1 filas), fuente 0x40. */
void or_rows(uint8_t *img, uint32_t group_count, uint32_t row_count, uint32_t dest, uint32_t offset) {
    uint32_t desc = A_SAVE_DESC + offset;
    uint32_t dst = dest;
    uint32_t groups = group_count & 0xffff;
    uint32_t rows = row_count & 0xffff;
    for (;;) {
        uint32_t dst2 = dst;
        uint32_t src = desc;
        for (uint32_t group = groups;; group = (group - 1) & 0xffff) {
            uint32_t w0 = be16(img + src); src += 2;
            uint32_t w1 = be16(img + src); src += 2;
            uint32_t w2 = be16(img + src); src += 2;
            uint32_t w3 = be16(img + src); src += 2;
            uint32_t mask = (~(w0 | w1 | w2 | w3)) & 0xffff;
            wr16(img + dst2, (be16(img + dst2) & mask) | w0); dst2 += 2;
            wr16(img + dst2, (be16(img + dst2) & mask) | w1); dst2 += 2;
            wr16(img + dst2, (be16(img + dst2) & mask) | w2); dst2 += 2;
            wr16(img + dst2, (be16(img + dst2) & mask) | w3); dst2 += 2;
            if (group == 0) break;
        }
        dst += 0xa0;
        desc += 0x40;
        if (rows == 0) break;
        rows--;
    }
}

/* FUN_00015944 @ 0x15944: 0x35 copias de 0x1e (0x1d+1) filas de 16 bytes desde
 * 0x2723e + k*0x1e0 a screen_ptr_b+0x150+k*tabla(0x19900) (con espera si 0x1a573),
 * mas 9 copias de 9 filas desde 0x2705e a screen_ptr_b+0x7070+k*0x10. */
void wait_vbl(uint8_t *img);

/* FUN_00015944 @ 0x15944 y su variante 0x15934: 0x35 copias de 0x1e (0x1d+1) filas de
 * 16 bytes desde 0x2723e + k*0x1e0 a screen_ptr+0x150+k*tabla(0x19900) (con espera
 * si 0x1a573), mas 9 copias de 9 filas desde 0x2705e a screen_ptr+0x7070+k*0x10.
 * 0x15944 usa screen_ptr_b y limpia el flag; 0x15934 usa screen_ptr_a y lo deja a 1
 * (entra directo al bucle, saltandose la carga). */
void wait_vbl(uint8_t *img);

static void screen_copies(uint8_t *img, uint32_t base) {
    base += 0x150;
    uint32_t table = 0x19900;
    uint32_t dst = base;
    uint32_t off = 0x1e0;
    for (uint32_t i = 0x34;; i = (i - 1) & 0xffff) {
        uint32_t src = 0x2705e + (off & 0xffff);
        off = ((off << 16) | (off >> 16));
        row_palette(img, src, dst, 0x1d);
        if (img[0x1a573] != 0)
            wait_vbl(img);
        /* El original suma con `add.w`: el acarreo NO pasa a la palabra alta,
         * asi que A2 se mueve dentro de la ventana de 64K de su base. */
        dst = (dst & 0xffff0000u) | ((dst + be16(img + table)) & 0xffff);
        table += 2;
        if (i == 0)
            break;
    }
    dst = base + 0x7070;
    for (uint32_t i = 8;; i = (i - 1) & 0xffff) {
        row_palette(img, 0x2705e, dst, 8);
        dst += 0x10;
        if (i == 0)
            break;
    }
}

/* 0x15944 — mide y dibuja una caja de texto autodimensionada. 15944, walk_entry sobre la tabla 0x24982 (indice 0x1acc7-1), recorre bloques de 3 bytes: los bytes 0x32..0x... */
void copy_rows(uint8_t *img) {
    uint32_t back = be32(img + A_SCREEN_PTR_B);
    img[0x1a573] = 0;
    screen_copies(img, back);
}

/* 0x15934 — copy rows a */
void copy_rows_a(uint8_t *img) {
    img[0x1a573] = 1;
    screen_copies(img, be32(img + A_SCREEN_PTR_A));
}

/* FUN_00018a30 @ 0x18a30: avanza la pista (0x18a10), limpia 0x500 bytes en
 * screen_ptr_a-0x500 y alterna el Setscreen a esa zona y de vuelta 5 veces (2 VBLs
 * por vuelta); al final copia 0x200 bytes desde screen_ptr_a+0x7b00 a la zona. */
void track_advance(uint8_t *img);
void wait_vbl(uint8_t *img);

/* 0x18a30 — screen clean */
void screen_clean(uint8_t *img) {
    track_advance(img);
    uint32_t front = be32(img + A_SCREEN_PTR_A);
    uint32_t zone = (front - 0x500) & 0xffffffffu;
    {
        uint32_t p = zone;
        for (uint32_t i = 0x13f;; i = (i - 1) & 0xffff) {
            wr32(img + p, 0);
            p += 4;
            if (i == 0) break;
        }
    }
    for (uint32_t i2 = 4;; i2 = (i2 - 1) & 0xffff) {
        os_setscreen(zone, zone, -1);
        wait_vbl(img);
        os_setscreen(front, front, -1);
        wait_vbl(img);
        if (i2 == 0) break;
    }
    front += 0x7b00;
    for (uint32_t i = 0x7f;; i = (i - 1) & 0xffff) {
        wr32(img + zone, be32(img + front));
        zone += 4;
        front += 4;
        if (i == 0) break;
    }
}

/* FUN_00018a98 @ 0x18a98: "attract"/fondo animado. Limpia el back buffer y copia;
 * construye el elemento de 0x19714 (0x128a4), fija $1a544/$1a542 (alto-1/ancho*4-1),
 * $1a4a8=0xc7 y $1a84e=0x2710, y recorre 53 entradas de 4 words en 0x19714+4..0x198c0:
 * por entrada calcula la direccion (0x10fee), escala el fondo (0x13bc2), en la primera
 * pasada inicializa los buffers (copias de filas + paleta 0x1a1d2 + 16300 + 0x6c
 * pasadas con 2 VBLs), compone el fondo en pantalla (3.er bloque de copias), espera la
 * VBL, gasta $1a84e iteraciones y suma 0x1f4. Al acabar: 0x182f0 y el bucle infinito
 * 0x18c80 (0x1834a con d5=0x64) — frontera verificada hasta 0x18c80; el bucle final no
 * se transcribe (los tests cortan con stop_pc). Devuelve A6 = entry+1. */
uint32_t elem_command(uint8_t *img, uint32_t a6, uint32_t *a6o, uint32_t *d0o,
                        uint32_t *d1o);
uint32_t screen_address_raw(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t *d7out);
void scale_blit(uint8_t *img, uint32_t d2in, uint32_t d3in, uint32_t d7, uint32_t a1in);
void set_palette(uint8_t *img);
void copy_screen(uint8_t *img);
void wait_vbl(uint8_t *img);
void sound_play_a(uint8_t *img, uint32_t d4);
void icon_list_reset(uint8_t *img);

/* 0x18a98 — attract */
uint32_t attract(uint8_t *img, uint32_t a6) {
    img[0x1ad0d] = 5;
    uint32_t back = be32(img + A_SCREEN_PTR_B);
    for (uint32_t y = 0x1f3f;; y = (y - 1) & 0xffff) {
        wr32(img + back, 0);
        back += 4;
        if (y == 0)
            break;
    }
    copy_screen(img);

    uint32_t entry_out, x0, y0;
    elem_command(img, 0x19714, &entry_out, &x0, &y0);
    wr16(img + 0x1a544, (be16(img + A_ELEM_1A52C) - 1) & 0xffff);
    wr16(img + 0x1a542, ((be16(img + A_ELEM_1A52E) << 2) - 1) & 0xffff);
    wr16(img + 0x1a4a8, 0xc7);
    wr16(img + 0x1a84e, 0x2710);

    uint32_t entry = (entry_out + 4) & 0xffffffffu;   /* la 1a entrada no relee D0/D1 */
    int first = 1;
    for (;;) {
        uint32_t x, y;
        if (first) {
            x = x0;
            y = y0;
            wr16(img + 0x1a4aa, be16(img + entry));
            uint32_t half = (be16(img + entry + 2) >> 1) & 0xffff;
            wr16(img + 0x1a4ac, half);
            wr16(img + 0x1a4a6, (half + y) & 0xffff);
            wr16(img + 0x1a4a8, (be16(img + 0x1a4a8) - y + 1) & 0xffff);
        } else {
            x = be16(img + entry);
            y = be16(img + entry + 2);
            wr16(img + 0x1a4aa, be16(img + entry + 4));
            uint32_t half = (be16(img + entry + 6) >> 1) & 0xffff;
            wr16(img + 0x1a4ac, half);
            wr16(img + 0x1a4a6, (half + y) & 0xffff);
            wr16(img + 0x1a4a8, (be16(img + 0x1a4a8) - y + 1) & 0xffff);
        }

        uint32_t phase;
        uint32_t addr = screen_address_raw(img, x, y, &phase);
        uint32_t xoff = (x & 0xfff0u) >> 1;
        wr32(img + 0x1a4ba, (addr - xoff) & 0xffffffffu);
        wr16(img + 0x1a486, phase);
        scale_blit(img, be16(img + 0x1a4ac), be16(img + 0x1a4aa), phase,
                    (addr - 0x8000) & 0xffffffffu);

        if (img[0x1a571] == 0) {
            img[0x1a571] = 1;
            uint32_t base = (be32(img + 0x1a4ba) - 0x8000) & 0xffffffffu;
            uint32_t cols = 0x13, rows = 0x36;
            uint32_t up = (base - 0xa0) & 0xffffffffu;
            for (uint32_t row = rows;; row = (row - 1) & 0xffff) {
                uint32_t dst = base, up2 = up;
                for (uint32_t col = cols;; col = (col - 1) & 0xffff) {
                    wr32(img + up2, be32(img + dst));
                    wr32(img + up2 + 4, be32(img + dst + 4));
                    up2 += 8;
                    dst += 8;
                    if (col == 0)
                        break;
                }
                base += 0xa0;
                up = (up - 0xa0) & 0xffffffffu;
                if (row == 0)
                    break;
            }
            {
                uint32_t pal_src = 0x1a1d2;
                uint32_t pal_dst = be32(img + A_PALETTE_PTR);
                for (uint32_t pal_i = 7;; pal_i = (pal_i - 1) & 0xffff) {
                    wr32(img + pal_dst, be32(img + pal_src));
                    pal_dst += 4;
                    pal_src += 4;
                    if (pal_i == 0)
                        break;
                }
            }
            set_palette(img);
            uint32_t dst2 = (be32(img + A_SCREEN_PTR_A) + 0x7c60) & 0xffffffffu;
            uint32_t w = 0x13;
            for (uint32_t pass = 0x6b;; pass = (pass - 1) & 0xffff) {
                uint32_t n = w;
                uint32_t src = up, dst3 = dst2;
                for (;;) {
                    wr32(img + dst3, be32(img + src));
                    wr32(img + dst3 + 4, be32(img + src + 4));
                    dst3 += 8;
                    src += 8;
                    if (n == 0)
                        break;
                    n = (n - 1) & 0xffff;
                }
                dst2 = (dst2 - 0xa0) & 0xffffffffu;
                w = (w + 0x14) & 0xffff;
                wait_vbl(img);
                wait_vbl(img);
                if (pass == 0)
                    break;
            }
            dst2 = (dst2 - 0x8000) & 0xffffffffu;
            for (uint32_t clr = 0x897;; clr = (clr - 1) & 0xffff) {
                wr32(img + dst2, 0);
                dst2 += 4;
                if (clr == 0)
                    break;
            }
            for (uint32_t spin = 0xea60;; spin = (spin - 1) & 0xffff) {
                if (spin == 0)
                    break;
            }
            sound_play_a(img, 0xf0);
        }

        {
            uint32_t addr = be32(img + 0x1a4ba);
            uint32_t cols = 0x13;
            uint32_t rows2 = be16(img + 0x1a4a8);
            uint32_t lower = (addr - 0x8000) & 0xffffffffu;
            uint32_t upper = (addr - 0xa0) & 0xffffffffu;
            for (uint32_t row = rows2;; row = (row - 1) & 0xffff) {
                uint32_t n2 = cols;
                uint32_t dst = addr, dst2 = lower, up2 = upper;
                for (;;) {
                    wr32(img + dst, be32(img + dst2));
                    wr32(img + up2, be32(img + dst2));
                    wr32(img + dst2, 0);
                    dst += 4; up2 += 4; dst2 += 4;
                    wr32(img + dst, be32(img + dst2));
                    wr32(img + up2, be32(img + dst2));
                    wr32(img + dst2, 0);
                    dst += 4; up2 += 4; dst2 += 4;
                    if (n2 == 0)
                        break;
                    n2 = (n2 - 1) & 0xffff;
                }
                addr += 0xa0;
                lower += 0xa0;
                upper = (upper - 0xa0) & 0xffffffffu;
                if (row == 0)
                    break;
            }
        }
        wr16(img + 0x1a4a8, be16(img + 0x1a4a6));
        wait_vbl(img);
        {
            uint32_t delay = be16(img + 0x1a84e);
            for (;;) {
                delay = (delay - 1) & 0xffff;
                if (delay == 0)
                    break;
            }
        }
        wr16(img + 0x1a84e, (be16(img + 0x1a84e) + 0x1f4) & 0xffff);
        entry += first ? 4 : 8;
        first = 0;
        if (entry == 0x198c0)
            break;
    }

    icon_list_reset(img);
    return a6 + 1;
}
