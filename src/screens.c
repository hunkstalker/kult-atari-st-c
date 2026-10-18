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
uint32_t copy_screen_a6(uint8_t *img, uint32_t a6) {
    a6 += 1;
    copy_screen(img);
    return a6;
}

/* 0x121de — intercambia screen_ptr_a y screen_ptr_b long a long. */
void swap_screens(uint8_t *img) {
    uint32_t a0 = be32(img + A_SCREEN_PTR_A);
    uint32_t a1 = be32(img + A_SCREEN_PTR_B);
    for (uint32_t i = 0; i < SCREEN_LONGS; i++) {
        uint32_t d1 = be32(img + a0);
        wr32(img + a0, be32(img + a1));
        wr32(img + a1, d1);
        a0 += 4;
        a1 += 4;
    }
}

/* 0x10ff4 — cuerpo común: A0 += ((d0 & 0xfff0) >> 1) + (d1 & 0xffff)*0xa0;
 * D7 = d0 & 0xf. Devuelve A0 y entrega D7. */
uint32_t screen_offset(uint8_t *img, uint32_t a0, uint32_t d0, uint32_t d1, uint32_t *d7) {
    (void)img;
    d0 &= 0xffff;
    d1 &= 0xffff;
    *d7 = d0 & 0xf;
    d0 &= 0xfff0;
    d0 >>= 1;
    return a0 + d0 + d1 * SCREEN_ROW;
}

/* 0x10fee — screen_address_raw: A0 = screen_ptr_a y cae en screen_offset. */
uint32_t screen_address_raw(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t *d7) {
    return screen_offset(img, be32(img + A_SCREEN_PTR_A), d0, d1, d7);
}

/* 0x10fec — screen_address: `lsl.w #2,d0` y cae en screen_address_raw. */
uint32_t screen_address(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t *d7) {
    return screen_address_raw(img, (d0 << 2) & 0xffff, d1, d7);
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
#include <stdint.h>

#include "machine.h"
#include "addrs.h"
#include "os.h"

#define SCREEN_LONGS 8000



/* FUN_000121c4 @ 0x121c4: `addq.l #1,a6` then falls into copy_screen; the copy is identical. */


/* screen_address @ 0x10fec: ST planar address of (x, y) on screen_ptr_a.
 * d0 = x pixels, d1 = y rows; returns the byte address in a0 (d7 = pixel-in-word). */
uint32_t screen_address_off(uint8_t *img, uint32_t d0, uint32_t d1) {
    uint32_t a0 = be32(img + A_SCREEN_PTR_A);
    d0 = (d0 << 2) & 0xffff;
    d1 = d1 & 0xffff;
    d0 &= 0xfff0;          /* word-aligned part (d7 = d0 & 0xf is the bit position) */
    d0 >>= 1;
    return a0 + d0 + d1 * 0xa0u;
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
void func_159bc(uint8_t *img, uint32_t a1, uint32_t a2, uint32_t d3) {
    uint32_t a0 = a2;
    uint32_t n = d3 & 0xffff;
    do {
        for (int i = 0; i < 4; i++) {
            wr32(img + a0, be32(img + a1));
            a0 += 4;
            a1 += 4;
        }
        a0 += 0x90;
        n = (n - 1) & 0xffff;
    } while (n != 0xffff);
}

/* FUN_0001350c @ 0x1350c: salta a la plantilla número (a6[2]<<8 | a6[1]): a6 = TEMPL+d0.
 * El word leído es el destino (no se usa aquí). Devuelve el A6 nuevo por out-param. */
void func_1350c(uint8_t *img, uint32_t *a6) {
    uint32_t d0 = ((img[*a6 + 2] << 8) | img[*a6 + 1]) & 0xffff;
    *a6 = A_TEMPL_DATA + d0;
}

/* FUN_00013522 @ 0x13522: como 0x1350c pero antes apila el A6 de retorno (a6+3) y el fin
 * de script actual ($1a4e0) en la pila de llamadas en 0x1a7d6. */
void func_13522(uint8_t *img, uint32_t *a6) {
    uint32_t a1 = be32(img + A_CALL_STACK);
    uint32_t d0 = ((img[*a6 + 2] << 8) | img[*a6 + 1]) & 0xffff;
    wr32(img + a1 - 4, *a6 + 3);
    wr32(img + a1 - 8, be32(img + A_TEMPL_PTR));
    wr32(img + A_CALL_STACK, a1 - 8);
    *a6 = A_TEMPL_DATA + d0;
}

/* FUN_00013552 @ 0x13552: desapila de 0x1a7d6: restaura $1a4e0 y A6 (vuelve de 0x13522). */
void func_13552(uint8_t *img, uint32_t *a6) {
    uint32_t a1 = be32(img + A_CALL_STACK);
    wr32(img + A_TEMPL_PTR, be32(img + a1));
    *a6 = be32(img + a1 + 4);
    wr32(img + A_CALL_STACK, a1 + 8);
}

/* load_fond @ 0x13568: fija el nombre 0x1a41e y carga una pantalla de 0x7d22 bytes en
 * screen_ptr_b-0x22 (cae en load_named_screen). FUN_00013572 @ 0x13572: solo estas dos
 * últimas asignaciones (el nombre lo trae ya quien llame). */
void load_resource(uint8_t *img);
void load_fond(uint8_t *img) {
    wr32(img + A_CUR_FILENAME, 0x1a41e);
    wr32(img + A_LOAD_BUFFER, (be32(img + A_SCREEN_PTR_B) - 0x22) & 0xffffffffu);
    wr32(img + A_LOAD_LENGTH, 0x7d22);
    load_resource(img);
}

void load_named_screen(uint8_t *img) {
    wr32(img + A_LOAD_BUFFER, (be32(img + A_SCREEN_PTR_B) - 0x22) & 0xffffffffu);
    wr32(img + A_LOAD_LENGTH, 0x7d22);
    load_resource(img);
}

/* FUN_000121b6 @ 0x121b6: copia 0x1f40 longs de screen_ptr_a a screen_ptr_b (la inversa
 * de copy_screen / 0x121c6). */
void func_121b6(uint8_t *img) {
    uint32_t a1 = be32(img + A_SCREEN_PTR_B);
    uint32_t a0 = be32(img + A_SCREEN_PTR_A);
    for (uint32_t n = 0x1f40; n != 0; n--) {
        wr32(img + a1, be32(img + a0));
        a0 += 4;
        a1 += 4;
    }
}

/* FUN_00012190 @ 0x12190: restaura D3 filas desde la capa delantera (screen_ptr_a) a la
 * trasera: precalcula máscaras (0x12698) y blitea cada fila (0x123c0) con a0 = origen-0x8000
 * y a1 = origen. */
void blit_mask_setup(uint8_t *img, uint32_t d2, uint32_t d7);
void blit_mask_row(uint8_t *img, uint32_t a0, uint32_t a1);
void func_12190(uint8_t *img, uint32_t a0, uint32_t d2, uint32_t d3, uint32_t d7) {
    blit_mask_setup(img, d2, d7);
    uint32_t n = (d3 - 1) & 0xffff;
    a0 = (a0 - 0x8000) & 0xffffffffu;
    for (;;) {
        blit_mask_row(img, a0, a0 + 0x8000);
        a0 += 0xa0;
        n = (n - 1) & 0xffff;
        if (n == 0xffff) break;
    }
}

/* FUN_00016b76 @ 0x16b76: copia D3+1 filas de (D2+1)*8 bytes desde A3-0x8000 a A3, con
 * stride de destino 0xa0. */
void func_16b76(uint8_t *img, uint32_t a3, uint32_t d2, uint32_t d3) {
    uint32_t a2 = a3;
    for (uint32_t ro = d3 & 0xffff;; ro = (ro - 1) & 0xffff) {
        uint32_t a0 = a2;
        uint32_t a1 = (a0 - 0x8000) & 0xffffffffu;
        for (uint32_t ci = d2 & 0xffff;; ci = (ci - 1) & 0xffff) {
            wr32(img + a0, be32(img + a1)); a0 += 4; a1 += 4;
            wr32(img + a0, be32(img + a1)); a0 += 4; a1 += 4;
            if (ci == 0) break;
        }
        a2 += 0xa0;
        if (ro == 0) break;
    }
}

/* FUN_00016b2e @ 0x16b2e: OR de una fila de (D2+1) grupos de 4 words desde el
 * descriptor 0xd8f0+A5 sobre el destino A3: por grupo lee 4 words (movem.w), calcula
 * la mascara ~(d4|d5|d6|d7) y escribe ((dst & mask) | word) en 4 words consecutivos.
 * Destino avanza 0xa0 por fila (D3+1 filas), fuente 0x40. */
void func_16b2e(uint8_t *img, uint32_t d2, uint32_t d3, uint32_t a3, uint32_t a5) {
    uint32_t a1 = A_SAVE_DESC + a5;
    uint32_t a2 = a3;
    uint32_t groups = d2 & 0xffff;
    uint32_t rows = d3 & 0xffff;
    for (;;) {
        uint32_t a0 = a2;
        uint32_t a4 = a1;
        for (uint32_t g = groups;; g = (g - 1) & 0xffff) {
            uint32_t d4 = be16(img + a4); a4 += 2;
            uint32_t d5 = be16(img + a4); a4 += 2;
            uint32_t d6 = be16(img + a4); a4 += 2;
            uint32_t d7 = be16(img + a4); a4 += 2;
            uint32_t mask = (~(d4 | d5 | d6 | d7)) & 0xffff;
            wr16(img + a0, (be16(img + a0) & mask) | d4); a0 += 2;
            wr16(img + a0, (be16(img + a0) & mask) | d5); a0 += 2;
            wr16(img + a0, (be16(img + a0) & mask) | d6); a0 += 2;
            wr16(img + a0, (be16(img + a0) & mask) | d7); a0 += 2;
            if (g == 0) break;
        }
        a2 += 0xa0;
        a1 += 0x40;
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

static void screen_copies(uint8_t *img, uint32_t a3) {
    a3 += 0x150;
    uint32_t a4 = 0x19900;
    uint32_t a2 = a3;
    uint32_t d7w = 0x1e0;
    for (uint32_t d6 = 0x34;; d6 = (d6 - 1) & 0xffff) {
        uint32_t a1 = 0x2705e + (d7w & 0xffff);
        d7w = ((d7w << 16) | (d7w >> 16));
        func_159bc(img, a1, a2, 0x1d);
        if (img[0x1a573] != 0)
            wait_vbl(img);
        /* El original suma con `add.w`: el acarreo NO pasa a la palabra alta,
         * asi que A2 se mueve dentro de la ventana de 64K de su base. */
        a2 = (a2 & 0xffff0000u) | ((a2 + be16(img + a4)) & 0xffff);
        a4 += 2;
        if (d6 == 0)
            break;
    }
    a2 = a3 + 0x7070;
    for (uint32_t d6 = 8;; d6 = (d6 - 1) & 0xffff) {
        func_159bc(img, 0x2705e, a2, 8);
        a2 += 0x10;
        if (d6 == 0)
            break;
    }
}

void func_15944(uint8_t *img) {
    uint32_t a3 = be32(img + A_SCREEN_PTR_B);
    img[0x1a573] = 0;
    screen_copies(img, a3);
}

void func_15934(uint8_t *img) {
    img[0x1a573] = 1;
    screen_copies(img, be32(img + A_SCREEN_PTR_A));
}

/* FUN_00018a30 @ 0x18a30: avanza la pista (0x18a10), limpia 0x500 bytes en
 * screen_ptr_a-0x500 y alterna el Setscreen a esa zona y de vuelta 5 veces (2 VBLs
 * por vuelta); al final copia 0x200 bytes desde screen_ptr_a+0x7b00 a la zona. */
void func_18a10(uint8_t *img);
void wait_vbl(uint8_t *img);

void func_18a30(uint8_t *img) {
    func_18a10(img);
    uint32_t a5 = be32(img + A_SCREEN_PTR_A);
    uint32_t a4 = (a5 - 0x500) & 0xffffffffu;
    {
        uint32_t a3 = a4;
        for (uint32_t d4 = 0x13f;; d4 = (d4 - 1) & 0xffff) {
            wr32(img + a3, 0);
            a3 += 4;
            if (d4 == 0) break;
        }
    }
    for (uint32_t d5 = 4;; d5 = (d5 - 1) & 0xffff) {
        os_setscreen(a4, a4, -1);
        wait_vbl(img);
        os_setscreen(a5, a5, -1);
        wait_vbl(img);
        if (d5 == 0) break;
    }
    a5 += 0x7b00;
    for (uint32_t d4 = 0x7f;; d4 = (d4 - 1) & 0xffff) {
        wr32(img + a4, be32(img + a5));
        a4 += 4;
        a5 += 4;
        if (d4 == 0) break;
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
void func_13bc2(uint8_t *img, uint32_t d2in, uint32_t d3in, uint32_t d7, uint32_t a1in);
void set_palette(uint8_t *img);
void copy_screen(uint8_t *img);
void wait_vbl(uint8_t *img);
void func_18f8e(uint8_t *img, uint32_t d4);
void func_182f0(uint8_t *img);

uint32_t func_18a98(uint8_t *img, uint32_t a6) {
    img[0x1ad0d] = 5;
    uint32_t a1 = be32(img + A_SCREEN_PTR_B);
    for (uint32_t d1 = 0x1f3f;; d1 = (d1 - 1) & 0xffff) {
        wr32(img + a1, 0);
        a1 += 4;
        if (d1 == 0)
            break;
    }
    copy_screen(img);

    uint32_t a6o, d0f, d1f;
    elem_command(img, 0x19714, &a6o, &d0f, &d1f);
    wr16(img + 0x1a544, (be16(img + A_ELEM_1A52C) - 1) & 0xffff);
    wr16(img + 0x1a542, ((be16(img + A_ELEM_1A52E) << 2) - 1) & 0xffff);
    wr16(img + 0x1a4a8, 0xc7);
    wr16(img + 0x1a84e, 0x2710);

    uint32_t ae = (a6o + 4) & 0xffffffffu;   /* la 1a entrada no relee D0/D1 */
    int first_entry = 1;
    for (;;) {
        uint32_t d0, d1;
        if (first_entry) {
            d0 = d0f;
            d1 = d1f;
            wr16(img + 0x1a4aa, be16(img + ae));
            uint32_t d3f = (be16(img + ae + 2) >> 1) & 0xffff;
            wr16(img + 0x1a4ac, d3f);
            wr16(img + 0x1a4a6, (d3f + d1) & 0xffff);
            wr16(img + 0x1a4a8, (be16(img + 0x1a4a8) - d1 + 1) & 0xffff);
        } else {
            d0 = be16(img + ae);
            d1 = be16(img + ae + 2);
            wr16(img + 0x1a4aa, be16(img + ae + 4));
            uint32_t d3f = (be16(img + ae + 6) >> 1) & 0xffff;
            wr16(img + 0x1a4ac, d3f);
            wr16(img + 0x1a4a6, (d3f + d1) & 0xffff);
            wr16(img + 0x1a4a8, (be16(img + 0x1a4a8) - d1 + 1) & 0xffff);
        }

        uint32_t d7;
        uint32_t a0 = screen_address_raw(img, d0, d1, &d7);
        uint32_t d0m = (d0 & 0xfff0u) >> 1;
        wr32(img + 0x1a4ba, (a0 - d0m) & 0xffffffffu);
        wr16(img + 0x1a486, d7);
        func_13bc2(img, be16(img + 0x1a4ac), be16(img + 0x1a4aa), d7,
                    (a0 - 0x8000) & 0xffffffffu);

        if (img[0x1a571] == 0) {
            img[0x1a571] = 1;
            uint32_t aa = (be32(img + 0x1a4ba) - 0x8000) & 0xffffffffu;
            uint32_t d2 = 0x13, d3b = 0x36;
            uint32_t a4 = (aa - 0xa0) & 0xffffffffu;
            for (uint32_t ro = d3b;; ro = (ro - 1) & 0xffff) {
                uint32_t a2 = aa, a5 = a4;
                for (uint32_t ci = d2;; ci = (ci - 1) & 0xffff) {
                    wr32(img + a5, be32(img + a2));
                    wr32(img + a5 + 4, be32(img + a2 + 4));
                    a5 += 8;
                    a2 += 8;
                    if (ci == 0)
                        break;
                }
                aa += 0xa0;
                a4 = (a4 - 0xa0) & 0xffffffffu;
                if (ro == 0)
                    break;
            }
            {
                uint32_t a0p = 0x1a1d2;
                uint32_t a1p = be32(img + A_PALETTE_PTR);
                for (uint32_t d0p = 7;; d0p = (d0p - 1) & 0xffff) {
                    wr32(img + a1p, be32(img + a0p));
                    a1p += 4;
                    a0p += 4;
                    if (d0p == 0)
                        break;
                }
            }
            set_palette(img);
            uint32_t a3 = (be32(img + A_SCREEN_PTR_A) + 0x7c60) & 0xffffffffu;
            uint32_t d5 = 0x13;
            for (uint32_t d7b = 0x6b;; d7b = (d7b - 1) & 0xffff) {
                uint32_t d6 = d5;
                uint32_t a0c = a4, a2c = a3;
                for (;;) {
                    wr32(img + a2c, be32(img + a0c));
                    wr32(img + a2c + 4, be32(img + a0c + 4));
                    a2c += 8;
                    a0c += 8;
                    if (d6 == 0)
                        break;
                    d6 = (d6 - 1) & 0xffff;
                }
                a3 = (a3 - 0xa0) & 0xffffffffu;
                d5 = (d5 + 0x14) & 0xffff;
                wait_vbl(img);
                wait_vbl(img);
                if (d7b == 0)
                    break;
            }
            a3 = (a3 - 0x8000) & 0xffffffffu;
            for (uint32_t d7c = 0x897;; d7c = (d7c - 1) & 0xffff) {
                wr32(img + a3, 0);
                a3 += 4;
                if (d7c == 0)
                    break;
            }
            for (uint32_t d7d = 0xea60;; d7d = (d7d - 1) & 0xffff) {
                if (d7d == 0)
                    break;
            }
            func_18f8e(img, 0xf0);
        }

        {
            uint32_t a0 = be32(img + 0x1a4ba);
            uint32_t d2 = 0x13;
            uint32_t d3 = be16(img + 0x1a4a8);
            uint32_t a1b = (a0 - 0x8000) & 0xffffffffu;
            uint32_t a4b = (a0 - 0xa0) & 0xffffffffu;
            for (uint32_t ro = d3;; ro = (ro - 1) & 0xffff) {
                uint32_t d4 = d2;
                uint32_t a2 = a0, a3 = a1b, a5 = a4b;
                for (;;) {
                    wr32(img + a2, be32(img + a3));
                    wr32(img + a5, be32(img + a3));
                    wr32(img + a3, 0);
                    a2 += 4; a5 += 4; a3 += 4;
                    wr32(img + a2, be32(img + a3));
                    wr32(img + a5, be32(img + a3));
                    wr32(img + a3, 0);
                    a2 += 4; a5 += 4; a3 += 4;
                    if (d4 == 0)
                        break;
                    d4 = (d4 - 1) & 0xffff;
                }
                a0 += 0xa0;
                a1b += 0xa0;
                a4b = (a4b - 0xa0) & 0xffffffffu;
                if (ro == 0)
                    break;
            }
        }
        wr16(img + 0x1a4a8, be16(img + 0x1a4a6));
        wait_vbl(img);
        {
            uint32_t dd = be16(img + 0x1a84e);
            for (;;) {
                dd = (dd - 1) & 0xffff;
                if (dd == 0)
                    break;
            }
        }
        wr16(img + 0x1a84e, (be16(img + 0x1a84e) + 0x1f4) & 0xffff);
        ae += first_entry ? 4 : 8;
        first_entry = 0;
        if (ae == 0x198c0)
            break;
    }

    func_182f0(img);
    return a6 + 1;
}
