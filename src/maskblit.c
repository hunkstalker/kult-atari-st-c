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
void blit_mask_setup(uint8_t *img, uint32_t d2, uint32_t d7) {
    uint16_t d0 = (uint16_t)d7;
    uint16_t d1 = (uint16_t)~lsr_w(0xffff, d0);   /* máscara de la primera palabra */
    d0 = (uint16_t)(d0 >> 2);
    uint16_t d5 = (uint16_t)(4 - d0);
    uint16_t d6 = (uint16_t)d2;
    uint16_t d4 = 1;

    /* sub.w d5,d6: d6 se actualiza; bls salta el recuento si d2 <= d5. */
    uint16_t before = d6;
    d6 = (uint16_t)(before - d5);
    if (!(word_sub_extend(before, d5) || d6 == 0)) {
        for (;;) {
            before = d6;
            d4 = (uint16_t)(d4 + 1);
            d6 = (uint16_t)(before - 4);
            if (word_sub_extend(before, 4) || d6 == 0)   /* subi.w / bhi */
                break;
        }
    }

    d6 = (uint16_t)(-d6);                          /* neg.w */
    d6 = (uint16_t)(d6 << 2);                      /* lsl.w #2 */
    uint16_t right = (uint16_t)~asl_w(0xffff, d6); /* asl.w d6,d5 / not.w */

    wr16(img + A_BLIT_MASK_FIRST, d1);
    wr16(img + A_BLIT_MASK_LAST, right);
    wr16(img + A_BLIT_GROUPS, d4);
}

/* Un grupo de 4 palabras mezclado bajo d4; d5 = ~d4. */
static void merge_group(uint8_t *img, uint32_t *a0, uint32_t *a1,
                        uint16_t d4, uint16_t d5) {
    for (int i = 0; i < 4; i++) {
        uint16_t s = be16(img + *a1);
        uint16_t d = be16(img + *a0);
        wr16(img + *a0, (uint16_t)((d & d4) | (s & d5)));
        *a0 += 2;
        *a1 += 2;
    }
}

/* 0x123c0 — blitea una fila enmascarada. */
void blit_mask_row(uint8_t *img, uint32_t a0, uint32_t a1) {
    uint16_t d4 = be16(img + A_BLIT_MASK_FIRST);
    uint16_t d6 = (uint16_t)(be16(img + A_BLIT_GROUPS) - 1);
    if (d6 == 0)                                   /* subq/bne falla: un solo grupo */
        d4 = (uint16_t)(d4 | be16(img + A_BLIT_MASK_LAST));
    uint16_t d5 = (uint16_t)~d4;

    merge_group(img, &a0, &a1, d4, d5);

    for (;;) {
        uint16_t before = d6;
        d6 = (uint16_t)(before - 1);               /* subq.w #1,d6 */
        if (word_sub_extend(before, 1))            /* bcs: borrow -> fin */
            break;
        if (d6 == 0) {                             /* beq: último grupo enmascarado */
            d4 = be16(img + A_BLIT_MASK_LAST);
            d5 = (uint16_t)~d4;
            merge_group(img, &a0, &a1, d4, d5);
            break;
        }
        wr32(img + a0, be32(img + a1));            /* move.l (a1)+,(a0)+ x2 */
        a0 += 4;
        a1 += 4;
        wr32(img + a0, be32(img + a1));
        a0 += 4;
        a1 += 4;
    }
}

/* 0x1215e — corre el blit de fila sobre D3 filas. */
void mask_restore_rows(uint8_t *img, uint32_t a0, uint32_t d2, uint32_t d3, uint32_t d7) {
    blit_mask_setup(img, d2, d7);
    if (img[A_BLIT_D8EA] != 0x41)
        img[A_TEMPL_DATA] = 0;
    uint32_t n = (d3 - 1) & 0xffff;                /* dbf cuenta pasadas del cuerpo */
    do {
        blit_mask_row(img, a0, a0 - 0x8000u);
        a0 += 0xa0;
        n = (n - 1) & 0xffff;
    } while (n != 0xffff);
}

/* 0x12466 — copia D3 filas desde el buffer 0x1a534 con máscaras de borde. */
void blit_mask_rows(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t d3) {
    uint32_t d7 = (d3 - 1) & 0xffff;
    uint32_t d4 = be16(img + 0x1a538);
    uint32_t d5 = be16(img + 0x1a53a);
    uint32_t a2 = a0, a3 = a1;
    for (;;) {
        uint32_t d6 = (be16(img + 0x1a53c) - 1) & 0xffff;
        uint32_t src[4];
        src[0] = be16(img + a1); a1 += 2;
        src[1] = be16(img + a1); a1 += 2;
        src[2] = be16(img + a1); a1 += 2;
        src[3] = be16(img + a1); a1 += 2;
        for (uint32_t p = 0; p < 4; p++) {
            wr16(img + a0, ((be16(img + a0) & d4) | src[p]) & 0xffff);
            a0 += 2;
        }
        for (;;) {
            d6 = (d6 - 1) & 0xffff;
            if (d6 == 0)
                break;
            wr32(img + a0, be32(img + a1)); a0 += 4; a1 += 4;
            wr32(img + a0, be32(img + a1)); a0 += 4; a1 += 4;
        }
        src[0] = be16(img + a1); a1 += 2;
        src[1] = be16(img + a1); a1 += 2;
        src[2] = be16(img + a1); a1 += 2;
        src[3] = be16(img + a1); a1 += 2;
        for (uint32_t p = 0; p < 4; p++) {
            wr16(img + a0, ((be16(img + a0) & d5) | src[p]) & 0xffff);
            a0 += 2;
        }
        a0 = a2 + 0xa0;
        a1 = a3 + 0x40;
        a2 = a0;
        a3 = a1;
        d7 = (d7 - 1) & 0xffff;
        if (d7 == 0xffff)
            break;
    }
    wait_vbl(img);
}

/* 0x120a8 — busca hueco libre en la tabla de 10 entradas de 16 B en 0x1a850 y guarda
 * [word d4][long a0][word d7][word d0][word d1][word d2][word d3]. */
void hint_slot_alloc(uint8_t *img, uint32_t d4, uint32_t a0, uint32_t d7,
                     uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3) {
    uint32_t a2 = 0x1a850;
    for (uint32_t d6 = 9;; d6 = (d6 - 1) & 0xffff) {
        if (be16(img + a2) == 0)
            break;
        a2 += 0x10;
        if (d6 == 0)
            break;
    }
    wr16(img + a2, d4 & 0xffff); a2 += 2;
    wr32(img + a2, a0); a2 += 4;
    wr16(img + a2, d7 & 0xffff); a2 += 2;
    wr16(img + a2, d0 & 0xffff); a2 += 2;
    wr16(img + a2, d1 & 0xffff); a2 += 2;
    wr16(img + a2, d2 & 0xffff); a2 += 2;
    wr16(img + a2, d3 & 0xffff);
    img[0x1ad0a] = img[0x1a851];
}

/* Cuerpo compartido de selección de icono (0x120f8): dado A1 (entrada de la tabla) y
 * D1 (valor a escribir), devuelve out[0..6] = a0, d4, d7, d0, d1, d2, d3; out[7] = d5. */
static void hint_body(uint8_t *img, uint32_t a1, uint32_t d1, uint32_t *o) {
    uint32_t d4 = be16(img + a1);
    wr16(img + a1, d1 & 0xffff);
    uint32_t dest = be32(img + a1 + 2);
    uint32_t d0 = (be32(img + A_SCREEN_PTR_A) & 0xffff0000u) | (dest & 0xffff);
    uint32_t a0 = d0;
    uint32_t d7 = be16(img + a1 + 6);
    d0 = (a0 & 0xffff0000u) | be16(img + a1 + 8);
    d1 = be16(img + a1 + 10);
    uint32_t d2 = be16(img + a1 + 12);
    uint32_t d3 = be16(img + a1 + 14);
    img[0x1ad0a] = img[0x1a851];
    o[0] = a0; o[1] = d4; o[2] = d7; o[3] = d0; o[4] = d1; o[5] = d2; o[6] = d3; o[7] = 0;
}

/* 0x120dc — selecciona el icono d0-ésimo (D1=2). */
void hint_select_on(uint8_t *img, uint32_t d0, uint32_t *o) {
    hint_body(img, 0x1a850 + ((d0 - 1) & 0xffff) * 0x10, 2, o);
}

/* 0x120e4 — selecciona el icono d0-ésimo (D1=0). */
void hint_select_off(uint8_t *img, uint32_t d0, uint32_t *o) {
    hint_body(img, 0x1a850 + ((d0 - 1) & 0xffff) * 0x10, 0, o);
}

/* 0x1213a — busca el word d0 en la tabla; si lo encuentra lo desactiva (d5=0). */
void hint_find(uint8_t *img, uint32_t d0, uint32_t *o) {
    uint32_t a1 = 0x1a850;
    for (uint32_t d6 = 9;; d6 = (d6 - 1) & 0xffff) {
        if (be16(img + a1) == (d0 & 0xffff)) {
            hint_body(img, a1, 0, o);
            return;
        }
        a1 += 0x10;
        if (d6 == 0)
            break;
    }
    o[7] = 1;
}

/* 0x1222e — selecciona el icono A6[1] y lo blitea; A6 += 2. */
void hint_blit_entry(uint8_t *img, uint32_t a6, uint32_t *a6_out) {
    uint32_t o[8];
    hint_select_off(img, img[a6 + 1], o);
    mask_restore_rows(img, o[0], o[5], o[6], o[2]);      /* a0, d2, d3, d7 */
    *a6_out = a6 + 2;
}

/* 0x1232e — blitea los iconos 1 y un trazo de 2x15 desde la dirección del icono. */
void hint_blit_stroke(uint8_t *img, uint32_t *a6) {
    *a6 += 1;
    wr16(img + 0x1a484, 0);
    for (;;) {
        uint32_t o[8];
        hint_find(img, 1, o);
        if (o[7] != 0)
            break;
        mask_restore_rows(img, o[0], o[5], o[6], o[2]);
        uint32_t d7;
        uint32_t a0 = screen_address(img, o[3], o[4], &d7);
        mask_restore_rows(img, a0, 2, 0xf, d7);
    }
}

/* 0x12368 — blitea los iconos 2. */
void hint_blit_2(uint8_t *img, uint32_t *a6) {
    *a6 += 1;
    for (;;) {
        uint32_t o[8];
        hint_find(img, 2, o);
        if (o[7] != 0)
            break;
        mask_restore_rows(img, o[0], o[5], o[6], o[2]);
    }
}

/* 0x12384 — blitea los iconos 3. */
void hint_blit_3(uint8_t *img, uint32_t *a6) {
    *a6 += 1;
    for (;;) {
        uint32_t o[8];
        hint_find(img, 3, o);
        if (o[7] != 0)
            break;
        mask_restore_rows(img, o[0], o[5], o[6], o[2]);
    }
}
