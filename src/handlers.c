/* handlers.c — handlers de comando pequenos (0x121xx-0x127xx).
 *
 * FUN_0001228a @ 0x1228a: prompt_wait y avanza A6 en 1.
 * FUN_000121fa @ 0x121fa: selecciona el icono A6[1] (0x120e4), lo blitea (0x1215e),
 *   calcula la direccion de (x,y) del descriptor (0x10fec) y blitea 2x15 en ella;
 *   A6 += 2 y limpia 0x1a484.
 * FUN_00012436 @ 0x12436: copia 0x1a49e/0x1a4a0/0x1a4a2 a las mascaras 0x1a538/53a/53c
 *   y cae en el cuerpo de 0x12466 con a0=*(0x1a4b2), a1=0xd8f0, d3=*(0x1a4a4).
 * FUN_000127fa @ 0x127fa: guarda una fila enmascarada del buffer A0 en el descriptor
 *   0xd8f0 (previa 0x12698 + 0x123c0), D3 filas con stride 0x40.
 * FUN_000127e0 @ 0x127e0: A0 -= 0x8000, guarda (0x127fa) y reblitea (0x124da).
 */
#include <stdint.h>

#include "machine.h"
#include "addrs.h"

void prompt_wait(uint8_t *img);
void hint_select_off(uint8_t *img, uint32_t d0, uint32_t *o);
void mask_restore_rows(uint8_t *img, uint32_t a0, uint32_t d2, uint32_t d3, uint32_t d7);
uint32_t screen_address(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t *d7out);
void blit_mask_rows(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t d3);
void blit_mask_setup(uint8_t *img, uint32_t d2, uint32_t d7);
void blit_mask_row(uint8_t *img, uint32_t a0, uint32_t a1);
void blit_rect_vbl(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t d2, uint32_t d3,
                 uint32_t d7);
void func_12d96(uint8_t *img);
void func_12dd8(uint8_t *img);
void hint_blit_entry(uint8_t *img, uint32_t a6, uint32_t *a6_out);

uint32_t func_1228a(uint8_t *img, uint32_t a6) {
    prompt_wait(img);
    return a6 + 1;
}

uint32_t func_121fa(uint8_t *img, uint32_t a6) {
    uint32_t o[8];
    hint_select_off(img, img[a6 + 1], o);
    mask_restore_rows(img, o[0], o[5], o[6], o[2]);
    uint32_t d7;
    uint32_t a0 = screen_address(img, o[3], o[4], &d7);
    mask_restore_rows(img, a0, 2, 0xf, d7);
    wr16(img + A_DAT_1A484, 0);
    return a6 + 2;
}

void func_12436(uint8_t *img) {
    wr16(img + A_BLIT_MASK_FIRST, be16(img + A_DAT_1A49E));
    wr16(img + A_BLIT_MASK_LAST, be16(img + A_ANIM_1A4A0));
    wr16(img + A_BLIT_GROUPS, be16(img + A_SAVE_1A4A2));
    uint32_t d3 = be16(img + A_SAVE_1A4A4);
    uint32_t a0 = be32(img + A_DAT_1A4B2);
    blit_mask_rows(img, a0, A_SAVE_DESC, d3);
}

void func_127fa(uint8_t *img, uint32_t a0, uint32_t d2, uint32_t d3, uint32_t d7) {
    wr32(img + A_DAT_1A4B2, a0);
    wr16(img + A_SAVE_1A4A4, d3 & 0xffff);

    for (int i = 0; i < 0x627; i++)
        wr32(img + A_SAVE_DESC + (uint32_t)i * 4, 0);

    blit_mask_setup(img, d2, d7);
    wr16(img + A_DAT_1A49E, be16(img + A_BLIT_MASK_FIRST));
    wr16(img + A_ANIM_1A4A0, be16(img + A_BLIT_MASK_LAST));
    wr16(img + A_SAVE_1A4A2, be16(img + A_BLIT_GROUPS));

    uint32_t rows = (d3 - 1) & 0xffff;
    uint32_t a0d = A_SAVE_DESC;
    uint32_t a1 = a0;
    for (;;) {
        blit_mask_row(img, a0d, a1);
        a0d += 0x40;
        a1 += 0xa0;
        if (rows == 0)
            break;
        rows--;
    }
}

void func_127e0(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t d2, uint32_t d3,
                 uint32_t d7) {
    uint32_t ab = a0 - 0x8000;
    func_127fa(img, ab, d2, d3, d7);
    blit_rect_vbl(img, ab, a1, d2, d3, d7);
}

/* FUN_000134e4 @ 0x134e4: copia una rejilla desde A1 a pantalla en (D0,D1)-0x8000:
 * (D3) filas de (D2>>2) celdas de 8 bytes. Depende de 0x10fec (screen_address). */
void func_134e4(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3,
                 uint32_t a1) {
    d2 = (d2 >> 2) & 0xffff;
    d3 = (d3 - 1) & 0xffff;
    d2 = (d2 - 1) & 0xffff;
    uint32_t d7;
    uint32_t a0 = screen_address(img, d0, d1, &d7) - 0x8000;
    for (;;) {
        uint32_t c = d2;
        uint32_t a2 = a0;
        for (;;) {
            wr32(img + a2, be32(img + a1)); a1 += 4; a2 += 4;
            wr32(img + a2, be32(img + a1)); a1 += 4; a2 += 4;
            if (c == 0)
                break;
            c--;
        }
        a0 += 0xa0;
        if (d3 == 0)
            break;
        d3--;
    }
}

void box_hline_copy(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d3);
void box_vline_copy(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d3);

/* FUN_00013370 @ 0x13370: revela una caja (D0,D1,D2,D3) con cuatro corridas
 * (0x130a4/0x13162) que van cerrando un rombo; por iteracion actualiza los ocho
 * bordes 0x1a516/518/51a/51c/51e/520/522/524 y espera 0x3e8 ciclos (sin efecto). */
void func_13370(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3) {
    d0 = (d0 << 2) & 0xffff;
    d2 = (d2 << 2) & 0xffff;
    wr16(img + 0x1a516, d0 & 0xffff);
    wr16(img + 0x1a51e, d1 & 0xffff);
    uint32_t d4 = d0;
    uint32_t x1 = (d0 + d2 - 1) & 0xffff;
    wr16(img + 0x1a518, x1);
    wr16(img + 0x1a520, d1 & 0xffff);
    uint32_t y1 = (d1 + d3 - 1) & 0xffff;
    wr16(img + 0x1a51a, x1);
    wr16(img + 0x1a522, y1);
    wr16(img + 0x1a51c, d4);
    wr16(img + 0x1a524, y1);

    uint32_t iters = (((d3 & 0xffff) >> 1) + 1) & 0xffff;
    d2 = 0;
    d3 = 1;
    for (;;) {
        uint32_t c0 = be16(img + 0x1a516);
        uint32_t c1 = (be16(img + 0x1a51e) - d2) & 0xffff;
        box_vline_copy(img, c0, c1, d3);
        c1 = (c1 + d2) & 0xffff;
        c0 = (c0 - d2) & 0xffff;
        box_hline_copy(img, c0, c1, d3);
        c0 = (c0 + d2 + 1) & 0xffff;
        c1 = (c1 + 1) & 0xffff;
        wr16(img + 0x1a516, c0);
        wr16(img + 0x1a51e, c1);

        c0 = be16(img + 0x1a518);
        c1 = be16(img + 0x1a520);
        box_hline_copy(img, c0, c1, d3);
        c1 = (c1 - d2) & 0xffff;
        box_vline_copy(img, c0, c1, d3);
        c1 = (c1 + d2 + 1) & 0xffff;
        c0 = (c0 - 1) & 0xffff;
        wr16(img + 0x1a518, c0);
        wr16(img + 0x1a520, c1);

        c0 = be16(img + 0x1a51a);
        c1 = be16(img + 0x1a522);
        box_vline_copy(img, c0, c1, d3);
        box_hline_copy(img, c0, c1, d3);
        c0 = (c0 - 1) & 0xffff;
        c1 = (c1 - 1) & 0xffff;
        wr16(img + 0x1a51a, c0);
        wr16(img + 0x1a522, c1);

        c0 = be16(img + 0x1a51c);
        c1 = be16(img + 0x1a524);
        box_vline_copy(img, c0, c1, d3);
        c0 = (c0 - d2) & 0xffff;
        box_hline_copy(img, c0, c1, d3);
        c0 = (c0 + d2 + 1) & 0xffff;
        c1 = (c1 - 1) & 0xffff;
        wr16(img + 0x1a51c, c0);
        wr16(img + 0x1a524, c1);

        d3 = (d3 + 1) & 0xffff;
        d2 = (d2 + 1) & 0xffff;
        if (iters == 0)
            break;
        iters--;
    }
}

/* FUN_00013474 @ 0x13474: si 0x1a570 != 0 cae en 0x1222e; si no, revela la caja del
 * icono A6[1] (0x120e4 + 0x13370) y A6 += 2. */
uint32_t func_13474(uint8_t *img, uint32_t a6) {
    if (img[A_SKIP_TRANSITION] != 0) {
        uint32_t a6o;
        hint_blit_entry(img, a6, &a6o);
        return a6o;
    }
    uint32_t o[8];
    hint_select_off(img, img[a6 + 1], o);
    func_13370(img, o[3], o[4], o[5], o[6]);
    return a6 + 2;
}

/* FUN_00014748 @ 0x14748: segun el modo 0x1acc6 lanza 0x12d96 (0x09/0x66) o
 * 0x12dd8 (0x5f/0x67). */
void func_14748(uint8_t *img) {
    if (img[0x1acc6] == 0x09) func_12d96(img);
    if (img[0x1acc6] == 0x66) func_12d96(img);
    if (img[0x1acc6] == 0x5f) func_12dd8(img);
    if (img[0x1acc6] == 0x67) func_12dd8(img);
}

void wait_frame(uint8_t *img);

/* Cuerpo comun de 0x12240/0x1224e: espera D5 frames comprobando el boton. D5 = byte
 * A6[1] (0x12240) o 4 (0x1224e) mas *(0x1a84e). Si el bit 15 esta puesto o el resultado
 * es 0, sale; si no, D5+1 repeticiones de 0x33 wait_frame. */
static void wait_count(uint8_t *img, uint32_t d5) {
    d5 = (d5 + be16(img + 0x1a84e)) & 0xffff;
    if (d5 & 0x8000)
        return;
    if (d5 == 0)
        return;
    d5 = (d5 - 1) & 0xffff;
    for (;;) {
        for (uint32_t c = 0x32;;) {
            wait_frame(img);
            if (img[A_INPUT_BUTTONS] & 0x04)
                return;
            c = (c - 1) & 0xffff;
            if (c == 0xffff)
                break;
        }
        d5 = (d5 - 1) & 0xffff;
        if (d5 == 0xffff)
            break;
    }
}

uint32_t func_12240(uint8_t *img, uint32_t a6) {
    wait_count(img, img[a6 + 1]);
    return a6 + 2;                 /* adda.l #1,a6 de entrada + addq.l #1,a6 de salida */
}

uint32_t func_1224e(uint8_t *img, uint32_t a6) {
    wait_count(img, 4);
    return a6 + 1;                 /* addq.l #1,a6 de salida */
}

/* ---- handlers de comando 0x11xxx (encadenan la salida de 0x128a4) ----
 *
 * 0x128a4 deja en el fichero de registros lo que sus llamantes encadenan:
 *   d2 = ancho  (0x1a52e), d3 = alto (0x1a52c), a1 = bitmap (0x1a528),
 *   a0 = screen_address(x,y) y d7 = fase, ambos recalculados por 0x10fec.
 * Como el C no comparte registros, el dispatcher los reconstruye de los globals
 * (y de `screen_address`) en el mismo punto en que el original los tendria.
 */

uint32_t func_128a4(uint8_t *img, uint32_t a6);
void func_126e0(uint8_t *img, uint32_t d2, uint32_t d3, uint32_t d7, uint32_t a1);
void func_11ece(uint8_t *img, uint32_t d1, uint32_t a0, uint32_t a1);
void func_11ee8(uint8_t *img, uint32_t d1, uint32_t a0, uint32_t a1);

typedef struct {
    uint32_t a0, a1, d2, d3, d7;
} elem_regs;

static elem_regs elem_after_128a4(uint8_t *img) {
    elem_regs r;
    r.a0 = screen_address(img, be16(img + A_ELEM_1A532),
                              be16(img + A_ELEM_1A530), &r.d7);
    r.a1 = be32(img + A_ELEM_1A528);
    r.d2 = be16(img + A_ELEM_1A52E);
    r.d3 = be16(img + A_ELEM_1A52C);
    return r;
}

/* FUN_00011f02 @ 0x11f02: con flag 0x1a570 se corta a 0x134da (128a4+124da); sin
 * el, 128a4 + 126e0 (compone el bitmap en 0xd8f0) y 11ece (d1=alto-1, a1=0xd8f0 +
 * (alto-1)*0x40). */
uint32_t func_11f02(uint8_t *img, uint32_t a6) {
    a6 = func_128a4(img, a6);
    elem_regs r = elem_after_128a4(img);
    if (img[A_SKIP_TRANSITION] != 0) {
        blit_rect_vbl(img, r.a0, r.a1, r.d2, r.d3, r.d7);
        return a6;
    }
    func_126e0(img, r.d2, r.d3, r.d7, r.a1);
    uint32_t d4 = ((r.d3 & 0xffff) - 1u) << 6;         /* subq.l #1 + lsl.l #6 */
    func_11ece(img, (r.d3 - 1) & 0xffff, r.a0, A_SAVE_DESC + d4);
    return a6;
}

/* FUN_00011f2e @ 0x11f2e: idem pero 11ee8; el segundo 0x10fec usa y = *(0x1a530)+alto-1
 * y a1 = 0xd8f0. */
uint32_t func_11f2e(uint8_t *img, uint32_t a6) {
    a6 = func_128a4(img, a6);
    elem_regs r = elem_after_128a4(img);
    if (img[A_SKIP_TRANSITION] != 0) {
        blit_rect_vbl(img, r.a0, r.a1, r.d2, r.d3, r.d7);
        return a6;
    }
    func_126e0(img, r.d2, r.d3, r.d7, r.a1);
    uint32_t x = be16(img + A_ELEM_1A532);
    uint32_t y = (be16(img + A_ELEM_1A530) + r.d3 - 1) & 0xffff;
    uint32_t d7;
    uint32_t a0 = screen_address(img, x, y, &d7);
    func_11ee8(img, (r.d3 - 1) & 0xffff, a0, A_SAVE_DESC);
    return a6;
}

void wait_vbl(uint8_t *img);
uint32_t func_134da(uint8_t *img, uint32_t a6);
void func_11dee(uint8_t *img, uint32_t d1, uint32_t d2, uint32_t d3, uint32_t d7,
                 uint32_t a0);
void func_11e5e(uint8_t *img, uint32_t d1, uint32_t d2, uint32_t d3, uint32_t d7,
                 uint32_t a0);
void overlay_compose_planes32_wrap(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3,
                 uint32_t d6, uint32_t a0, uint32_t a1);
void overlay_compose_planes_wrap(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3,
                 uint32_t a0, uint32_t a1);
void overlay_compose_masked_wrap(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3,
                 uint32_t a0);
void overlay_compose_shift_wrap(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3,
                 uint32_t a0);

/* FUN_00011dee @ 0x11dee / FUN_00011e5e @ 0x11e5e: desplazan el elemento D1+1
 * pasadas copiando D3 filas con 0x123c0 (hacia arriba/abajo) y rebliteando la fila
 * del buffer trasero; 0x1a53e (long) controla el descenso por pasada. */
void func_11dee(uint8_t *img, uint32_t d1in, uint32_t d2in, uint32_t d3in,
                 uint32_t d7in, uint32_t a0in) {
    uint32_t d1 = d1in & 0xffff, a0 = a0in;
    wr32(img + 0x1a53e, 1);
    blit_mask_setup(img, d2in, d7in);
    int32_t a3 = (int32_t)(int16_t)(d3in & 0xffff);
    for (uint32_t k = 0; k <= d1; k++) {
        uint32_t base = a0;
        uint32_t inner = (uint32_t)a3 & 0xffff;
        if (inner == 0)
            inner = 0x10000;
        for (uint32_t i = 0; i < inner; i++) {
            blit_mask_row(img, a0, a0 - 0xa0);
            a0 -= 0xa0;
        }
        a0 += 0xa0;
        blit_mask_row(img, a0, a0 - 0x8000);
        wait_vbl(img);
        a0 = base;
        a3 -= 1;
    }
}

void func_11e5e(uint8_t *img, uint32_t d1in, uint32_t d2in, uint32_t d3in,
                 uint32_t d7in, uint32_t a0in) {
    uint32_t d1 = d1in & 0xffff, a0 = a0in;
    wr32(img + 0x1a53e, 1);
    blit_mask_setup(img, d2in, d7in);
    int32_t a3 = (int32_t)(int16_t)(d3in & 0xffff);
    for (uint32_t k = 0; k <= d1; k++) {
        uint32_t base = a0;
        uint32_t inner = (uint32_t)a3 & 0xffff;
        if (inner == 0)
            inner = 0x10000;
        for (uint32_t i = 0; i < inner; i++) {
            uint32_t a1 = a0;
            a0 -= 0xa0;
            blit_mask_row(img, a0, a1);
            a0 += 0x140;
        }
        a0 -= 0xa0;
        blit_mask_row(img, a0, a0 - 0x8000);
        wait_vbl(img);
        a0 = base;
        a3 -= 1;
    }
}

/* FUN_00011f64 @ 0x11f64: 128a4 + 124da (a1 = bitmap + stride - 2, d2 = 1) + 111ac
 * (d0 = fase, d1 = ancho - 1) sobre a1 - 2. */
uint32_t func_11f64(uint8_t *img, uint32_t a6) {
    if (img[A_SKIP_TRANSITION] != 0)
        return func_134da(img, a6);
    a6 = func_128a4(img, a6);
    elem_regs r = elem_after_128a4(img);
    uint32_t a1 = r.a1 + be32(img + A_DAT_1A534) - 2;
    blit_rect_vbl(img, r.a0, a1, 1, r.d3, r.d7);
    a1 -= 2;
    overlay_compose_planes_wrap(img, r.d7, be16(img + A_ELEM_1A52E) - 1, 0, r.d3, r.a0, a1);
    return a6;
}

/* FUN_00011f98 @ 0x11f98: como 0x11f64 pero el segundo blit es 114b0 y usa el borde
 * derecho del elemento (x + ancho - 1) recalculado con 0x10fec. */
uint32_t func_11f98(uint8_t *img, uint32_t a6) {
    if (img[A_SKIP_TRANSITION] != 0)
        return func_134da(img, a6);
    a6 = func_128a4(img, a6);
    elem_regs r = elem_after_128a4(img);
    uint32_t d7;
    uint32_t a0 = screen_address(img, be16(img + A_ELEM_1A532) + r.d2 - 1,
                                     be16(img + A_ELEM_1A530), &d7);
    blit_rect_vbl(img, a0, r.a1, 1, r.d3, d7);
    uint32_t d0 = (d7 - 1) & 0xffff;
    if (d7 == 0) {
        d0 = 0xf;
        a0 -= 8;
    }
    overlay_compose_planes32_wrap(img, d0, be16(img + A_ELEM_1A52E) - 1, 0, r.d3, 0, a0, r.a1 + 2);
    return a6;
}

/* FUN_00011fd8 @ 0x11fd8: icono A6[1] (0x120e4) + 11e5e + 123c0 sobre a0 - 0xa0. */
uint32_t func_11fd8(uint8_t *img, uint32_t a6) {
    if (img[A_SKIP_TRANSITION] != 0) {
        uint32_t a6o;
        hint_blit_entry(img, a6, &a6o);
        return a6o;
    }
    uint32_t o[8];
    hint_select_off(img, img[a6 + 1], o);
    func_11e5e(img, (o[6] - 1) & 0xffff, o[5], o[6], o[2], o[0]);
    uint32_t a0 = o[0] - 0xa0;
    blit_mask_row(img, a0, a0 - 0x8000);
    return a6 + 2;
}

/* FUN_0001200a @ 0x1200a: icono + 11dee sobre el borde derecho (d1 = icono[10] +
 * alto - 1) recalculado con 0x10fec. */
uint32_t func_1200a(uint8_t *img, uint32_t a6) {
    if (img[A_SKIP_TRANSITION] != 0) {
        uint32_t a6o;
        hint_blit_entry(img, a6, &a6o);
        return a6o;
    }
    uint32_t o[8];
    hint_select_off(img, img[a6 + 1], o);
    uint32_t d1 = (o[4] + o[6] - 1) & 0xffff;
    uint32_t d7;
    uint32_t a0 = screen_address(img, o[3], d1, &d7);
    func_11dee(img, (o[6] - 1) & 0xffff, o[5], o[6], o[2], a0);
    a0 += 0xa0;
    blit_mask_row(img, a0, a0 - 0x8000);
    return a6 + 2;
}

/* FUN_00012044 @ 0x12044: icono + 117de (d0 = cookie con d7 en la palabra baja,
 * d1 = d2 = icono[10], d3 = alto) + 1215e. */
uint32_t func_12044(uint8_t *img, uint32_t a6) {
    if (img[A_SKIP_TRANSITION] != 0) {
        uint32_t a6o;
        hint_blit_entry(img, a6, &a6o);
        return a6o;
    }
    uint32_t o[8];
    hint_select_off(img, img[a6 + 1], o);
    overlay_compose_masked_wrap(img, (o[3] & 0xffff0000u) | (o[2] & 0xffff), o[5], o[5], o[6], o[0]);
    mask_restore_rows(img, o[0], o[5], o[6], o[2]);
    return a6 + 2;
}

/* FUN_00012076 @ 0x12076: como 0x12044 pero con 11ada. */
uint32_t func_12076(uint8_t *img, uint32_t a6) {
    if (img[A_SKIP_TRANSITION] != 0) {
        uint32_t a6o;
        hint_blit_entry(img, a6, &a6o);
        return a6o;
    }
    uint32_t o[8];
    hint_select_off(img, img[a6 + 1], o);
    overlay_compose_shift_wrap(img, (o[3] & 0xffff0000u) | (o[2] & 0xffff), o[5], o[5], o[6], o[0]);
    mask_restore_rows(img, o[0], o[5], o[6], o[2]);
    return a6 + 2;
}

void func_127e0(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t d2, uint32_t d3,
                 uint32_t d7);
void func_13370(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3);
void reveal_lines_h(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3);
void reveal_lines_v(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3);

/* Tras 0x127e0 el A0 real queda en a0-0x8000 (127e0 resta 0x8000 y 127fa/124da
 * preservan A0); los handlers que continuan con 0x12466 usan ese valor. */
static uint32_t border_after_127e0(uint8_t *img, elem_regs r) {
    func_127e0(img, r.a0, r.a1, r.d2, r.d3, r.d7);
    return r.a0 - 0x8000;
}

/* 0x132aa: 128a4 + 127e0 + 13370 + 12466 (la rama 0x1a570!=0 es 0x134da). */
uint32_t func_132aa(uint8_t *img, uint32_t a6) {
    a6 = func_128a4(img, a6);
    elem_regs r = elem_after_128a4(img);
    if (img[A_SKIP_TRANSITION] != 0) {
        blit_rect_vbl(img, r.a0, r.a1, r.d2, r.d3, r.d7);
        return a6;
    }
    uint32_t b0 = border_after_127e0(img, r);
    func_13370(img, be16(img + A_ELEM_1A532), be16(img + A_ELEM_1A530),
                be16(img + A_ELEM_1A52E), be16(img + A_ELEM_1A52C));
    blit_mask_rows(img, b0, A_SAVE_DESC, be16(img + A_ELEM_1A52C));
    return a6;
}

/* 0x132ec: como 0x132aa pero con 0x13e14. */
uint32_t func_132ec(uint8_t *img, uint32_t a6) {
    a6 = func_128a4(img, a6);
    elem_regs r = elem_after_128a4(img);
    if (img[A_SKIP_TRANSITION] != 0) {
        blit_rect_vbl(img, r.a0, r.a1, r.d2, r.d3, r.d7);
        return a6;
    }
    uint32_t b0 = border_after_127e0(img, r);
    reveal_lines_h(img, be16(img + A_ELEM_1A532), be16(img + A_ELEM_1A530),
                be16(img + A_ELEM_1A52E), be16(img + A_ELEM_1A52C));
    blit_mask_rows(img, b0, A_SAVE_DESC, be16(img + A_ELEM_1A52C));
    return a6;
}

/* 0x1332e: como 0x132aa pero con 0x13eae. */
uint32_t func_1332e(uint8_t *img, uint32_t a6) {
    a6 = func_128a4(img, a6);
    elem_regs r = elem_after_128a4(img);
    if (img[A_SKIP_TRANSITION] != 0) {
        blit_rect_vbl(img, r.a0, r.a1, r.d2, r.d3, r.d7);
        return a6;
    }
    uint32_t b0 = border_after_127e0(img, r);
    reveal_lines_v(img, be16(img + A_ELEM_1A532), be16(img + A_ELEM_1A530),
                be16(img + A_ELEM_1A52E), be16(img + A_ELEM_1A52C));
    blit_mask_rows(img, b0, A_SAVE_DESC, be16(img + A_ELEM_1A52C));
    return a6;
}

/* 0x13490: revela la caja del icono A6[1] con 0x13e14. */
uint32_t func_13490(uint8_t *img, uint32_t a6) {
    if (img[A_SKIP_TRANSITION] != 0) {
        uint32_t a6o;
        hint_blit_entry(img, a6, &a6o);
        return a6o;
    }
    uint32_t o[8];
    hint_select_off(img, img[a6 + 1], o);
    wr16(img + A_ELEM_1A52C, o[6] & 0xffff);
    reveal_lines_h(img, o[3], o[4], o[5], o[6]);
    return a6 + 2;
}

/* 0x134b2: revela la caja del icono A6[1] con 0x13eae. */
uint32_t func_134b2(uint8_t *img, uint32_t a6) {
    if (img[A_SKIP_TRANSITION] != 0) {
        uint32_t a6o;
        hint_blit_entry(img, a6, &a6o);
        return a6o;
    }
    uint32_t o[8];
    hint_select_off(img, img[a6 + 1], o);
    wr16(img + A_ELEM_1A52C, o[6] & 0xffff);
    wr16(img + A_ELEM_1A52E, o[5] & 0xffff);
    reveal_lines_v(img, o[3], o[4], o[5], o[6]);
    return a6 + 2;
}

/* ---- bucle principal: piezas inmediatas ---- */

void run_command_keep_sp(uint8_t *img, uint32_t a6);
void cursor_restore(uint8_t *img);
void cursor_draw(uint8_t *img);
void overlay_a_frame(uint8_t *img);
void overlay_b_frame(uint8_t *img);
void wait_vbl(uint8_t *img);

/* FUN_000103a8 @ 0x103a8: guarda A7 en 0x1a826 y cae en run_command_keep_sp (0x103ae). */
void run_command(uint8_t *img, uint32_t a7) {
    wr32(img + A_RUN_STACK, a7);
    run_command_keep_sp(img, 0);
}

/* FUN_0001031c @ 0x1031c (handle_frame_input): espera un frame; mientras el flag
 * 0x1ab4f este puesto, el cursor este bajo (>=0x96) y el contador de VBL sea impar,
 * repite. Luego restaura el cursor (0x1060a), repinta los marcos de overlay A/B
 * (0x10a54/0x10acc) si cambiaron sus selectores, y dibuja el cursor (0x10526). */
void handle_frame_input(uint8_t *img) {
    for (;;) {
        wait_vbl(img);
        if (img[A_FRAME_1AB4F] == 0)
            break;
        if (be16(img + A_CURSOR_Y) < 0x96)
            break;
        if (!(img[A_DAT_1ACD4] & 1))
            break;
    }
    cursor_restore(img);
    if (img[A_TEXT_SEL_A] != img[A_1AB44]) {
        overlay_a_frame(img);
        img[A_1AB44] = img[A_TEXT_SEL_A];
    }
    if (img[A_TEXT_SEL_B] != img[A_1AB47]) {
        overlay_b_frame(img);
        img[A_1AB47] = img[A_TEXT_SEL_B];
    }
    cursor_draw(img);
}

/* FUN_00014798 @ 0x14798: devuelve el C si (d0b,d1b) cae en el rect [a4..a4+3]
 * (bytes). FUN_0001478a @ 0x1478a carga (cursor_x>>2, cursor_y) y cae en el. */
int func_14798(uint8_t *img, uint32_t a4, uint32_t d0, uint32_t d1) {
    uint8_t x = (uint8_t)d0, y = (uint8_t)d1;
    if (x < img[a4] || x >= img[a4 + 1])
        return 0;
    if (y < img[a4 + 2] || y >= img[a4 + 3])
        return 0;
    return 1;
}
int func_1478a(uint8_t *img, uint32_t a4) {
    return func_14798(img, a4, (be16(img + A_CURSOR_X) >> 2) & 0xff,
                       be16(img + A_CURSOR_Y) & 0xff);
}

/* FUN_000147ce @ 0x147ce: busca el rect del cursor en la tabla 0x19b06 (7 entradas
 * de 4 bytes). Si lo encuentra fija the_command = indice|0xa000 y 0x1ab46 = indice+2
 * (0x65 si 3); si no, the_command = 0, 0x1ab46 = 0x64 y 0x1a48a = 1. */
static uint32_t action_select(uint8_t *img, uint32_t a4, uint32_t d2) {
    uint32_t d0 = (be16(img + A_CURSOR_X) >> 2) & 0xff;
    uint32_t d1 = be16(img + A_CURSOR_Y) & 0xff;
    int found = func_14798(img, a4, d0, d1);
    if (!found) {
        uint32_t d3 = 6;
        for (;;) {
            a4 += 4;
            d2 = (d2 + 1) & 0xffff;
            found = func_14798(img, a4, d0, d1);
            if (found)
                break;                 /* dbcs: condicion verdadera -> cae */
            d3 = (d3 - 1) & 0xffff;    /* condicion falsa -> decrementa y salta */
            if (d3 == 0xffff)
                break;
        }
    }
    if (!found) {
        img[A_TEXT_SEL_B] = 0x64;
        img[A_1AB48A] = 1;
        wr16(img + A_THE_COMMAND, 0);
        return 0;
    }
    d2 = (d2 | 0xa000) & 0xffff;
    wr16(img + A_THE_COMMAND, d2);
    uint32_t d2b = ((d2 & 0xff) + 2) & 0xff;
    if (d2b == 3)
        d2b = 0x65;
    img[A_TEXT_SEL_B] = (uint8_t)d2b;
    img[A_1AB48A] = 0;
    return 0;
}

uint32_t func_147ce(uint8_t *img, uint32_t a4) {
    return action_select(img, a4, 1);
}

/* FUN_000147c2 @ 0x147c2: variante de 0x147ce sobre la tabla 0x19b26 con el
 * contador inicial D2 = 0xa. */
uint32_t func_147c2(uint8_t *img) {
    return action_select(img, 0x19b26, 0xa);
}

/* FUN_00014926 @ 0x14926: busca en la tabla de clases 0x1aa72 (5 B/entrada, 0x29
 * entradas) la primera cuyo byte 1 & 0xf == 0x1acc9; si la encuentra deja
 * 0x1a772 = entrada y 0x1ace6 = indice (1-based); si no, 0x1ace6 = 0. */
void func_14926(uint8_t *img) {
    uint32_t d2 = 1;
    uint32_t d4 = img[A_SEL_1ACC9];
    uint32_t a0 = A_OBJ_TBL;
    for (uint32_t n = 0x28; ; n--) {
        if (((uint32_t)img[a0 + 1] & 0xf) == d4) {
            wr32(img + A_ELEM_CLASS, a0);
            img[A_CLASS_IDX] = (uint8_t)d2;
            return;
        }
        a0 += 5;
        d2++;
        if (n == 0)
            break;
    }
    img[A_CLASS_IDX] = 0;
}

/* FUN_00014966 @ 0x14966: hit-test de las D3 entradas de 10 B de 0x1a6ae contra el
 * cursor. Encontrada: the_command = word[6], 0x1ab46 = byte[4], 0x1a48a = 0,
 * 0x1ace9 = byte[8] y 0x1a75e = 0x1a8f0 + (byte[8]-1)*6. Si no: the_command = 0,
 * 0x1ab46 = 0x64, 0x1a48a = 1. D3 < 2 falla sin probar la segunda entrada. */
void action_bar_hit_test(uint8_t *img, uint32_t d3) {
    uint32_t a4 = A_TBL_1A6AE;
    int found = func_1478a(img, a4);
    if (!found) {
        uint32_t count = d3 & 0xffff;
        if (count >= 2) {
            uint32_t n = count - 2;
            for (;;) {
                a4 += 10;
                found = func_14798(img, a4, (be16(img + A_CURSOR_X) >> 2) & 0xff,
                                    be16(img + A_CURSOR_Y) & 0xff);
                if (found || n == 0)
                    break;
                n--;
            }
        }
        if (!found) {
            img[A_TEXT_SEL_B] = 0x64;
            img[A_1AB48A] = 1;
            wr16(img + A_THE_COMMAND, 0);
            return;
        }
    }
    wr16(img + A_THE_COMMAND, be16(img + a4 + 6));
    img[A_TEXT_SEL_B] = img[a4 + 4];
    img[A_1AB48A] = 0;
    uint32_t d0 = img[a4 + 8];
    img[A_DAT_1ACE9] = (uint8_t)d0;
    d0 = (d0 - 1) & 0xff;
    wr32(img + A_DAT_1A75E, (uint32_t)(A_LIST_1A8F0 + d0 * 6));
}

/* FUN_00015588 @ 0x15588 (cmd idx 5) y FUN_000155b8 @ 0x155b8 (idx 6): fijan
 * the_command a 0xc35c/0xc35d y entran al despacho. */
void run_command_keep_sp(uint8_t *img, uint32_t a6);
void func_15588(uint8_t *img) {
    wr16(img + A_THE_COMMAND, 0xc35c);
    run_command_keep_sp(img, 0);
}
void func_155b8(uint8_t *img) {
    wr16(img + A_THE_COMMAND, 0xc35d);
    run_command_keep_sp(img, 0);
}

/* FUN_000134da @ 0x134da: rama compartida por los dispatchers 0x11xxx con
 * 0x1a570 != 0: 0x128a4 y rebliteo (0x124da). */
uint32_t func_134da(uint8_t *img, uint32_t a6) {
    a6 = func_128a4(img, a6);
    elem_regs r = elem_after_128a4(img);
    blit_rect_vbl(img, r.a0, r.a1, r.d2, r.d3, r.d7);
    return a6;
}

/* ---- espera con hit-test (0x167aa/0x167be/0x1680c) ---- */

void wait_frame_cursor(uint8_t *img);
void wait_frame(uint8_t *img);
void hit_test_objects(uint8_t *img, uint32_t d2, uint32_t d3);
void overlay_a_draw(uint8_t *img, uint32_t a6);
void handle_frame_input(uint8_t *img);
void cursor_restore(uint8_t *img);
void set_cursor_clip(uint8_t *img, uint32_t a0, uint32_t d0);

/* FUN_000167be @ 0x167be: bucle de espera (hasta clic, bit2 de 0x1a456). Guarda
 * (D2,D3) en 0x1a74e/0x1a74f y hace 0x10522 (espera frame + dibuja cursor) UNA
 * VEZ; luego itera: 0x13698, hit-test de objetos (0x14886), 0x1031c (restaura +
 * repinta cursor). No toca A6; lo comparten 0x167aa, 0x1680c y varios comandos.
 * OJO: 0x10522 dibuja el cursor SIN restaurar; si se llamara dentro del bucle
 * dejaria una replica del cursor por VBL (rastro). */
void func_167be(uint8_t *img, uint32_t d2, uint32_t d3) {
    img[A_DAT_1A74E] = (uint8_t)d2;
    img[A_DAT_1A74F] = (uint8_t)d3;
    wait_frame_cursor(img);           /* 0x167ca: espera frame + dibuja cursor (una vez) */
    for (;;) {
        wait_frame(img);
        hit_test_objects(img, img[A_DAT_1A74E], img[A_DAT_1A74F]);
        if (img[A_TEXT_SEL_A] != img[A_1AB44])
            overlay_a_draw(img, 0);
        handle_frame_input(img);
        if (!(img[A_INPUT_BUTTONS] & 0x04))
            continue;
        cursor_restore(img);
        break;
    }
}

/* FUN_000167aa @ 0x167aa (cmd idx 77): A6[1] elige origen/cursor (0x14a98),
 * A6[2]/A6[3] son D2/D3, A6 += 4 y cae en el bucle de espera. */
uint32_t func_167aa(uint8_t *img, uint32_t a6) {
    set_cursor_clip(img, 0, img[a6 + 1]);
    uint32_t d2 = img[a6 + 2];
    uint32_t d3 = img[a6 + 3];
    a6 += 4;
    func_167be(img, d2, d3);
    return a6;
}

/* FUN_0001680c @ 0x1680c: espera con origen/cursor 6 y (0xb8,0x90); devuelve A0 =
 * puntero de clase 0x1a772 (lo consume 0x169b4). */
uint32_t func_1680c(uint8_t *img) {
    set_cursor_clip(img, 0, 6);
    func_167be(img, 0xb8, 0x90);
    return be32(img + A_ELEM_CLASS);
}

/* FUN_0001695a @ 0x1695a: formatea el nombre del objeto seleccionado en 0x19970.
 * Con clase (a0[0]) < 0x23 usa a0[1] tal cual y marca 0xf4 en 0x19973; si no,
 * a0[1]-0x14 y 0xd4. Fila = a0[2]-0x28 en 0x19972 y a0[1] ajustado en 0x19971. */
void func_1695a(uint8_t *img) {
    uint32_t a0 = be32(img + A_SELECTED_OBJECT);
    uint32_t d0;
    if (img[a0] < 0x23) {
        d0 = img[a0 + 1];
        img[0x19973] = 0xf4;
    } else {
        d0 = (img[a0] - 0x14) & 0xff;
        img[0x19973] = 0xd4;
    }
    img[0x19972] = (uint8_t)((img[a0 + 2] - 0x28) & 0xff);
    img[0x19971] = (uint8_t)d0;
}

/* ---- impresion del nombre del objeto seleccionado (0x1687e/0x168a0/0x168d4) ----
 *
 * Las tres saltan el primer byte, leen D4 de A6[0] y posicionan el texto con
 * 0x10fb4 (A5 = 0x3c534); despues imprimen con 0x10b6c sobre la estructura de
 * 0x1996c/0x19970 y esperan (0x1228a -> Cnecin en modo 0x87). La diferencia entre
 * ellas es el sonido (0x168a0), el filtro del nombre (0x168d4) o nada (0x1687e). */
uint32_t text_index_arg(uint8_t *img, uint32_t a5, uint32_t d4, uint32_t *a6);
void text_window(uint8_t *img, uint32_t a5, uint32_t a6);
void hint_blit_stroke(uint8_t *img, uint32_t *a6);
void func_18f7a(uint8_t *img, uint32_t d1, uint32_t d4);

/* FUN_0001687e @ 0x1687e: nombre del objeto sin sonido ni filtro. */
void func_1687e(uint8_t *img, uint32_t a6) {
    a6 += 1;
    uint32_t a5 = text_index_arg(img, 0x3c534, img[a6], &a6);
    text_window(img, a5, 0x1996c);
    a6 += 1;
}

/* FUN_000168a0 @ 0x168a0: nombre con sonido (0xe6) y espera de tecla. */
void func_168a0(uint8_t *img, uint32_t a6) {
    func_18f7a(img, 0xe6, 0x18);
    a6 += 1;
    uint32_t a5 = text_index_arg(img, 0x3c534, img[a6], &a6);
    func_1695a(img);
    text_window(img, a5, 0x19970);
    (void)func_1228a(img, 0x19970);
    uint32_t a6b = 0x19970;
    hint_blit_stroke(img, &a6b);
    a6 += 1;
}

/* FUN_000168d4 @ 0x168d4: como 0x168a0 pero sin sonido y con el bit 6 de 0x19973
 * (el color del nombre) limpio. */
void func_168d4(uint8_t *img, uint32_t a6) {
    a6 += 1;
    uint32_t a5 = text_index_arg(img, 0x3c534, img[a6], &a6);
    func_1695a(img);
    img[0x19973] &= (uint8_t)~0x40u;
    text_window(img, a5, 0x19970);
    (void)func_1228a(img, 0x19970);
    uint32_t a6b = 0x19970;
    hint_blit_stroke(img, &a6b);
    a6 += 1;
}
