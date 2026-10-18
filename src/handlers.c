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

void prompt_wait(uint8_t *);
void hint_select_off(uint8_t *, uint32_t, uint32_t *);
void mask_restore_rows(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t);
uint32_t screen_address(uint8_t *, uint32_t, uint32_t, uint32_t *);
void blit_mask_rows(uint8_t *, uint32_t, uint32_t, uint32_t);
void blit_mask_setup(uint8_t *, uint32_t, uint32_t);
void blit_mask_row(uint8_t *, uint32_t, uint32_t);
void blit_rect_vbl(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t,
                 uint32_t);
void elem_blit_pair(uint8_t *);
void elem_blit_pair_vbl(uint8_t *);
void hint_blit_entry(uint8_t *, uint32_t, uint32_t *);

/* 0x1228a — prompt wait entry */
uint32_t prompt_wait_entry(uint8_t *img, uint32_t script) {
    prompt_wait(img);
    return script + 1;
}

/* 0x121fa — icon blit handler */
uint32_t icon_blit_handler(uint8_t *img, uint32_t script) {
    uint32_t o[8];
    hint_select_off(img, img[script + 1], o);
    mask_restore_rows(img, o[0], o[5], o[6], o[2]);
    uint32_t phase;
    uint32_t addr = screen_address(img, o[3], o[4], &phase);
    mask_restore_rows(img, addr, 2, 0xf, phase);
    wr16(img + A_DAT_1A484, 0);
    return script + 2;
}

/* 0x12436 — mask rows handler */
void mask_rows_handler(uint8_t *img) {
    wr16(img + A_BLIT_MASK_FIRST, be16(img + A_DAT_1A49E));
    wr16(img + A_BLIT_MASK_LAST, be16(img + A_ANIM_1A4A0));
    wr16(img + A_BLIT_GROUPS, be16(img + A_SAVE_1A4A2));
    uint32_t rows = be16(img + A_SAVE_1A4A4);
    uint32_t src = be32(img + A_DAT_1A4B2);
    blit_mask_rows(img, src, A_SAVE_DESC, rows);
}

/* 0x127fa — masked save row */
void masked_save_row(uint8_t *img, uint32_t src, uint32_t cols, uint32_t rows_in, uint32_t phase) {
    wr32(img + A_DAT_1A4B2, src);
    wr16(img + A_SAVE_1A4A4, rows_in & 0xffff);

    for (int i = 0; i < 0x627; i++)
        wr32(img + A_SAVE_DESC + (uint32_t)i * 4, 0);

    blit_mask_setup(img, cols, phase);
    wr16(img + A_DAT_1A49E, be16(img + A_BLIT_MASK_FIRST));
    wr16(img + A_ANIM_1A4A0, be16(img + A_BLIT_MASK_LAST));
    wr16(img + A_SAVE_1A4A2, be16(img + A_BLIT_GROUPS));

    uint32_t rows = (rows_in - 1) & 0xffff;
    uint32_t dst = A_SAVE_DESC;
    uint32_t src2 = src;
    for (;;) {
        blit_mask_row(img, dst, src2);
        dst += 0x40;
        src2 += 0xa0;
        if (rows == 0)
            break;
        rows--;
    }
}

/* 0x127e0 — masked save */
void masked_save(uint8_t *img, uint32_t src, uint32_t src2, uint32_t cols, uint32_t rows,
                 uint32_t phase) {
    uint32_t base = src - 0x8000;
    masked_save_row(img, base, cols, rows, phase);
    blit_rect_vbl(img, base, src2, cols, rows, phase);
}

/* FUN_000134e4 @ 0x134e4: copia una rejilla desde A1 a pantalla en (D0,D1)-0x8000:
 * (D3) filas de (D2>>2) celdas de 8 bytes. Depende de 0x10fec (screen_address). */
void copy_grid(uint8_t *img, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                 uint32_t src) {
    w = (w >> 2) & 0xffff;
    h = (h - 1) & 0xffff;
    w = (w - 1) & 0xffff;
    uint32_t phase;
    uint32_t addr = screen_address(img, x, y, &phase) - 0x8000;
    for (;;) {
        uint32_t c = w;
        uint32_t dst = addr;
        for (;;) {
            wr32(img + dst, be32(img + src)); src += 4; dst += 4;
            wr32(img + dst, be32(img + src)); src += 4; dst += 4;
            if (c == 0)
                break;
            c--;
        }
        addr += 0xa0;
        if (h == 0)
            break;
        h--;
    }
}

void box_hline_copy(uint8_t *, uint32_t, uint32_t, uint32_t);
void box_vline_copy(uint8_t *, uint32_t, uint32_t, uint32_t);

/* FUN_00013370 @ 0x13370: revela una caja (D0,D1,D2,D3) con cuatro corridas
 * (0x130a4/0x13162) que van cerrando un rombo; por iteracion actualiza los ocho
 * bordes 0x1a516/518/51a/51c/51e/520/522/524 y espera 0x3e8 ciclos (sin efecto). */
void reveal_diamond(uint8_t *img, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    x = (x << 2) & 0xffff;
    w = (w << 2) & 0xffff;
    wr16(img + 0x1a516, x & 0xffff);
    wr16(img + 0x1a51e, y & 0xffff);
    uint32_t x0 = x;
    uint32_t x1 = (x + w - 1) & 0xffff;
    wr16(img + 0x1a518, x1);
    wr16(img + 0x1a520, y & 0xffff);
    uint32_t y1 = (y + h - 1) & 0xffff;
    wr16(img + 0x1a51a, x1);
    wr16(img + 0x1a522, y1);
    wr16(img + 0x1a51c, x0);
    wr16(img + 0x1a524, y1);

    uint32_t iters = (((h & 0xffff) >> 1) + 1) & 0xffff;
    w = 0;
    h = 1;
    for (;;) {
        uint32_t c0 = be16(img + 0x1a516);
        uint32_t c1 = (be16(img + 0x1a51e) - w) & 0xffff;
        box_vline_copy(img, c0, c1, h);
        c1 = (c1 + w) & 0xffff;
        c0 = (c0 - w) & 0xffff;
        box_hline_copy(img, c0, c1, h);
        c0 = (c0 + w + 1) & 0xffff;
        c1 = (c1 + 1) & 0xffff;
        wr16(img + 0x1a516, c0);
        wr16(img + 0x1a51e, c1);

        c0 = be16(img + 0x1a518);
        c1 = be16(img + 0x1a520);
        box_hline_copy(img, c0, c1, h);
        c1 = (c1 - w) & 0xffff;
        box_vline_copy(img, c0, c1, h);
        c1 = (c1 + w + 1) & 0xffff;
        c0 = (c0 - 1) & 0xffff;
        wr16(img + 0x1a518, c0);
        wr16(img + 0x1a520, c1);

        c0 = be16(img + 0x1a51a);
        c1 = be16(img + 0x1a522);
        box_vline_copy(img, c0, c1, h);
        box_hline_copy(img, c0, c1, h);
        c0 = (c0 - 1) & 0xffff;
        c1 = (c1 - 1) & 0xffff;
        wr16(img + 0x1a51a, c0);
        wr16(img + 0x1a522, c1);

        c0 = be16(img + 0x1a51c);
        c1 = be16(img + 0x1a524);
        box_vline_copy(img, c0, c1, h);
        c0 = (c0 - w) & 0xffff;
        box_hline_copy(img, c0, c1, h);
        c0 = (c0 + w + 1) & 0xffff;
        c1 = (c1 - 1) & 0xffff;
        wr16(img + 0x1a51c, c0);
        wr16(img + 0x1a524, c1);

        h = (h + 1) & 0xffff;
        w = (w + 1) & 0xffff;
        if (iters == 0)
            break;
        iters--;
    }
}

/* FUN_00013474 @ 0x13474: si 0x1a570 != 0 cae en 0x1222e; si no, revela la caja del
 * icono A6[1] (0x120e4 + 0x13370) y A6 += 2. */
uint32_t icon_reveal(uint8_t *img, uint32_t script) {
    if (img[A_SKIP_TRANSITION] != 0) {
        uint32_t script_out;
        hint_blit_entry(img, script, &script_out);
        return script_out;
    }
    uint32_t out[8];
    hint_select_off(img, img[script + 1], out);
    reveal_diamond(img, out[3], out[4], out[5], out[6]);
    return script + 2;
}

/* FUN_00014748 @ 0x14748: segun el modo 0x1acc6 lanza 0x12d96 (0x09/0x66) o
 * 0x12dd8 (0x5f/0x67). */
void elem_mode_blit(uint8_t *img) {
    if (img[0x1acc6] == 0x09) elem_blit_pair(img);
    if (img[0x1acc6] == 0x66) elem_blit_pair(img);
    if (img[0x1acc6] == 0x5f) elem_blit_pair_vbl(img);
    if (img[0x1acc6] == 0x67) elem_blit_pair_vbl(img);
}

void wait_frame(uint8_t *);

/* Cuerpo comun de 0x12240/0x1224e: espera D5 frames comprobando el boton. D5 = byte
 * A6[1] (0x12240) o 4 (0x1224e) mas *(0x1a84e). Si el bit 15 esta puesto o el resultado
 * es 0, sale; si no, D5+1 repeticiones de 0x33 wait_frame. */
static void wait_count(uint8_t *img, uint32_t count) {
    count = (count + be16(img + 0x1a84e)) & 0xffff;
    if (count & 0x8000)
        return;
    if (count == 0)
        return;
    count = (count - 1) & 0xffff;
    for (;;) {
        for (uint32_t c = 0x32;;) {
            wait_frame(img);
            if (img[A_INPUT_BUTTONS] & 0x04)
                return;
            c = (c - 1) & 0xffff;
            if (c == 0xffff)
                break;
        }
        count = (count - 1) & 0xffff;
        if (count == 0xffff)
            break;
    }
}

/* 0x12240 — wait button a */
uint32_t wait_button_a(uint8_t *img, uint32_t script) {
    wait_count(img, img[script + 1]);
    return script + 2;                 /* adda.l #1,script de entrada + addq.l #1,script de salida */
}

/* 0x1224e — wait button b */
uint32_t wait_button_b(uint8_t *img, uint32_t script) {
    wait_count(img, 4);
    return script + 1;                 /* addq.l #1,script de salida */
}

/* ---- handlers de comando 0x11xxx (encadenan la salida de 0x128a4) ----
 *
 * 0x128a4 deja en el fichero de registros lo que sus llamantes encadenan:
 *   d2 = ancho  (0x1a52e), d3 = alto (0x1a52c), a1 = bitmap (0x1a528),
 *   a0 = screen_address(x,y) y d7 = fase, ambos recalculados por 0x10fec.
 * Como el C no comparte registros, el dispatcher los reconstruye de los globals
 * (y de `screen_address`) en el mismo punto en que el original los tendria.
 */

uint32_t elem_command_wrap(uint8_t *, uint32_t);
void grid_fill(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t);
void row_blit_a(uint8_t *, uint32_t, uint32_t, uint32_t);
void row_blit_b(uint8_t *, uint32_t, uint32_t, uint32_t);

typedef struct {
    uint32_t dest, src, cols, rows, phase;
} elem_regs;

static elem_regs elem_after_128a4(uint8_t *img) {
    elem_regs r;
    r.dest = screen_address(img, be16(img + A_ELEM_1A532),
                              be16(img + A_ELEM_1A530), &r.phase);
    r.src = be32(img + A_ELEM_1A528);
    r.cols = be16(img + A_ELEM_1A52E);
    r.rows = be16(img + A_ELEM_1A52C);
    return r;
}

/* FUN_00011f02 @ 0x11f02: con flag 0x1a570 se corta a 0x134da (128a4+124da); sin
 * el, 128a4 + 126e0 (compone el bitmap en 0xd8f0) y 11ece (d1=alto-1, a1=0xd8f0 +
 * (alto-1)*0x40). */
uint32_t blit_dispatch_a(uint8_t *img, uint32_t script) {
    script = elem_command_wrap(img, script);
    elem_regs r = elem_after_128a4(img);
    if (img[A_SKIP_TRANSITION] != 0) {
        blit_rect_vbl(img, r.dest, r.src, r.cols, r.rows, r.phase);
        return script;
    }
    grid_fill(img, r.cols, r.rows, r.phase, r.src);
    uint32_t off = ((r.rows & 0xffff) - 1u) << 6;         /* subq.l #1 + lsl.l #6 */
    row_blit_a(img, (r.rows - 1) & 0xffff, r.dest, A_SAVE_DESC + off);
    return script;
}

/* FUN_00011f2e @ 0x11f2e: idem pero 11ee8; el segundo 0x10fec usa y = *(0x1a530)+alto-1
 * y a1 = 0xd8f0. */
uint32_t blit_dispatch_b(uint8_t *img, uint32_t script) {
    script = elem_command_wrap(img, script);
    elem_regs r = elem_after_128a4(img);
    if (img[A_SKIP_TRANSITION] != 0) {
        blit_rect_vbl(img, r.dest, r.src, r.cols, r.rows, r.phase);
        return script;
    }
    grid_fill(img, r.cols, r.rows, r.phase, r.src);
    uint32_t x = be16(img + A_ELEM_1A532);
    uint32_t y = (be16(img + A_ELEM_1A530) + r.rows - 1) & 0xffff;
    uint32_t phase;
    uint32_t addr = screen_address(img, x, y, &phase);
    row_blit_b(img, (r.rows - 1) & 0xffff, addr, A_SAVE_DESC);
    return script;
}

void wait_vbl(uint8_t *);
uint32_t elem_blit_vbl(uint8_t *, uint32_t);
void scroll_up(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t,
                 uint32_t);
void scroll_down(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t,
                 uint32_t);
void overlay_compose_planes32_wrap(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t,
                 uint32_t, uint32_t, uint32_t);
void overlay_compose_planes_wrap(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t,
                 uint32_t, uint32_t);
void overlay_compose_masked_wrap(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t,
                 uint32_t);
void overlay_compose_shift_wrap(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t,
                 uint32_t);

/* FUN_00011dee @ 0x11dee / FUN_00011e5e @ 0x11e5e: desplazan el elemento D1+1
 * pasadas copiando D3 filas con 0x123c0 (hacia arriba/abajo) y rebliteando la fila
 * del buffer trasero; 0x1a53e (long) controla el descenso por pasada. */
void scroll_up(uint8_t *img, uint32_t passes_in, uint32_t cols, uint32_t rows,
                 uint32_t phase, uint32_t base) {
    uint32_t passes = passes_in & 0xffff, p = base;
    wr32(img + 0x1a53e, 1);
    blit_mask_setup(img, cols, phase);
    int32_t n = (int32_t)(int16_t)(rows & 0xffff);
    for (uint32_t k = 0; k <= passes; k++) {
        uint32_t base = p;
        uint32_t inner = (uint32_t)n & 0xffff;
        if (inner == 0)
            inner = 0x10000;
        for (uint32_t i = 0; i < inner; i++) {
            blit_mask_row(img, p, p - 0xa0);
            p -= 0xa0;
        }
        p += 0xa0;
        blit_mask_row(img, p, p - 0x8000);
        wait_vbl(img);
        p = base;
        n -= 1;
    }
}

/* 0x11e5e — scroll down */
void scroll_down(uint8_t *img, uint32_t passes_in, uint32_t d2in, uint32_t d3in,
                 uint32_t d7in, uint32_t base) {
    uint32_t passes = passes_in & 0xffff, p = base;
    wr32(img + 0x1a53e, 1);
    blit_mask_setup(img, d2in, d7in);
    int32_t n = (int32_t)(int16_t)(d3in & 0xffff);
    for (uint32_t k = 0; k <= passes; k++) {
        uint32_t base = p;
        uint32_t inner = (uint32_t)n & 0xffff;
        if (inner == 0)
            inner = 0x10000;
        for (uint32_t i = 0; i < inner; i++) {
            uint32_t tmp = p;
            p -= 0xa0;
            blit_mask_row(img, p, tmp);
            p += 0x140;
        }
        p -= 0xa0;
        blit_mask_row(img, p, p - 0x8000);
        wait_vbl(img);
        p = base;
        n -= 1;
    }
}

/* FUN_00011f64 @ 0x11f64: 128a4 + 124da (a1 = bitmap + stride - 2, d2 = 1) + 111ac
 * (d0 = fase, d1 = ancho - 1) sobre a1 - 2. */
uint32_t overlay_dispatch_1(uint8_t *img, uint32_t script) {
    if (img[A_SKIP_TRANSITION] != 0)
        return elem_blit_vbl(img, script);
    script = elem_command_wrap(img, script);
    elem_regs r = elem_after_128a4(img);
    uint32_t bitmap = r.src + be32(img + A_DAT_1A534) - 2;
    blit_rect_vbl(img, r.dest, bitmap, 1, r.rows, r.phase);
    bitmap -= 2;
    overlay_compose_planes_wrap(img, r.phase, be16(img + A_ELEM_1A52E) - 1, 0, r.rows, r.dest, bitmap);
    return script;
}

/* FUN_00011f98 @ 0x11f98: como 0x11f64 pero el segundo blit es 114b0 y usa el borde
 * derecho del elemento (x + ancho - 1) recalculado con 0x10fec. */
uint32_t overlay_dispatch_2(uint8_t *img, uint32_t script) {
    if (img[A_SKIP_TRANSITION] != 0)
        return elem_blit_vbl(img, script);
    script = elem_command_wrap(img, script);
    elem_regs r = elem_after_128a4(img);
    uint32_t phase;
    uint32_t addr = screen_address(img, be16(img + A_ELEM_1A532) + r.cols - 1,
                                     be16(img + A_ELEM_1A530), &phase);
    blit_rect_vbl(img, addr, r.src, 1, r.rows, phase);
    uint32_t phase_adj = (phase - 1) & 0xffff;
    if (phase == 0) {
        phase_adj = 0xf;
        addr -= 8;
    }
    overlay_compose_planes32_wrap(img, phase_adj, be16(img + A_ELEM_1A52E) - 1, 0, r.rows, 0, addr, r.src + 2);
    return script;
}

/* FUN_00011fd8 @ 0x11fd8: icono A6[1] (0x120e4) + 11e5e + 123c0 sobre a0 - 0xa0. */
uint32_t overlay_dispatch_3(uint8_t *img, uint32_t script) {
    if (img[A_SKIP_TRANSITION] != 0) {
        uint32_t script_out;
        hint_blit_entry(img, script, &script_out);
        return script_out;
    }
    uint32_t out[8];
    hint_select_off(img, img[script + 1], out);
    scroll_down(img, (out[6] - 1) & 0xffff, out[5], out[6], out[2], out[0]);
    uint32_t addr = out[0] - 0xa0;
    blit_mask_row(img, addr, addr - 0x8000);
    return script + 2;
}

/* FUN_0001200a @ 0x1200a: icono + 11dee sobre el borde derecho (d1 = icono[10] +
 * alto - 1) recalculado con 0x10fec. */
uint32_t overlay_dispatch_4(uint8_t *img, uint32_t script) {
    if (img[A_SKIP_TRANSITION] != 0) {
        uint32_t script_out;
        hint_blit_entry(img, script, &script_out);
        return script_out;
    }
    uint32_t out[8];
    hint_select_off(img, img[script + 1], out);
    uint32_t y = (out[4] + out[6] - 1) & 0xffff;
    uint32_t phase;
    uint32_t addr = screen_address(img, out[3], y, &phase);
    scroll_up(img, (out[6] - 1) & 0xffff, out[5], out[6], out[2], addr);
    addr += 0xa0;
    blit_mask_row(img, addr, addr - 0x8000);
    return script + 2;
}

/* FUN_00012044 @ 0x12044: icono + 117de (d0 = cookie con d7 en la palabra baja,
 * d1 = d2 = icono[10], d3 = alto) + 1215e. */
uint32_t overlay_dispatch_5(uint8_t *img, uint32_t script) {
    if (img[A_SKIP_TRANSITION] != 0) {
        uint32_t script_out;
        hint_blit_entry(img, script, &script_out);
        return script_out;
    }
    uint32_t out[8];
    hint_select_off(img, img[script + 1], out);
    overlay_compose_masked_wrap(img, (out[3] & 0xffff0000u) | (out[2] & 0xffff), out[5], out[5], out[6], out[0]);
    mask_restore_rows(img, out[0], out[5], out[6], out[2]);
    return script + 2;
}

/* FUN_00012076 @ 0x12076: como 0x12044 pero con 11ada. */
uint32_t overlay_dispatch_6(uint8_t *img, uint32_t script) {
    if (img[A_SKIP_TRANSITION] != 0) {
        uint32_t script_out;
        hint_blit_entry(img, script, &script_out);
        return script_out;
    }
    uint32_t out[8];
    hint_select_off(img, img[script + 1], out);
    overlay_compose_shift_wrap(img, (out[3] & 0xffff0000u) | (out[2] & 0xffff), out[5], out[5], out[6], out[0]);
    mask_restore_rows(img, out[0], out[5], out[6], out[2]);
    return script + 2;
}

void masked_save(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t,
                 uint32_t);
void reveal_diamond(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t);
void reveal_lines_h(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t);
void reveal_lines_v(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t);

/* Tras 0x127e0 el A0 real queda en a0-0x8000 (127e0 resta 0x8000 y 127fa/124da
 * preservan A0); los handlers que continuan con 0x12466 usan ese valor. */
static uint32_t border_after_127e0(uint8_t *img, elem_regs r) {
    masked_save(img, r.dest, r.src, r.cols, r.rows, r.phase);
    return r.dest - 0x8000;
}

/* 0x132aa: 128a4 + 127e0 + 13370 + 12466 (la rama 0x1a570!=0 es 0x134da). */
uint32_t reveal_box_build(uint8_t *img, uint32_t script) {
    script = elem_command_wrap(img, script);
    elem_regs r = elem_after_128a4(img);
    if (img[A_SKIP_TRANSITION] != 0) {
        blit_rect_vbl(img, r.dest, r.src, r.cols, r.rows, r.phase);
        return script;
    }
    uint32_t b0 = border_after_127e0(img, r);
    reveal_diamond(img, be16(img + A_ELEM_1A532), be16(img + A_ELEM_1A530),
                be16(img + A_ELEM_1A52E), be16(img + A_ELEM_1A52C));
    blit_mask_rows(img, b0, A_SAVE_DESC, be16(img + A_ELEM_1A52C));
    return script;
}

/* 0x132ec: como 0x132aa pero con 0x13e14. */
uint32_t reveal_box_wait_a(uint8_t *img, uint32_t script) {
    script = elem_command_wrap(img, script);
    elem_regs r = elem_after_128a4(img);
    if (img[A_SKIP_TRANSITION] != 0) {
        blit_rect_vbl(img, r.dest, r.src, r.cols, r.rows, r.phase);
        return script;
    }
    uint32_t b0 = border_after_127e0(img, r);
    reveal_lines_h(img, be16(img + A_ELEM_1A532), be16(img + A_ELEM_1A530),
                be16(img + A_ELEM_1A52E), be16(img + A_ELEM_1A52C));
    blit_mask_rows(img, b0, A_SAVE_DESC, be16(img + A_ELEM_1A52C));
    return script;
}

/* 0x1332e: como 0x132aa pero con 0x13eae. */
uint32_t reveal_box_wait_b(uint8_t *img, uint32_t script) {
    script = elem_command_wrap(img, script);
    elem_regs r = elem_after_128a4(img);
    if (img[A_SKIP_TRANSITION] != 0) {
        blit_rect_vbl(img, r.dest, r.src, r.cols, r.rows, r.phase);
        return script;
    }
    uint32_t b0 = border_after_127e0(img, r);
    reveal_lines_v(img, be16(img + A_ELEM_1A532), be16(img + A_ELEM_1A530),
                be16(img + A_ELEM_1A52E), be16(img + A_ELEM_1A52C));
    blit_mask_rows(img, b0, A_SAVE_DESC, be16(img + A_ELEM_1A52C));
    return script;
}

/* 0x13490: revela la caja del icono A6[1] con 0x13e14. */
uint32_t icon_reveal_h(uint8_t *img, uint32_t script) {
    if (img[A_SKIP_TRANSITION] != 0) {
        uint32_t script_out;
        hint_blit_entry(img, script, &script_out);
        return script_out;
    }
    uint32_t out[8];
    hint_select_off(img, img[script + 1], out);
    wr16(img + A_ELEM_1A52C, out[6] & 0xffff);
    reveal_lines_h(img, out[3], out[4], out[5], out[6]);
    return script + 2;
}

/* 0x134b2: revela la caja del icono A6[1] con 0x13eae. */
uint32_t icon_reveal_v(uint8_t *img, uint32_t script) {
    if (img[A_SKIP_TRANSITION] != 0) {
        uint32_t script_out;
        hint_blit_entry(img, script, &script_out);
        return script_out;
    }
    uint32_t out[8];
    hint_select_off(img, img[script + 1], out);
    wr16(img + A_ELEM_1A52C, out[6] & 0xffff);
    wr16(img + A_ELEM_1A52E, out[5] & 0xffff);
    reveal_lines_v(img, out[3], out[4], out[5], out[6]);
    return script + 2;
}

/* ---- bucle principal: piezas inmediatas ---- */

void run_command_keep_sp(uint8_t *, uint32_t);
void cursor_restore(uint8_t *);
void cursor_draw(uint8_t *);
void overlay_a_frame(uint8_t *);
void overlay_b_frame(uint8_t *);
void wait_vbl(uint8_t *);

/* FUN_000103a8 @ 0x103a8: guarda A7 en 0x1a826 y cae en run_command_keep_sp (0x103ae). */
void run_command(uint8_t *img, uint32_t stack) {
    wr32(img + A_RUN_STACK, stack);
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
int rect_hit_test(uint8_t *img, uint32_t rect, uint32_t x_in, uint32_t y_in) {
    uint8_t x = (uint8_t)x_in, y = (uint8_t)y_in;
    if (x < img[rect] || x >= img[rect + 1])
        return 0;
    if (y < img[rect + 2] || y >= img[rect + 3])
        return 0;
    return 1;
}
/* 0x1478a — rect hit test a */
int rect_hit_test_a(uint8_t *img, uint32_t rect) {
    return rect_hit_test(img, rect, (be16(img + A_CURSOR_X) >> 2) & 0xff,
                       be16(img + A_CURSOR_Y) & 0xff);
}

/* FUN_000147ce @ 0x147ce: busca el rect del cursor en la tabla 0x19b06 (7 entradas
 * de 4 bytes). Si lo encuentra fija the_command = indice|0xa000 y 0x1ab46 = indice+2
 * (0x65 si 3); si no, the_command = 0, 0x1ab46 = 0x64 y 0x1a48a = 1. */
static uint32_t action_select(uint8_t *img, uint32_t rect, uint32_t idx) {
    uint32_t x = (be16(img + A_CURSOR_X) >> 2) & 0xff;
    uint32_t y = be16(img + A_CURSOR_Y) & 0xff;
    int found = rect_hit_test(img, rect, x, y);
    if (!found) {
        uint32_t n = 6;
        for (;;) {
            rect += 4;
            idx = (idx + 1) & 0xffff;
            found = rect_hit_test(img, rect, x, y);
            if (found)
                break;                 /* dbcs: condicion verdadera -> cae */
            n = (n - 1) & 0xffff;    /* condicion falsa -> decrementa y salta */
            if (n == 0xffff)
                break;
        }
    }
    if (!found) {
        img[A_TEXT_SEL_B] = 0x64;
        img[A_1AB48A] = 1;
        wr16(img + A_THE_COMMAND, 0);
        return 0;
    }
    idx = (idx | 0xa000) & 0xffff;
    wr16(img + A_THE_COMMAND, idx);
    uint32_t sel = ((idx & 0xff) + 2) & 0xff;
    if (sel == 3)
        sel = 0x65;
    img[A_TEXT_SEL_B] = (uint8_t)sel;
    img[A_1AB48A] = 0;
    return 0;
}

/* 0x147ce — cursor select b */
uint32_t cursor_select_b(uint8_t *img, uint32_t rect) {
    return action_select(img, rect, 1);
}

/* FUN_000147c2 @ 0x147c2: variante de 0x147ce sobre la tabla 0x19b26 con el
 * contador inicial D2 = 0xa. */
uint32_t cursor_select_a(uint8_t *img) {
    return action_select(img, 0x19b26, 0xa);
}

/* FUN_00014926 @ 0x14926: busca en la tabla de clases 0x1aa72 (5 B/entrada, 0x29
 * entradas) la primera cuyo byte 1 & 0xf == 0x1acc9; si la encuentra deja
 * 0x1a772 = entrada y 0x1ace6 = indice (1-based); si no, 0x1ace6 = 0. */
void class_find(uint8_t *img) {
    uint32_t idx = 1;
    uint32_t cls = img[A_SEL_1ACC9];
    uint32_t p = A_OBJ_TBL;
    for (uint32_t n = 0x28; ; n--) {
        if (((uint32_t)img[p + 1] & 0xf) == cls) {
            wr32(img + A_ELEM_CLASS, p);
            img[A_CLASS_IDX] = (uint8_t)idx;
            return;
        }
        p += 5;
        idx++;
        if (n == 0)
            break;
    }
    img[A_CLASS_IDX] = 0;
}

/* FUN_00014966 @ 0x14966: hit-test de las D3 entradas de 10 B de 0x1a6ae contra el
 * cursor. Encontrada: the_command = word[6], 0x1ab46 = byte[4], 0x1a48a = 0,
 * 0x1ace9 = byte[8] y 0x1a75e = 0x1a8f0 + (byte[8]-1)*6. Si no: the_command = 0,
 * 0x1ab46 = 0x64, 0x1a48a = 1. D3 < 2 falla sin probar la segunda entrada. */
void action_bar_hit_test(uint8_t *img, uint32_t count_in) {
    uint32_t entry = A_TBL_1A6AE;
    int found = rect_hit_test_a(img, entry);
    if (!found) {
        uint32_t count = count_in & 0xffff;
        if (count >= 2) {
            uint32_t n = count - 2;
            for (;;) {
                entry += 10;
                found = rect_hit_test(img, entry, (be16(img + A_CURSOR_X) >> 2) & 0xff,
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
    wr16(img + A_THE_COMMAND, be16(img + entry + 6));
    img[A_TEXT_SEL_B] = img[entry + 4];
    img[A_1AB48A] = 0;
    uint32_t idx = img[entry + 8];
    img[A_DAT_1ACE9] = (uint8_t)idx;
    idx = (idx - 1) & 0xff;
    wr32(img + A_DAT_1A75E, (uint32_t)(A_LIST_1A8F0 + idx * 6));
}

/* FUN_00015588 @ 0x15588 (cmd idx 5) y FUN_000155b8 @ 0x155b8 (idx 6): fijan
 * the_command a 0xc35c/0xc35d y entran al despacho. */
void run_command_keep_sp(uint8_t *, uint32_t);
/* 0x15588 — mode flag wrap */
void mode_flag_wrap(uint8_t *img) {
    wr16(img + A_THE_COMMAND, 0xc35c);
    run_command_keep_sp(img, 0);
}
/* 0x155b8 — mode queue wrap */
void mode_queue_wrap(uint8_t *img) {
    wr16(img + A_THE_COMMAND, 0xc35d);
    run_command_keep_sp(img, 0);
}

/* FUN_000134da @ 0x134da: rama compartida por los dispatchers 0x11xxx con
 * 0x1a570 != 0: 0x128a4 y rebliteo (0x124da). */
uint32_t elem_blit_vbl(uint8_t *img, uint32_t script) {
    script = elem_command_wrap(img, script);
    elem_regs r = elem_after_128a4(img);
    blit_rect_vbl(img, r.dest, r.src, r.cols, r.rows, r.phase);
    return script;
}

/* ---- espera con hit-test (0x167aa/0x167be/0x1680c) ---- */

void wait_frame_cursor(uint8_t *);
void wait_frame(uint8_t *);
void hit_test_objects(uint8_t *, uint32_t, uint32_t);
void overlay_a_draw(uint8_t *, uint32_t);
void handle_frame_input(uint8_t *);
void cursor_restore(uint8_t *);
void set_cursor_clip(uint8_t *, uint32_t, uint32_t);

/* FUN_000167be @ 0x167be: bucle de espera (hasta clic, bit2 de 0x1a456). Guarda
 * (D2,D3) en 0x1a74e/0x1a74f y hace 0x10522 (espera frame + dibuja cursor) UNA
 * VEZ; luego itera: 0x13698, hit-test de objetos (0x14886), 0x1031c (restaura +
 * repinta cursor). No toca A6; lo comparten 0x167aa, 0x1680c y varios comandos.
 * OJO: 0x10522 dibuja el cursor SIN restaurar; si se llamara dentro del bucle
 * dejaria una replica del cursor por VBL (rastro). */
void obj_wait_hit(uint8_t *img, uint32_t x, uint32_t y) {
    img[A_DAT_1A74E] = (uint8_t)x;
    img[A_DAT_1A74F] = (uint8_t)y;
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
uint32_t obj_wait(uint8_t *img, uint32_t script) {
    set_cursor_clip(img, 0, img[script + 1]);
    uint32_t x = img[script + 2];
    uint32_t y = img[script + 3];
    script += 4;
    obj_wait_hit(img, x, y);
    return script;
}

/* FUN_0001680c @ 0x1680c: espera con origen/cursor 6 y (0xb8,0x90); devuelve A0 =
 * puntero de clase 0x1a772 (lo consume 0x169b4). */
uint32_t obj_wait_a0(uint8_t *img) {
    set_cursor_clip(img, 0, 6);
    obj_wait_hit(img, 0xb8, 0x90);
    return be32(img + A_ELEM_CLASS);
}

/* FUN_0001695a @ 0x1695a: formatea el nombre del objeto seleccionado en 0x19970.
 * Con clase (a0[0]) < 0x23 usa a0[1] tal cual y marca 0xf4 en 0x19973; si no,
 * a0[1]-0x14 y 0xd4. Fila = a0[2]-0x28 en 0x19972 y a0[1] ajustado en 0x19971. */
void obj_name_table(uint8_t *img) {
    uint32_t obj = be32(img + A_SELECTED_OBJECT);
    uint32_t x;
    if (img[obj] < 0x23) {
        x = img[obj + 1];
        img[0x19973] = 0xf4;
    } else {
        x = (img[obj] - 0x14) & 0xff;
        img[0x19973] = 0xd4;
    }
    img[0x19972] = (uint8_t)((img[obj + 2] - 0x28) & 0xff);
    img[0x19971] = (uint8_t)x;
}

/* ---- impresion del nombre del objeto seleccionado (0x1687e/0x168a0/0x168d4) ----
 *
 * Las tres saltan el primer byte, leen D4 de A6[0] y posicionan el texto con
 * 0x10fb4 (A5 = 0x3c534); despues imprimen con 0x10b6c sobre la estructura de
 * 0x1996c/0x19970 y esperan (0x1228a -> Cnecin en modo 0x87). La diferencia entre
 * ellas es el sonido (0x168a0), el filtro del nombre (0x168d4) o nada (0x1687e). */
uint32_t text_index_arg(uint8_t *, uint32_t, uint32_t, uint32_t *);
void text_window(uint8_t *, uint32_t, uint32_t);
void hint_blit_stroke(uint8_t *, uint32_t *);
void supexec_b(uint8_t *, uint32_t, uint32_t);

/* FUN_0001687e @ 0x1687e: nombre del objeto sin sonido ni filtro. */
void obj_name(uint8_t *img, uint32_t script) {
    script += 1;
    uint32_t str = text_index_arg(img, 0x3c534, img[script], &script);
    text_window(img, str, 0x1996c);
    script += 1;
}

/* FUN_000168a0 @ 0x168a0: nombre con sonido (0xe6) y espera de tecla. */
void obj_name_snd(uint8_t *img, uint32_t script) {
    supexec_b(img, 0xe6, 0x18);
    script += 1;
    uint32_t str = text_index_arg(img, 0x3c534, img[script], &script);
    obj_name_table(img);
    text_window(img, str, 0x19970);
    (void)prompt_wait_entry(img, 0x19970);
    uint32_t ptr = 0x19970;
    hint_blit_stroke(img, &ptr);
    script += 1;
}

/* FUN_000168d4 @ 0x168d4: como 0x168a0 pero sin sonido y con el bit 6 de 0x19973
 * (el color del nombre) limpio. */
void obj_name_clean(uint8_t *img, uint32_t script) {
    script += 1;
    uint32_t str = text_index_arg(img, 0x3c534, img[script], &script);
    obj_name_table(img);
    img[0x19973] &= (uint8_t)~0x40u;
    text_window(img, str, 0x19970);
    (void)prompt_wait_entry(img, 0x19970);
    uint32_t ptr = 0x19970;
    hint_blit_stroke(img, &ptr);
    script += 1;
}
