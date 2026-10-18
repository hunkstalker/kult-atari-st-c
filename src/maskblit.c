/* maskblit.c — familia del blit de filas enmascarado y tabla de hints.
 *
 *   blit_mask_setup  @ 0x12698 : precalcula las máscaras de borde (0x1a538 primera
 *                                palabra, 0x1a53a última, 0x1a53c nº de grupos de
 *                                4 palabras) para un blit de D2 px con fase D7.
 *   blit_mask_row    @ 0x123c0 : blitea una fila de A1 a A0: primer grupo de 4
 *                                palabras enmascarado con 0x1a538 (OR 0x1a53a si hay
 *                                un solo grupo), grupos centrales literales, último
 *                                enmascarado con 0x1a53a.
 *   mask_restore_rows@ 0x1215e : corre el blit de fila sobre D3 filas; fuente
 *                                A0-0x8000 (buffer trasero), stride 0xa0.
 *   blit_mask_rows   @ 0x12466 : copia D3 filas desde el buffer 0x1a534 con máscaras
 *                                de borde; cierra con una espera de VBL.
 *   hint_slot_alloc  @ 0x120a8 : busca hueco libre en la tabla de 10 entradas de 16 B
 *                                (0x1a850) y guarda el registro del icono.
 *   hint_select_on   @ 0x120dc : selecciona el icono d0-ésimo (D1=2).
 *   hint_select_off  @ 0x120e4 : idem con D1=0.
 *   hint_find        @ 0x1213a : busca el word d0 en la tabla y lo desactiva; d5=0/1.
 *   hint_blit_entry  @ 0x1222e : selecciona el icono A6[1] y lo blitea; A6 += 2.
 *   hint_blit_stroke @ 0x1232e : blitea los iconos 1 y un trazo de 2x15.
 *   hint_blit_2/3    @ 0x12368/0x12384 : blitea los iconos 2/3.
 *
 * Todo el efecto es la imagen; las esperas de VBL se liberan con `schedule`.
 */
#include <stdint.h>

#include "machine.h"
#include "addrs.h"

uint32_t screen_address(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t *d7);
void wait_vbl(uint8_t *img);

/* 68k: el resultado de un desplazamiento de word es 0 si el count (mod 64) >= 16. */
static uint16_t lsr_w(uint16_t v, unsigned count) {
    count &= 63;
    return count >= 16 ? 0 : (uint16_t)(v >> count);
}
static uint16_t asl_w(uint16_t v, unsigned count) {
    count &= 63;
    return count >= 16 ? 0 : (uint16_t)(v << count);
}

/* 0x12698 — precalcula las máscaras de borde. */
void blit_mask_setup(uint8_t *img, uint32_t width, uint32_t phase) {
    uint16_t phase16 = (uint16_t)phase;
    uint16_t first = (uint16_t)~lsr_w(0xffff, phase16);   /* máscara de la primera palabra */
    phase16 = (uint16_t)(phase16 >> 2);
    uint16_t rem = (uint16_t)(4 - phase16);
    uint16_t acc = (uint16_t)width;
    uint16_t groups = 1;

    /* sub.w rem,acc: acc se actualiza; bls salta el recuento si width <= rem. */
    uint16_t before = acc;
    acc = (uint16_t)(before - rem);
    if (!(word_sub_extend(before, rem) || acc == 0)) {
        for (;;) {
            before = acc;
            groups = (uint16_t)(groups + 1);
            acc = (uint16_t)(before - 4);
            if (word_sub_extend(before, 4) || acc == 0)   /* subi.w / bhi */
                break;
        }
    }

    acc = (uint16_t)(-acc);                          /* neg.w */
    acc = (uint16_t)(acc << 2);                      /* lsl.w #2 */
    uint16_t last = (uint16_t)~asl_w(0xffff, acc); /* asl.w acc,rem / not.w */

    wr16(img + A_BLIT_MASK_FIRST, first);
    wr16(img + A_BLIT_MASK_LAST, last);
    wr16(img + A_BLIT_GROUPS, groups);
}

/* Un grupo de 4 palabras mezclado bajo d4; d5 = ~d4. */
static void merge_group(uint8_t *img, uint32_t *dest, uint32_t *src,
                        uint16_t mask, uint16_t inv) {
    for (int i = 0; i < 4; i++) {
        uint16_t s = be16(img + *src);
        uint16_t d = be16(img + *dest);
        wr16(img + *dest, (uint16_t)((d & mask) | (s & inv)));
        *dest += 2;
        *src += 2;
    }
}

/* 0x123c0 — blitea una fila enmascarada. */
void blit_mask_row(uint8_t *img, uint32_t dest, uint32_t src) {
    uint16_t first = be16(img + A_BLIT_MASK_FIRST);
    uint16_t groups = (uint16_t)(be16(img + A_BLIT_GROUPS) - 1);
    if (groups == 0)                                   /* subq/bne falla: un solo grupo */
        first = (uint16_t)(first | be16(img + A_BLIT_MASK_LAST));
    uint16_t inv = (uint16_t)~first;

    merge_group(img, &dest, &src, first, inv);

    for (;;) {
        uint16_t before = groups;
        groups = (uint16_t)(before - 1);               /* subq.w #1,groups */
        if (word_sub_extend(before, 1))            /* bcs: borrow -> fin */
            break;
        if (groups == 0) {                             /* beq: último grupo enmascarado */
            first = be16(img + A_BLIT_MASK_LAST);
            inv = (uint16_t)~first;
            merge_group(img, &dest, &src, first, inv);
            break;
        }
        wr32(img + dest, be32(img + src));            /* move.l (src)+,(dest)+ x2 */
        dest += 4;
        src += 4;
        wr32(img + dest, be32(img + src));
        dest += 4;
        src += 4;
    }
}

/* 0x1215e — corre el blit de fila sobre D3 filas. */
void mask_restore_rows(uint8_t *img, uint32_t dest, uint32_t cols, uint32_t rows, uint32_t phase) {
    blit_mask_setup(img, cols, phase);
    if (img[A_BLIT_D8EA] != 0x41)
        img[A_TEMPL_DATA] = 0;
    uint32_t n = (rows - 1) & 0xffff;                /* dbf cuenta pasadas del cuerpo */
    do {
        blit_mask_row(img, dest, dest - 0x8000u);
        dest += 0xa0;
        n = (n - 1) & 0xffff;
    } while (n != 0xffff);
}

/* 0x12466 — copia D3 filas desde el buffer 0x1a534 con máscaras de borde. */
void blit_mask_rows(uint8_t *img, uint32_t dest, uint32_t s, uint32_t rows) {
    uint32_t row = (rows - 1) & 0xffff;
    uint32_t first = be16(img + 0x1a538);
    uint32_t last = be16(img + 0x1a53a);
    uint32_t dst = dest, src2 = s;
    for (;;) {
        uint32_t groups = (be16(img + 0x1a53c) - 1) & 0xffff;
        uint32_t src[4];
        src[0] = be16(img + s); s += 2;
        src[1] = be16(img + s); s += 2;
        src[2] = be16(img + s); s += 2;
        src[3] = be16(img + s); s += 2;
        for (uint32_t p = 0; p < 4; p++) {
            wr16(img + dest, ((be16(img + dest) & first) | src[p]) & 0xffff);
            dest += 2;
        }
        for (;;) {
            groups = (groups - 1) & 0xffff;
            if (groups == 0)
                break;
            wr32(img + dest, be32(img + s)); dest += 4; s += 4;
            wr32(img + dest, be32(img + s)); dest += 4; s += 4;
        }
        src[0] = be16(img + s); s += 2;
        src[1] = be16(img + s); s += 2;
        src[2] = be16(img + s); s += 2;
        src[3] = be16(img + s); s += 2;
        for (uint32_t p = 0; p < 4; p++) {
            wr16(img + dest, ((be16(img + dest) & last) | src[p]) & 0xffff);
            dest += 2;
        }
        dest = dst + 0xa0;
        s = src2 + 0x40;
        dst = dest;
        src2 = s;
        row = (row - 1) & 0xffff;
        if (row == 0xffff)
            break;
    }
    wait_vbl(img);
}

/* 0x120a8 — busca hueco libre en la tabla de 10 entradas de 16 B en 0x1a850 y guarda
 * [word d4][long a0][word d7][word d0][word d1][word d2][word d3]. */
void hint_slot_alloc(uint8_t *img, uint32_t word0, uint32_t arg0, uint32_t phase,
                     uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4) {
    uint32_t p = 0x1a850;
    for (uint32_t i = 9;; i = (i - 1) & 0xffff) {
        if (be16(img + p) == 0)
            break;
        p += 0x10;
        if (i == 0)
            break;
    }
    wr16(img + p, word0 & 0xffff); p += 2;
    wr32(img + p, arg0); p += 4;
    wr16(img + p, phase & 0xffff); p += 2;
    wr16(img + p, arg1 & 0xffff); p += 2;
    wr16(img + p, arg2 & 0xffff); p += 2;
    wr16(img + p, arg3 & 0xffff); p += 2;
    wr16(img + p, arg4 & 0xffff);
    img[0x1ad0a] = img[0x1a851];
}

/* Cuerpo compartido de selección de icono (0x120f8): dado A1 (entrada de la tabla) y
 * D1 (valor a escribir), devuelve out[0..6] = a0, d4, d7, d0, d1, d2, d3; out[7] = d5. */
static void hint_body(uint8_t *img, uint32_t entry, uint32_t value, uint32_t *o) {
    uint32_t word = be16(img + entry);
    wr16(img + entry, value & 0xffff);
    uint32_t dest = be32(img + entry + 2);
    uint32_t tmp = (be32(img + A_SCREEN_PTR_A) & 0xffff0000u) | (dest & 0xffff);
    uint32_t addr = tmp;
    uint32_t phase = be16(img + entry + 6);
    tmp = (addr & 0xffff0000u) | be16(img + entry + 8);
    value = be16(img + entry + 10);
    uint32_t w = be16(img + entry + 12);
    uint32_t h = be16(img + entry + 14);
    img[0x1ad0a] = img[0x1a851];
    o[0] = addr; o[1] = word; o[2] = phase; o[3] = tmp; o[4] = value; o[5] = w; o[6] = h; o[7] = 0;
}

/* 0x120dc — selecciona el icono d0-ésimo (D1=2). */
void hint_select_on(uint8_t *img, uint32_t index, uint32_t *out) {
    hint_body(img, 0x1a850 + ((index - 1) & 0xffff) * 0x10, 2, out);
}

/* 0x120e4 — selecciona el icono d0-ésimo (D1=0). */
void hint_select_off(uint8_t *img, uint32_t index, uint32_t *out) {
    hint_body(img, 0x1a850 + ((index - 1) & 0xffff) * 0x10, 0, out);
}

/* 0x1213a — busca el word d0 en la tabla; si lo encuentra lo desactiva (d5=0). */
void hint_find(uint8_t *img, uint32_t word, uint32_t *out) {
    uint32_t p = 0x1a850;
    for (uint32_t i = 9;; i = (i - 1) & 0xffff) {
        if (be16(img + p) == (word & 0xffff)) {
            hint_body(img, p, 0, out);
            return;
        }
        p += 0x10;
        if (i == 0)
            break;
    }
    out[7] = 1;
}

/* 0x1222e — selecciona el icono A6[1] y lo blitea; A6 += 2. */
void hint_blit_entry(uint8_t *img, uint32_t script, uint32_t *script_out) {
    uint32_t out[8];
    hint_select_off(img, img[script + 1], out);
    mask_restore_rows(img, out[0], out[5], out[6], out[2]);      /* a0, d2, d3, d7 */
    *script_out = script + 2;
}

/* 0x1232e — blitea los iconos 1 y un trazo de 2x15 desde la dirección del icono. */
void hint_blit_stroke(uint8_t *img, uint32_t *script) {
    *script += 1;
    wr16(img + 0x1a484, 0);
    for (;;) {
        uint32_t out[8];
        hint_find(img, 1, out);
        if (out[7] != 0)
            break;
        mask_restore_rows(img, out[0], out[5], out[6], out[2]);
        uint32_t phase;
        uint32_t addr = screen_address(img, out[3], out[4], &phase);
        mask_restore_rows(img, addr, 2, 0xf, phase);
    }
}

/* 0x12368 — blitea los iconos 2. */
void hint_blit_2(uint8_t *img, uint32_t *script) {
    *script += 1;
    for (;;) {
        uint32_t out[8];
        hint_find(img, 2, out);
        if (out[7] != 0)
            break;
        mask_restore_rows(img, out[0], out[5], out[6], out[2]);
    }
}

/* 0x12384 — blitea los iconos 3. */
void hint_blit_3(uint8_t *img, uint32_t *script) {
    *script += 1;
    for (;;) {
        uint32_t out[8];
        hint_find(img, 3, out);
        if (out[7] != 0)
            break;
        mask_restore_rows(img, out[0], out[5], out[6], out[2]);
    }
}
