/* anim.c — FUN_000159d2 @ 0x159d2: intérprete de animación.
 *
 * Lee una entrada de la tabla A_ANIM_TABLE (0x23ee8, formato [u8 len][data]) elegida
 * por A6[1] (1-based) con `walk_entry` (0x12950), y recorre sus bytes hasta el fin
 * de la entrada (`$1a4b6`):
 *
 *   0xfe  -> $1a47a = sign_ext8(A6[1]); $1a47c = sign_ext8(A6[2]); A6 += 3
 *   0xfd  -> wrappers de sonido FUN_00018f8e/0x98; no-op con el bypass `$1ab4f != 0`
 *   0xfc  -> XBIOS Supexec (FUN_00018fa4); no-op con el bypass; A6 += 1
 *   otro  -> handler del motor de sprites 0x15bde, índice = byte & 7
 *
 * Los 8 handlers (`0x15a90`..) caen al motor `0x15bde` y quedan PENDIENTES; los tests
 * de este primer corte usan solo 0xfe/0xfd/0xfc (ver src/anim.c en el TODO).
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "machine.h"
#include "addrs.h"
#include "kult_port.h"

void walk_entry(uint8_t *img, uint32_t a4, uint32_t d1, uint32_t *a4o, uint32_t *a5o);
uint32_t screen_address(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t *d7out);
void mask_restore_rows(uint8_t *img, uint32_t a0, uint32_t d2, uint32_t d3, uint32_t d7);
void text_blit_fwd(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t a3,
                     uint32_t d2w, uint32_t d3w, uint32_t d7w);
void wait_vbl(uint8_t *img);

void sound_play_a(uint8_t *img, uint32_t d4);
void sound_play_b(uint8_t *img, uint32_t d4);
void sound_stop(uint8_t *img);
void reveal_region(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3);
uint32_t advance_text_entry(uint8_t *img, uint32_t a1, uint32_t d4);
void elem_blit(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t d2, uint32_t d3, uint32_t a3);
void elem_blit_rev(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t d2, uint32_t d3, uint32_t a3);

/* FUN_00015a90 @ 0x15a90: elige el banco de sprites activo. A0 = 0xd8f0 + (bit7 ?
 * 0xc80 : 0) + (bit6 ? 0x640 : 0) y lo guarda en $1a4c6; devuelve A0. */
uint32_t anim_bank_select(uint8_t *img, uint32_t d0) {
    uint32_t a0 = 0xd8f0u;
    uint32_t d1 = 0;
    if (d0 & 0x80) d1 = 0xc80;
    if (d0 & 0x40) d1 += 0x640;
    a0 = addr_add(a0, d1);
    wr32(img + A_ANIM_SPR_PTR, a0);
    return a0;
}

/* FUN_00015d7e @ 0x15d7e: restaura la zona bajo el elemento. Carga $1a476/$1a478 en
 * D2/D3 y $1a472/$1a474 en D0/D1, calcula la dirección (screen_address), corre el
 * blit enmascarado (FUN_0001215e) y limpia $1a478. */
void anim_restore(uint8_t *img) {
    uint32_t d2 = be16(img + A_ANIM_1A476);
    uint32_t d3 = be16(img + A_ANIM_1A478);
    uint32_t d0 = be16(img + A_ANIM_1A472);
    uint32_t d1 = be16(img + A_ANIM_1A474);
    uint32_t d7;
    uint32_t a0 = screen_address(img, d0, d1, &d7);
    mask_restore_rows(img, a0, d2, d3, d7);
    wr16(img + A_ANIM_1A478, 0);
}

/* FUN_00015cd8 @ 0x15cd8: ajusta avance/recorte de un elemento. Lee los flags
 * $1a574 (0..7) y el contador $1a46e; según ellos avanza/decrementa el contador,
 * ajusta D0/D2 y el puntero fuente A1, y puede anular el contador. Devuelve los
 * registros por punteros (no hay efecto en imagen salvo $1a46e). */
void anim_counter(uint8_t *img, uint32_t d0, uint32_t d2, uint32_t d3,
                 uint32_t d4, uint32_t d5, uint32_t a1,
                 uint32_t *d0o, uint32_t *d2o, uint32_t *d3o, uint32_t *a1o) {
    uint32_t fl = img[A_ANIM_1A574] & 7;
    if (fl != 7) {
        uint32_t d6 = be16(img + A_ANIM_1A46E);
        if (fl & 4) {
            if (d6 != 0) {
                if (fl & 2) {
                    d3 = d6;
                    wr16(img + A_ANIM_1A46E, (uint16_t)(d6 - d5));
                    if ((uint16_t)d6 < (uint16_t)d5)
                        wr16(img + A_ANIM_1A46E, 0);
                } else {
                    if (!(fl & 1)) {
                        d0 = (uint16_t)(d0 - d4);
                        d2 = (uint16_t)((uint16_t)(d2 - d6) << 1);
                        a1 = addr_add(a1, d2);
                    }
                    d2 = d6;
                    wr16(img + A_ANIM_1A46E, (uint16_t)(d6 - 1));
                }
            }
        } else if (fl & 2) {
            if (d2 == d6) {
                wr16(img + A_ANIM_1A46E, 0);
            } else {
                if (!(fl & 1)) {
                    d0 = (uint16_t)(d0 - d4);
                    d2 = (uint16_t)((uint16_t)(d2 - d6) << 1);
                    a1 = addr_add(a1, d2);
                }
                d2 = d6;
                wr16(img + A_ANIM_1A46E, (uint16_t)(d6 + 1));
            }
        }
    }
    if (d0o) *d0o = d0;
    if (d2o) *d2o = d2;
    if (d3o) *d3o = d3;
    if (a1o) *a1o = a1;
}

/* FUN_00015da6 @ 0x15da6: dibuja/actualiza un elemento. Respalda la zona del
 * buffer trasero en A_ANIM_BACKUP, blitea el sprite (motor de text_blit_fwd),
 * aplica un retardo calibrado y una espera de VBL (FUN_00013594), dibuja con
 * FUN_00012e2e (reveal) o restaura con FUN_0001215e según $1a578, y finalmente
 * devuelve el respaldo al buffer trasero y actualiza $1a472/4/6/8. */
void anim_engine_entry(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2,
                 uint32_t d3, uint32_t d5, uint32_t a1) {
    /* El original salva D0-D3 con `movem` al entrar y los restaura antes de
     * escribir los globales: $1a472/4/6/8 reciben los valores de ENTRADA del
     * elemento; el resultado del recorte solo alimenta el reveal/restore. */
    uint32_t d0in = d0, d1in = d1, d2in = d2, d3in = d3;
    uint32_t d7;
    uint32_t back = screen_address(img, d0, d1, &d7) - 0x8000u;
    uint32_t a3 = (d5 << 1) & 0xffff;
    uint32_t d2b = ((d2 & 0xffff) >> 2) + 2;
    uint32_t d3b = ((d3 & 0xffff) - 1) & 0xffff;
    wr32(img + A_DAT_1A4B2, back);
    wr16(img + A_DAT_1A49E, (uint16_t)d2b);
    wr16(img + A_ANIM_1A4A0, (uint16_t)d3b);

    {   /* respaldo del buffer trasero */
        uint32_t a0 = back, a2 = A_ANIM_BACKUP, n3 = d3b;
        do {
            uint32_t a4 = a0, n5 = d2b;
            do {
                wr32(img + a2, be32(img + a4)); a4 += 4; a2 += 4;
                wr32(img + a2, be32(img + a4)); a4 += 4; a2 += 4;
                n5 = (n5 - 1) & 0xffff;
            } while (n5 != 0xffff);
            a0 = addr_add(a0, 0xa0);
            n3 = (n3 - 1) & 0xffff;
        } while (n3 != 0xffff);
    }

    text_blit_fwd(img, back, a1, a3, d2 & 0xffff, d3 & 0xffff, d7);

    /* Recorte contra $1a472/4/6/8 (posición/tamaño previos del elemento). */
    d3 = (d3 + d1) & 0xffff;
    d2 = (d2 + d0) & 0xffff;
    if (be16(img + A_ANIM_1A478) != 0) {
        uint32_t c = (be16(img + A_ANIM_1A478) + be16(img + A_ANIM_1A474)) & 0xffff;
        if (c >= d3) d3 = c;
        c = (be16(img + A_ANIM_1A476) + be16(img + A_ANIM_1A472)) & 0xffff;
        if (c >= d2) d2 = c;
        if (d0 >= be16(img + A_ANIM_1A472)) d0 = be16(img + A_ANIM_1A472);
        if (d1 >= be16(img + A_ANIM_1A474)) d1 = be16(img + A_ANIM_1A474);
    }
    d2 = (d2 - d0) & 0xffff;
    d3 = (d3 - d1) & 0xffff;

    uint32_t d7b;
    uint32_t a0b = screen_address(img, d0, d1, &d7b);
    uint32_t delay = be16(img + A_ANIM_1A470);      /* dbf d0 con d0 = ancho */
    /* KULT_ANIM_GATE: multiplicador (%) de la puerta de retardo del motor de
     * elementos, para calibrar el ritmo del motor contra el ST real. */
    { static int gm = -1;
      if (gm < 0) { const char *e = getenv("KULT_ANIM_GATE"); gm = e ? atoi(e) : 100; }
      if (gm <= 0) gm = 100;
      if (getenv("KULT_15DA6")) {
          static FILE *g15 = (FILE *)-1;
          if (g15 == (FILE *)-1) {
              const char *p = getenv("KULT_15DA6");
              g15 = (p && *p && strcmp(p, "1")) ? fopen(p, "w") : fopen("log-15da6.txt", "w");
          }
          if (g15) {
              fprintf(g15, "15da6 delay=%u gm=%d d0=%u d1=%u d2=%u d3=%u clip=%u/%u reveal=%u\n",
                      delay, gm, d0, d1, d2, d3,
                      be16(img + A_ANIM_1A476), be16(img + A_ANIM_1A478), img[A_ANIM_1A578]);
              fflush(g15);
          }
      }
      cpu_delay((uint32_t)((uint64_t)delay * 3200u * (uint32_t)gm / 100u)); }

    wait_vbl(img);                                /* espera de VBL */
    if (img[A_ANIM_1A578] != 0)
        reveal_region(img, d0, d1, d2, d3);
    else
        mask_restore_rows(img, a0b, d2, d3, d7b);

    {   /* devuelve el respaldo al buffer trasero */
        uint32_t a0 = be32(img + A_DAT_1A4B2);
        uint32_t n2 = be16(img + A_DAT_1A49E);
        uint32_t n3 = be16(img + A_ANIM_1A4A0);
        uint32_t a2 = A_ANIM_BACKUP;
        do {
            uint32_t a4 = a0, n5 = n2;
            do {
                wr32(img + a4, be32(img + a2)); a2 += 4; a4 += 4;
                wr32(img + a4, be32(img + a2)); a2 += 4; a4 += 4;
                n5 = (n5 - 1) & 0xffff;
            } while (n5 != 0xffff);
            a0 = addr_add(a0, 0xa0);
            n3 = (n3 - 1) & 0xffff;
        } while (n3 != 0xffff);
    }

    wr16(img + A_ANIM_1A472, (uint16_t)d0in);
    wr16(img + A_ANIM_1A474, (uint16_t)d1in);
    wr16(img + A_ANIM_1A476, (uint16_t)d2in);
    wr16(img + A_ANIM_1A478, (uint16_t)d3in);
    wr16(img + A_ANIM_1A47C, 0);
    wr16(img + A_ANIM_1A47A, 0);
}

/* ---- motor de animación (0x15bda) y handlers de la tabla 0x19eaa ----
 *
 * Cada handler fija unos globales y cae al motor. El motor lee el byte de cabecera
 * (flags en los bits 0..2, nº de bloques en 3..), y por cada bloque un byte de
 * cabecera (ancho en bits 3..15, nº de elementos en 0..2) y luego lee un byte por
 * elemento: elige el banco de sprites (0x15a90), calcula posición con los offsets
 * firmados del byte y los ajustes $1a47a/$1a47c, y dibuja con 0x15cd8 + 0x15da6.
 * Si el contador $1a46e llega a 0 y los flags lo piden, restaura (0x15d7e) y corta.
 * Al salir avanza A6 por 1 + (byte final & 7). */
static uint32_t anim_engine(uint8_t *img, uint32_t a6) {
    uint32_t hdr = img[a6++];
    img[A_ANIM_1A574] = (uint8_t)(hdr & 7);
    uint32_t outer = hdr >> 3;
    uint32_t a6s = a6;                 /* el original salva A6/D6 por bloque (movem) */
    for (uint32_t oi = 0; oi < outer; oi++) {
        uint32_t bh = img[a6++];
        uint32_t w2 = (bh & 0xfff8) >> 2;
        wr16(img + A_ANIM_1A470, (uint16_t)w2);
        img[A_ANIM_1A577] = (uint8_t)(w2 >> 1);
        wr16(img + A_ANIM_1A56A, 0x1f4);
        uint32_t inner = bh & 7;
        for (uint32_t ii = 0; ii < inner; ii++) {
            uint32_t d0 = img[a6++];
            anim_bank_select(img, d0);
            uint32_t d5 = d0 & 7;
            int negx = d5 & 1;
            d5 &= ~1u;
            if (negx) d5 = (uint16_t)(~(uint16_t)d5 + 1);
            uint32_t d1 = (uint16_t)(be16(img + A_ANIM_1A474) + d5);
            d1 = (uint16_t)(d1 + be16(img + A_ANIM_1A47C));
            uint32_t d4 = d0 & 0x38;
            int negy = (d4 >> 3) & 1;          /* último bit que sale en lsr.w #4 */
            d4 >>= 4;
            if (negy) d4 = (uint16_t)(~(uint16_t)d4 + 1);
            d0 = (uint16_t)(be16(img + A_ANIM_1A472) + d4);
            d0 = (uint16_t)(d0 + be16(img + A_ANIM_1A47A));
            uint32_t a1 = be32(img + A_ANIM_SPR_PTR);
            uint32_t d2 = img[a1++];
            uint32_t d3 = img[a1++];
            uint32_t d0o, d2o, d3o, a1o;
            anim_counter(img, d0, d2, d3, d4, d5, a1, &d0o, &d2o, &d3o, &a1o);
            anim_engine_entry(img, d0o, d1, d2o, d3o, d2, a1o);
            uint32_t fl = img[A_ANIM_1A574];
            int stop = 0;
            if (fl & 4) {
                if (be16(img + A_ANIM_1A46E) == 0) { anim_restore(img); stop = 1; }
            } else if (fl & 2) {
                if (be16(img + A_ANIM_1A46E) == 0) stop = 1;
            }
            if (stop) {
                a6 = a6s;
                uint8_t t = img[a6];
                return a6 + 1u + (t & 7);
            }
        }
        a6 = a6s;
    }
    uint8_t t = img[a6];
    return a6 + 1u + (t & 7);
}

/* FUN_00012b40 @ 0x12b40: permutación de bits de un byte (D1 -> D6), usada por el
 * blitter 0x12b04 (orientación invertida). */
uint32_t bit_permute_byte(uint32_t d1) {
    uint8_t b = (uint8_t)d1;
    b = (uint8_t)((b >> 1) | (b << 7));          /* ror.b #1 */
    uint8_t d6 = (uint8_t)(b & 0x22);
    b = (uint8_t)((b >> 2) | (b << 6));          /* ror.b #2 */
    d6 |= (uint8_t)(b & 0x11);
    b = (uint8_t)((b >> 2) | (b << 6));
    d6 |= (uint8_t)(b & 0x88);
    b = (uint8_t)((b >> 2) | (b << 6));
    d6 |= (uint8_t)(b & 0x44);
    return d6;
}

/* FUN_00012ad6 @ 0x12ad6: blitter de bytes directo (D2+1 columnas x D3+1 filas). */
void elem_blit(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t d2, uint32_t d3, uint32_t a3) {
    { FILE *fl = kult_dbgfile();
      if (fl) { fprintf(fl, "12ad6 a0=%x d2=%u d3=%u a3=%x\n", a0, d2, d3, a3); fflush(fl); } }
    uint32_t cols = (d2 - 1) & 0xffff, rows = (d3 - 1) & 0xffff;
    do {
        uint32_t a2 = a0, c = cols;
        do {
            uint8_t d0 = img[a1++], d1 = img[a1++];
            uint8_t d5 = d0, d6 = d1;
            d0 |= d1;
            d1 = d0;
            d1 = (uint8_t)((d1 >> 4) | (d1 << 4));   /* ror.b #4 */
            d0 |= d1;
            d0 = (uint8_t)~d0;
            img[a2] = (uint8_t)(img[a2] & d0); img[a2] = (uint8_t)(img[a2] | d5); a2++;
            img[a2] = (uint8_t)(img[a2] & d0); img[a2] = (uint8_t)(img[a2] | d6); a2++;
            c = (c - 1) & 0xffff;
        } while (c != 0xffff);
        a0 = addr_add(a0, a3);
        rows = (rows - 1) & 0xffff;
    } while (rows != 0xffff);
}

/* FUN_00012b04 @ 0x12b04: variante invertida (permuta el byte con 0x12b40 y escribe
 * hacia atrás desde A0+2*D2). */
void elem_blit_rev(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t d2, uint32_t d3, uint32_t a3) {
    { FILE *fl = kult_dbgfile();
      if (fl) { fprintf(fl, "12b04 a0=%x d2=%u d3=%u a3=%x\n", a0, d2, d3, a3); fflush(fl); } }
    a0 = addr_add(a0, d2);
    a0 = addr_add(a0, d2);
    uint32_t cols = (d2 - 1) & 0xffff, rows = (d3 - 1) & 0xffff;
    do {
        uint32_t a2 = a0, c = cols;
        do {
            uint8_t d5 = (uint8_t)bit_permute_byte(img[a1++]);
            uint8_t d6 = (uint8_t)bit_permute_byte(img[a1++]);
            uint8_t d0 = d5, d1 = d6;
            d0 |= d1;
            d1 = d0;
            d1 = (uint8_t)((d1 >> 4) | (d1 << 4));
            d0 |= d1;
            d0 = (uint8_t)~d0;
            a2 = a2 - 1; img[a2] = (uint8_t)(img[a2] & d0); img[a2] = (uint8_t)(img[a2] | d6);
            a2 = a2 - 1; img[a2] = (uint8_t)(img[a2] & d0); img[a2] = (uint8_t)(img[a2] | d5);
            c = (c - 1) & 0xffff;
        } while (c != 0xffff);
        a0 = addr_add(a0, a3);
        rows = (rows - 1) & 0xffff;
    } while (rows != 0xffff);
}

/* FUN_00015abe @ 0x15abe (handler idx 0): limpia el banco de sprites, elige la
 * entrada `idx` de la tabla 0x27fe2 (walk_entry), copia sus 2 primeros bytes como
 * cabecera del banco y, por cada subentrada de 3 bytes [idx][x_hi|x_flag][x_lo],
 * toma el bitmap idx de la tabla 0x28ad2 y lo blitea (0x12ad6/0x12b04 según el bit7
 * del byte alto del offset). */


uint32_t anim_bank_build(uint8_t *img, uint32_t a6) {
    frame_gate_delay(img);
    anim_bank_select(img, img[a6++]);
    uint32_t idx = img[a6++];
    uint32_t a1 = be32(img + A_ANIM_SPR_PTR);
    for (uint32_t i = 0; i <= 0x18f; i++) { wr32(img + a1, 0); a1 += 4; }
    uint32_t a4, a5;
    walk_entry(img, A_ANIM_TBL_27FE2, idx, &a4, &a5);
    a1 = be32(img + A_ANIM_SPR_PTR);
    img[a1++] = img[a5++];
    img[a1++] = img[a5++];
    for (;;) {
        uint32_t d4 = img[a5];
        a1 = advance_text_entry(img, A_ANIM_TBL_28AD2, d4);
        uint32_t d3 = img[a1++];
        uint32_t d2b = img[a1++];
        uint32_t off = (uint32_t)(((img[a5 + 2] & 0x7f) << 8) | img[a5 + 1]);
        uint32_t a0 = be32(img + A_ANIM_SPR_PTR);
        uint32_t a3 = (uint32_t)(img[a0] << 1);
        a0 += 2;
        a0 = addr_add(a0, off << 1);
        if (img[a5 + 2] & 0x80) elem_blit_rev(img, a0, a1, d2b, d3, a3);
        else elem_blit(img, a0, a1, d2b, d3, a3);
        a5 += 3;
        if (a5 == a4) break;
    }
    return a6;
}

uint32_t anim_h7(uint8_t *img, uint32_t a6) { wr16(img + A_ANIM_1A46E, 0xffff); img[A_ANIM_1A578] = 1; return anim_engine(img, a6); }
uint32_t anim_h1(uint8_t *img, uint32_t a6) { wr16(img + A_ANIM_1A46E, 0xffff); img[A_ANIM_1A578] = 0; return anim_engine(img, a6); }
uint32_t anim_h6(uint8_t *img, uint32_t a6) { wr16(img + A_ANIM_1A46E, be16(img + A_ANIM_1A478)); img[A_ANIM_1A578] = 0; return anim_engine(img, a6); }
uint32_t anim_h2(uint8_t *img, uint32_t a6) { wr16(img + A_ANIM_1A46E, 1); img[A_ANIM_1A578] = 0; return anim_engine(img, a6); }
uint32_t anim_h4(uint8_t *img, uint32_t a6) { wr16(img + A_ANIM_1A46E, (uint16_t)(be16(img + A_ANIM_1A476) - 1)); img[A_ANIM_1A578] = 0; return anim_engine(img, a6); }

uint32_t anim_bank_build(uint8_t *img, uint32_t a6);
uint32_t anim_h7(uint8_t *img, uint32_t a6);
uint32_t anim_h1(uint8_t *img, uint32_t a6);
uint32_t anim_h6(uint8_t *img, uint32_t a6);
uint32_t anim_h2(uint8_t *img, uint32_t a6);
uint32_t anim_h4(uint8_t *img, uint32_t a6);

/* Cuerpo compartido por 0x159d2 y 0x159e2 (entradas gemelas, como 0x11016/0x11020).
 * D1 = índice 1-based de la tabla 0x23ee8, D0/D2 van a 0x1a472/0x1a474. El A6
 * original no lo toca el cuerpo (0x159e2 lo salva en la pila al entrar). */
static void anim_run(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2) {
    wr16(img + A_ANIM_1A476, 0);
    wr16(img + A_ANIM_1A478, 0);
    wr16(img + A_ANIM_1A472, (uint16_t)d0);
    wr16(img + A_ANIM_1A474, (uint16_t)d2);

    uint32_t a4o, a5o;
    walk_entry(img, A_ANIM_TABLE, (d1 - 1) & 0xffff, &a4o, &a5o);
    uint32_t a5 = a5o;
    wr32(img + A_ANIM_END, a4o);

    for (;;) {
        uint32_t b = img[a5];
        if (a5 == a4o) break;
        if (b == 0xfe) {
            wr16(img + A_ANIM_1A47A, (uint16_t)sign_ext8(img[a5 + 1]));
            wr16(img + A_ANIM_1A47C, (uint16_t)sign_ext8(img[a5 + 2]));
            a5 = addr_add(a5, 3);
        } else if (b == 0xfd) {
            /* 0x15a6a: D4 = A6[1] (indice); si A6[2] != 0 -> 0x18f98, si no -> 0x18f8e
             * (el envoltorio salta el trap con el bypass $1ab4f; luego A6 += 3). */
            uint32_t d4s = img[a5 + 1];
            if (img[a5 + 2] != 0)
                sound_play_b(img, d4s);
            else
                sound_play_a(img, d4s);
            a5 = addr_add(a5, 3);
        } else if (b == 0xfc) {
            /* 0x15a86: XBIOS Supexec (0x18fa4, no-op con el bypass $1ab4f); A6 += 1. */
            sound_stop(img);
            a5 = addr_add(a5, 1);
        } else {
            /* Despacho por (byte & 7) a la tabla 0x19eaa (handlers del motor 0x15bde). */
            switch (b & 7) {
            case 0: a5 = anim_bank_build(img, a5); break;
            case 1: a5 = anim_h1(img, a5); break;
            case 2: a5 = anim_h2(img, a5); break;
            case 3: a5 = anim_h2(img, a5); break;
            case 4: a5 = anim_h4(img, a5); break;
            case 5: a5 = anim_h4(img, a5); break;
            case 6: a5 = anim_h6(img, a5); break;
            default: a5 = anim_h7(img, a5); break;
            }
        }
    }
}

/* FUN_000159d2 @ 0x159d2: prólogo que lee D1/D0/D2 del script y cae en el cuerpo. */
void anim_entry_a(uint8_t *img, uint32_t a6) {
    uint32_t d1 = img[a6 + 1] & 0xff;
    a6 += 2;
    uint32_t d0 = img[a6] & 0xff; a6++;
    uint32_t d2 = img[a6] & 0xff; a6++;
    anim_run(img, d0, d1, d2);
}

/* FUN_000159e2 @ 0x159e2: entrada gemela con D0/D1/D2 ya en registros; conserva A6. */
uint32_t anim_entry_b(uint8_t *img, uint32_t a6, uint32_t d0, uint32_t d1, uint32_t d2) {
    anim_run(img, d0, d1, d2);
    return a6;
}

/* FUN_00014f24 @ 0x14f24: gestiona el umbral 0x1ad20. Si D0 <= 0x1ad20 lo resta y,
 * cuando cambia el nibble alto (o 0x1ad20 < D0), dispara la animacion 0x159e2 con
 * D0/D1/D2 = 0x4a/0x44/0x47. La rama de disparo hace `addq.l #4,a7` en el original
 * (se salta el resto de su llamante), asi que su test usa stop_pc=0x14f5c. */
uint32_t anim_threshold(uint8_t *img, uint32_t a6, uint32_t d0) {
    uint32_t d2 = img[A_DAT_1AD20];
    if ((d2 & 0xff) < (d0 & 0xff)) {
        anim_entry_b(img, a6, 0x4a, 0x44, 0x47);
        return 2;                        /* ademas el original salta al llamante */
    }
    uint32_t d1 = d2 & 0xf0;
    d2 = (d2 - (d0 & 0xff)) & 0xff;
    img[A_DAT_1AD20] = (uint8_t)d2;
    if ((d2 & 0xf0) != d1) {
        anim_entry_b(img, a6, 0x4a, 0x44, 0x47);
        return 1;                        /* dispara pero el llamante continua */
    }
    return 0;
}

/* FUN_00015f6c @ 0x15f6c: lee un argumento del script. Devuelve D0 y el puntero A0
 * (0 en la vía corta). D1 = primer byte: bit7 = indirecto por tabla 0x1a752 (índice
 * bits0-4), bit6 = saltar D4 entradas de texto (0x10f8e), bit5 = word (2 bytes). */
uint32_t advance_text_entry(uint8_t *img, uint32_t a1, uint32_t d4);
uint32_t script_arg(uint8_t *img, uint32_t *a6, uint32_t *a0_out) {
    uint32_t p = *a6;
    uint32_t d1 = img[p++];
    if (!(d1 & 0x80)) {
        uint32_t d0 = img[p++];
        if (d1 & 0x20) {
            d0 = ((d0 << 8) | img[p++]) & 0xffff;
            d0 |= 0x80000000u;               /* bset #31,d0 (0x15f86) */
        }
        *a6 = p;
        *a0_out = 0;
        return d0;
    }
    uint32_t d2 = (d1 & 0x1f) << 2;
    uint32_t a1 = be32(img + 0x1a752 + d2);
    if (d1 & 0x40) {
        uint32_t d4 = img[p++];
        a1 = advance_text_entry(img, a1, d4);
    }
    a1 += img[p++];
    uint32_t d0 = img[a1];
    if (d1 & 0x20) {
        d0 = ((d0 << 8) | img[a1 + 1]) & 0xffff;
        d0 |= 0x80000000u;
    }
    *a6 = p;
    *a0_out = a1;
    return d0;
}

/* FUN_00015fd4 @ 0x15fd4: ALU sobre D6/D0 según los bits de D7. Con bit6 = 0 es
 * aritmética (add/sub/and/or/eor); con bit6 = 1 es una comparación -> D6 = 0xffff si
 * alguna condición se cumple, si no 0. Devuelve D6. */
uint32_t script_alu(uint8_t *img, uint32_t d6, uint32_t d0, uint32_t d7) {
    (void)img;
    d7 &= 0xffff;
    /* El original usa operaciones de PALABRA (add.w/sub.w/and.w/or.w/eor.w y
     * move.w/clr.w del resultado): la palabra alta de D6 se conserva intacta.
     * Sin esto, un D6 con palabra baja 0 y alta != 0 (p. ej. 0xc20000) se
     * confundia con 0xffff y el flujo de guion (0x160aa) tomaba otra rama. */
    uint32_t hi = d6 & 0xffff0000u;
    if (!(d7 & 0x40)) {
        if (d7 & 0x20) d6 = hi | ((d6 + d0) & 0xffffu);
        if (d7 & 0x10) d6 = hi | ((d6 - d0) & 0xffffu);
        if (d7 & 0x08) d6 = hi | (d6 & d0 & 0xffffu);
        if (d7 & 0x04) d6 = hi | ((d6 | d0) & 0xffffu);
        if (d7 & 0x02) d6 = hi | ((d6 ^ d0) & 0xffffu);
        return d6;
    }
    /* Rama de comparacion: cmp.w mira solo la palabra baja; el resultado
     * (move.w #$ffff / clr.w) tambien preserva la palabra alta. */
    int hit = 0;
    if (d7 & 0x20) hit |= ((d6 & 0xffff) == (d0 & 0xffff));
    if (d7 & 0x10) hit |= ((d6 & 0xffff) < (d0 & 0xffff));
    if (d7 & 0x08) hit |= ((d6 & 0xffff) > (d0 & 0xffff));
    if (d7 & 0x04) hit |= ((d6 & 0xffff) != (d0 & 0xffff));
    if (d7 & 0x02) hit |= ((int)(int16_t)(d6 & 0xffff) <= (int)(int16_t)(d0 & 0xffff));
    if (d7 & 0x01) hit |= ((int)(int16_t)(d6 & 0xffff) >= (int)(int16_t)(d0 & 0xffff));
    return (d6 & 0xffff0000u) | (hit ? 0xffffu : 0u);
}

/* FUN_0001606e @ 0x1606e: evalúa una expresión: D6 = primer argumento (0x15f6c) y luego,
 * mientras el byte operador (a6)+ no tenga el bit7, aplica 0x15fd4 con el siguiente
 * argumento. Devuelve D6 y actualiza a6/a0. */
uint32_t script_expr(uint8_t *img, uint32_t *a6, uint32_t *a0_out) {
    uint32_t p = *a6;
    uint32_t a0 = 0;
    uint32_t d6 = script_arg(img, &p, &a0);
    for (;;) {
        uint32_t d7 = img[p++];
        if (d7 & 0x80)
            break;
        uint32_t d0 = script_arg(img, &p, &a0);
        d6 = script_alu(img, d6, d0, d7);
    }
    *a6 = p;
    *a0_out = a0;
    return d6 & 0xffff;
}

/* FUN_00016088 @ 0x16088: lee una dirección (0x15f6c) y el valor (0x1606e) y guarda:
 * word si el bit31 de la dirección está puesto, byte si no. */
void script_store(uint8_t *img, uint32_t *a6) {
    uint32_t p = *a6 + 1;
    uint32_t a0, a0b;
    uint32_t d0 = script_arg(img, &p, &a0);
    uint32_t d6 = script_expr(img, &p, &a0b);
    if (d0 & 0x80000000u)
        wr16(img + a0, d6 & 0xffff);
    else
        img[a0] = d6 & 0xff;
    *a6 = p;
}

/* FUN_000160aa @ 0x160aa: evalúa una expresión; si D6 == 0 vuelve una posición y salta a
 * 0x1350c (a6 = TEMPL + word), si no avanza 2. */
void func_1350c(uint8_t *img, uint32_t *a6);
void script_jump_zero(uint8_t *img, uint32_t *a6) {
    uint32_t p = *a6 + 1;
    uint32_t a0b;
    uint32_t d6 = script_expr(img, &p, &a0b);
    if ((d6 & 0xffff) == 0) {
        p -= 1;
        func_1350c(img, &p);
    } else {
        p += 2;
    }
    *a6 = p;
}
