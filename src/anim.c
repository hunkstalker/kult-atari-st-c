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
void elem_blit(uint8_t *img, uint32_t dest, uint32_t src, uint32_t cols, uint32_t rows, uint32_t stride);
void elem_blit_rev(uint8_t *img, uint32_t dest, uint32_t src, uint32_t cols, uint32_t rows, uint32_t stride);

/* FUN_00015a90 @ 0x15a90: elige el banco de sprites activo. A0 = 0xd8f0 + (bit7 ?
 * 0xc80 : 0) + (bit6 ? 0x640 : 0) y lo guarda en $1a4c6; devuelve A0. */
uint32_t anim_bank_select(uint8_t *img, uint32_t flags) {
    uint32_t bank = 0xd8f0u;
    uint32_t off = 0;
    if (flags & 0x80) off = 0xc80;
    if (flags & 0x40) off += 0x640;
    bank = addr_add(bank, off);
    wr32(img + A_ANIM_SPR_PTR, bank);
    return bank;
}

/* FUN_00015d7e @ 0x15d7e: restaura la zona bajo el elemento. Carga $1a476/$1a478 en
 * D2/D3 y $1a472/$1a474 en D0/D1, calcula la dirección (screen_address), corre el
 * blit enmascarado (FUN_0001215e) y limpia $1a478. */
void anim_restore(uint8_t *img) {
    uint32_t w = be16(img + A_ANIM_1A476);
    uint32_t h = be16(img + A_ANIM_1A478);
    uint32_t x = be16(img + A_ANIM_1A472);
    uint32_t y = be16(img + A_ANIM_1A474);
    uint32_t phase;
    uint32_t addr = screen_address(img, x, y, &phase);
    mask_restore_rows(img, addr, w, h, phase);
    wr16(img + A_ANIM_1A478, 0);
}

/* FUN_00015cd8 @ 0x15cd8: ajusta avance/recorte de un elemento. Lee los flags
 * $1a574 (0..7) y el contador $1a46e; según ellos avanza/decrementa el contador,
 * ajusta D0/D2 y el puntero fuente A1, y puede anular el contador. Devuelve los
 * registros por punteros (no hay efecto en imagen salvo $1a46e). */
void anim_counter(uint8_t *img, uint32_t x, uint32_t w, uint32_t h,
                 uint32_t dx, uint32_t dy, uint32_t src,
                 uint32_t *x_out, uint32_t *w_out, uint32_t *h_out, uint32_t *src_out) {
    uint32_t flags = img[A_ANIM_1A574] & 7;
    if (flags != 7) {
        uint32_t count = be16(img + A_ANIM_1A46E);
        if (flags & 4) {
            if (count != 0) {
                if (flags & 2) {
                    h = count;
                    wr16(img + A_ANIM_1A46E, (uint16_t)(count - dy));
                    if ((uint16_t)count < (uint16_t)dy)
                        wr16(img + A_ANIM_1A46E, 0);
                } else {
                    if (!(flags & 1)) {
                        x = (uint16_t)(x - dx);
                        w = (uint16_t)((uint16_t)(w - count) << 1);
                        src = addr_add(src, w);
                    }
                    w = count;
                    wr16(img + A_ANIM_1A46E, (uint16_t)(count - 1));
                }
            }
        } else if (flags & 2) {
            if (w == count) {
                wr16(img + A_ANIM_1A46E, 0);
            } else {
                if (!(flags & 1)) {
                    x = (uint16_t)(x - dx);
                    w = (uint16_t)((uint16_t)(w - count) << 1);
                    src = addr_add(src, w);
                }
                w = count;
                wr16(img + A_ANIM_1A46E, (uint16_t)(count + 1));
            }
        }
    }
    if (x_out) *x_out = x;
    if (w_out) *w_out = w;
    if (h_out) *h_out = h;
    if (src_out) *src_out = src;
}

/* FUN_00015da6 @ 0x15da6: dibuja/actualiza un elemento. Respalda la zona del
 * buffer trasero en A_ANIM_BACKUP, blitea el sprite (motor de text_blit_fwd),
 * aplica un retardo calibrado y una espera de VBL (FUN_00013594), dibuja con
 * FUN_00012e2e (reveal) o restaura con FUN_0001215e según $1a578, y finalmente
 * devuelve el respaldo al buffer trasero y actualiza $1a472/4/6/8. */
void anim_engine_entry(uint8_t *img, uint32_t x, uint32_t y, uint32_t w,
                 uint32_t h, uint32_t stride, uint32_t src) {
    /* El original salva D0-D3 con `movem` al entrar y los restaura antes de
     * escribir los globales: $1a472/4/6/8 reciben los valores de ENTRADA del
     * elemento; el resultado del recorte solo alimenta el reveal/restore. */
    uint32_t x_in = x, y_in = y, w_in = w, h_in = h;
    uint32_t phase;
    uint32_t back = screen_address(img, x, y, &phase) - 0x8000u;
    uint32_t stride2 = (stride << 1) & 0xffff;
    uint32_t cols = ((w & 0xffff) >> 2) + 2;
    uint32_t rows = ((h & 0xffff) - 1) & 0xffff;
    wr32(img + A_DAT_1A4B2, back);
    wr16(img + A_DAT_1A49E, (uint16_t)cols);
    wr16(img + A_ANIM_1A4A0, (uint16_t)rows);

    {   /* respaldo del buffer trasero */
        uint32_t p = back, dst = A_ANIM_BACKUP, n3 = rows;
        do {
            uint32_t src2 = p, n5 = cols;
            do {
                wr32(img + dst, be32(img + src2)); src2 += 4; dst += 4;
                wr32(img + dst, be32(img + src2)); src2 += 4; dst += 4;
                n5 = (n5 - 1) & 0xffff;
            } while (n5 != 0xffff);
            p = addr_add(p, 0xa0);
            n3 = (n3 - 1) & 0xffff;
        } while (n3 != 0xffff);
    }

    text_blit_fwd(img, back, src, stride2, w & 0xffff, h & 0xffff, phase);

    /* Recorte contra $1a472/4/6/8 (posición/tamaño previos del elemento). */
    h = (h + y) & 0xffff;
    w = (w + x) & 0xffff;
    if (be16(img + A_ANIM_1A478) != 0) {
        uint32_t c = (be16(img + A_ANIM_1A478) + be16(img + A_ANIM_1A474)) & 0xffff;
        if (c >= h) h = c;
        c = (be16(img + A_ANIM_1A476) + be16(img + A_ANIM_1A472)) & 0xffff;
        if (c >= w) w = c;
        if (x >= be16(img + A_ANIM_1A472)) x = be16(img + A_ANIM_1A472);
        if (y >= be16(img + A_ANIM_1A474)) y = be16(img + A_ANIM_1A474);
    }
    w = (w - x) & 0xffff;
    h = (h - y) & 0xffff;

    uint32_t phase2;
    uint32_t addr = screen_address(img, x, y, &phase2);
    uint32_t delay = be16(img + A_ANIM_1A470);      /* dbf x con x = ancho */
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
              fprintf(g15, "15da6 delay=%u gm=%d x=%u y=%u w=%u h=%u clip=%u/%u reveal=%u\n",
                      delay, gm, x, y, w, h,
                      be16(img + A_ANIM_1A476), be16(img + A_ANIM_1A478), img[A_ANIM_1A578]);
              fflush(g15);
          }
      }
      cpu_delay((uint32_t)((uint64_t)delay * 3200u * (uint32_t)gm / 100u)); }

    wait_vbl(img);                                /* espera de VBL */
    if (img[A_ANIM_1A578] != 0)
        reveal_region(img, x, y, w, h);
    else
        mask_restore_rows(img, addr, w, h, phase2);

    {   /* devuelve el respaldo al buffer trasero */
        uint32_t p = be32(img + A_DAT_1A4B2);
        uint32_t n2 = be16(img + A_DAT_1A49E);
        uint32_t n3 = be16(img + A_ANIM_1A4A0);
        uint32_t dst = A_ANIM_BACKUP;
        do {
            uint32_t src2 = p, n5 = n2;
            do {
                wr32(img + src2, be32(img + dst)); dst += 4; src2 += 4;
                wr32(img + src2, be32(img + dst)); dst += 4; src2 += 4;
                n5 = (n5 - 1) & 0xffff;
            } while (n5 != 0xffff);
            p = addr_add(p, 0xa0);
            n3 = (n3 - 1) & 0xffff;
        } while (n3 != 0xffff);
    }

    wr16(img + A_ANIM_1A472, (uint16_t)x_in);
    wr16(img + A_ANIM_1A474, (uint16_t)y_in);
    wr16(img + A_ANIM_1A476, (uint16_t)w_in);
    wr16(img + A_ANIM_1A478, (uint16_t)h_in);
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
static uint32_t anim_engine(uint8_t *img, uint32_t script) {
    uint32_t hdr = img[script++];
    img[A_ANIM_1A574] = (uint8_t)(hdr & 7);
    uint32_t blocks = hdr >> 3;
    uint32_t script_saved = script;                 /* el original salva A6/D6 por bloque (movem) */
    for (uint32_t b = 0; b < blocks; b++) {
        uint32_t bh = img[script++];
        uint32_t w = (bh & 0xfff8) >> 2;
        wr16(img + A_ANIM_1A470, (uint16_t)w);
        img[A_ANIM_1A577] = (uint8_t)(w >> 1);
        wr16(img + A_ANIM_1A56A, 0x1f4);
        uint32_t count = bh & 7;
        for (uint32_t i = 0; i < count; i++) {
            uint32_t code = img[script++];
            anim_bank_select(img, code);
            uint32_t off = code & 7;
            int negx = off & 1;
            off &= ~1u;
            if (negx) off = (uint16_t)(~(uint16_t)off + 1);
            uint32_t y = (uint16_t)(be16(img + A_ANIM_1A474) + off);
            y = (uint16_t)(y + be16(img + A_ANIM_1A47C));
            uint32_t x = code & 0x38;
            int negy = (x >> 3) & 1;          /* último bit que sale en lsr.w #4 */
            x >>= 4;
            if (negy) x = (uint16_t)(~(uint16_t)x + 1);
            code = (uint16_t)(be16(img + A_ANIM_1A472) + x);
            code = (uint16_t)(code + be16(img + A_ANIM_1A47A));
            uint32_t spr = be32(img + A_ANIM_SPR_PTR);
            uint32_t w2 = img[spr++];
            uint32_t h2 = img[spr++];
            uint32_t x2, w3, h3, spr2;
            anim_counter(img, code, w2, h2, x, off, spr, &x2, &w3, &h3, &spr2);
            anim_engine_entry(img, x2, y, w3, h3, w2, spr2);
            uint32_t flags = img[A_ANIM_1A574];
            int stop = 0;
            if (flags & 4) {
                if (be16(img + A_ANIM_1A46E) == 0) { anim_restore(img); stop = 1; }
            } else if (flags & 2) {
                if (be16(img + A_ANIM_1A46E) == 0) stop = 1;
            }
            if (stop) {
                script = script_saved;
                uint8_t t = img[script];
                return script + 1u + (t & 7);
            }
        }
        script = script_saved;
    }
    uint8_t t = img[script];
    return script + 1u + (t & 7);
}

/* FUN_00012b40 @ 0x12b40: permutación de bits de un byte (D1 -> D6), usada por el
 * blitter 0x12b04 (orientación invertida). */
uint32_t bit_permute_byte(uint32_t byte) {
    uint8_t b = (uint8_t)byte;
    b = (uint8_t)((b >> 1) | (b << 7));          /* ror.b #1 */
    uint8_t out = (uint8_t)(b & 0x22);
    b = (uint8_t)((b >> 2) | (b << 6));          /* ror.b #2 */
    out |= (uint8_t)(b & 0x11);
    b = (uint8_t)((b >> 2) | (b << 6));
    out |= (uint8_t)(b & 0x88);
    b = (uint8_t)((b >> 2) | (b << 6));
    out |= (uint8_t)(b & 0x44);
    return out;
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


uint32_t anim_bank_build(uint8_t *img, uint32_t script) {
    frame_gate_delay(img);
    anim_bank_select(img, img[script++]);
    uint32_t idx = img[script++];
    uint32_t p = be32(img + A_ANIM_SPR_PTR);
    for (uint32_t i = 0; i <= 0x18f; i++) { wr32(img + p, 0); p += 4; }
    uint32_t end, s;
    walk_entry(img, A_ANIM_TBL_27FE2, idx, &end, &s);
    p = be32(img + A_ANIM_SPR_PTR);
    img[p++] = img[s++];
    img[p++] = img[s++];
    for (;;) {
        uint32_t n = img[s];
        p = advance_text_entry(img, A_ANIM_TBL_28AD2, n);
        uint32_t h = img[p++];
        uint32_t w = img[p++];
        uint32_t off = (uint32_t)(((img[s + 2] & 0x7f) << 8) | img[s + 1]);
        uint32_t addr = be32(img + A_ANIM_SPR_PTR);
        uint32_t stride = (uint32_t)(img[addr] << 1);
        addr += 2;
        addr = addr_add(addr, off << 1);
        if (img[s + 2] & 0x80) elem_blit_rev(img, addr, p, w, h, stride);
        else elem_blit(img, addr, p, w, h, stride);
        s += 3;
        if (s == end) break;
    }
    return script;
}

/* 0x15b94 — anim h7 */
uint32_t anim_h7(uint8_t *img, uint32_t script) { wr16(img + A_ANIM_1A46E, 0xffff); img[A_ANIM_1A578] = 1; return anim_engine(img, script); }
/* 0x15bcc — anim h1 */
uint32_t anim_h1(uint8_t *img, uint32_t script) { wr16(img + A_ANIM_1A46E, 0xffff); img[A_ANIM_1A578] = 0; return anim_engine(img, script); }
/* 0x15ba6 — anim h6 */
uint32_t anim_h6(uint8_t *img, uint32_t script) { wr16(img + A_ANIM_1A46E, be16(img + A_ANIM_1A478)); img[A_ANIM_1A578] = 0; return anim_engine(img, script); }
/* 0x15bb2 — anim h2 */
uint32_t anim_h2(uint8_t *img, uint32_t script) { wr16(img + A_ANIM_1A46E, 1); img[A_ANIM_1A578] = 0; return anim_engine(img, script); }
/* 0x15bbc — anim h4 */
uint32_t anim_h4(uint8_t *img, uint32_t script) { wr16(img + A_ANIM_1A46E, (uint16_t)(be16(img + A_ANIM_1A476) - 1)); img[A_ANIM_1A578] = 0; return anim_engine(img, script); }

uint32_t anim_bank_build(uint8_t *img, uint32_t a6);
uint32_t anim_h7(uint8_t *img, uint32_t a6);
uint32_t anim_h1(uint8_t *img, uint32_t a6);
uint32_t anim_h6(uint8_t *img, uint32_t a6);
uint32_t anim_h2(uint8_t *img, uint32_t a6);
uint32_t anim_h4(uint8_t *img, uint32_t a6);

/* Cuerpo compartido por 0x159d2 y 0x159e2 (entradas gemelas, como 0x11016/0x11020).
 * D1 = índice 1-based de la tabla 0x23ee8, D0/D2 van a 0x1a472/0x1a474. El A6
 * original no lo toca el cuerpo (0x159e2 lo salva en la pila al entrar). */
static void anim_run(uint8_t *img, uint32_t x, uint32_t index, uint32_t y) {
    wr16(img + A_ANIM_1A476, 0);
    wr16(img + A_ANIM_1A478, 0);
    wr16(img + A_ANIM_1A472, (uint16_t)x);
    wr16(img + A_ANIM_1A474, (uint16_t)y);

    uint32_t end, p;
    walk_entry(img, A_ANIM_TABLE, (index - 1) & 0xffff, &end, &p);
    uint32_t s = p;
    wr32(img + A_ANIM_END, end);

    for (;;) {
        uint32_t b = img[s];
        if (s == end) break;
        if (b == 0xfe) {
            wr16(img + A_ANIM_1A47A, (uint16_t)sign_ext8(img[s + 1]));
            wr16(img + A_ANIM_1A47C, (uint16_t)sign_ext8(img[s + 2]));
            s = addr_add(s, 3);
        } else if (b == 0xfd) {
            /* 0x15a6a: D4 = A6[1] (indice); si A6[2] != 0 -> 0x18f98, si no -> 0x18f8e
             * (el envoltorio salta el trap con el bypass $1ab4f; luego A6 += 3). */
            uint32_t id = img[s + 1];
            if (img[s + 2] != 0)
                sound_play_b(img, id);
            else
                sound_play_a(img, id);
            s = addr_add(s, 3);
        } else if (b == 0xfc) {
            /* 0x15a86: XBIOS Supexec (0x18fa4, no-op con el bypass $1ab4f); A6 += 1. */
            sound_stop(img);
            s = addr_add(s, 1);
        } else {
            /* Despacho por (byte & 7) a la tabla 0x19eaa (handlers del motor 0x15bde). */
            switch (b & 7) {
            case 0: s = anim_bank_build(img, s); break;
            case 1: s = anim_h1(img, s); break;
            case 2: s = anim_h2(img, s); break;
            case 3: s = anim_h2(img, s); break;
            case 4: s = anim_h4(img, s); break;
            case 5: s = anim_h4(img, s); break;
            case 6: s = anim_h6(img, s); break;
            default: s = anim_h7(img, s); break;
            }
        }
    }
}

/* FUN_000159d2 @ 0x159d2: prólogo que lee D1/D0/D2 del script y cae en el cuerpo. */
void anim_entry_a(uint8_t *img, uint32_t script) {
    uint32_t index = img[script + 1] & 0xff;
    script += 2;
    uint32_t x = img[script] & 0xff; script++;
    uint32_t y = img[script] & 0xff; script++;
    anim_run(img, x, index, y);
}

/* FUN_000159e2 @ 0x159e2: entrada gemela con D0/D1/D2 ya en registros; conserva A6. */
uint32_t anim_entry_b(uint8_t *img, uint32_t script, uint32_t x, uint32_t index, uint32_t y) {
    anim_run(img, x, index, y);
    return script;
}

/* FUN_00014f24 @ 0x14f24: gestiona el umbral 0x1ad20. Si D0 <= 0x1ad20 lo resta y,
 * cuando cambia el nibble alto (o 0x1ad20 < D0), dispara la animacion 0x159e2 con
 * D0/D1/D2 = 0x4a/0x44/0x47. La rama de disparo hace `addq.l #4,a7` en el original
 * (se salta el resto de su llamante), asi que su test usa stop_pc=0x14f5c. */
uint32_t anim_threshold(uint8_t *img, uint32_t script, uint32_t amount) {
    uint32_t value = img[A_DAT_1AD20];
    if ((value & 0xff) < (amount & 0xff)) {
        anim_entry_b(img, script, 0x4a, 0x44, 0x47);
        return 2;                        /* ademas el original salta al llamante */
    }
    uint32_t hi = value & 0xf0;
    value = (value - (amount & 0xff)) & 0xff;
    img[A_DAT_1AD20] = (uint8_t)value;
    if ((value & 0xf0) != hi) {
        anim_entry_b(img, script, 0x4a, 0x44, 0x47);
        return 1;                        /* dispara pero el llamante continua */
    }
    return 0;
}

/* FUN_00015f6c @ 0x15f6c: lee un argumento del script. Devuelve D0 y el puntero A0
 * (0 en la vía corta). D1 = primer byte: bit7 = indirecto por tabla 0x1a752 (índice
 * bits0-4), bit6 = saltar D4 entradas de texto (0x10f8e), bit5 = word (2 bytes). */
uint32_t advance_text_entry(uint8_t *img, uint32_t a1, uint32_t d4);
/* 0x15f6c — lee un argumento del script (directo/indirecto/texto) */
uint32_t script_arg(uint8_t *img, uint32_t *script, uint32_t *ptr_out) {
    uint32_t p = *script;
    uint32_t flags = img[p++];
    if (!(flags & 0x80)) {
        uint32_t value = img[p++];
        if (flags & 0x20) {
            value = ((value << 8) | img[p++]) & 0xffff;
            value |= 0x80000000u;               /* bset #31,value (0x15f86) */
        }
        *script = p;
        *ptr_out = 0;
        return value;
    }
    uint32_t off = (flags & 0x1f) << 2;
    uint32_t p2 = be32(img + 0x1a752 + off);
    if (flags & 0x40) {
        uint32_t n = img[p++];
        p2 = advance_text_entry(img, p2, n);
    }
    p2 += img[p++];
    uint32_t value = img[p2];
    if (flags & 0x20) {
        value = ((value << 8) | img[p2 + 1]) & 0xffff;
        value |= 0x80000000u;
    }
    *script = p;
    *ptr_out = p2;
    return value;
}

/* FUN_00015fd4 @ 0x15fd4: ALU sobre D6/D0 según los bits de D7. Con bit6 = 0 es
 * aritmética (add/sub/and/or/eor); con bit6 = 1 es una comparación -> D6 = 0xffff si
 * alguna condición se cumple, si no 0. Devuelve D6. */
uint32_t script_alu(uint8_t *img, uint32_t a, uint32_t b, uint32_t op) {
    (void)img;
    op &= 0xffff;
    /* El original usa operaciones de PALABRA (add.w/sub.w/and.w/or.w/eor.w y
     * move.w/clr.w del resultado): la palabra alta de D6 se conserva intacta.
     * Sin esto, un D6 con palabra baja 0 y alta != 0 (p. ej. 0xc20000) se
     * confundia con 0xffff y el flujo de guion (0x160aa) tomaba otra rama. */
    uint32_t hi = a & 0xffff0000u;
    if (!(op & 0x40)) {
        if (op & 0x20) a = hi | ((a + b) & 0xffffu);
        if (op & 0x10) a = hi | ((a - b) & 0xffffu);
        if (op & 0x08) a = hi | (a & b & 0xffffu);
        if (op & 0x04) a = hi | ((a | b) & 0xffffu);
        if (op & 0x02) a = hi | ((a ^ b) & 0xffffu);
        return a;
    }
    /* Rama de comparacion: cmp.w mira solo la palabra baja; el resultado
     * (move.w #$ffff / clr.w) tambien preserva la palabra alta. */
    int hit = 0;
    if (op & 0x20) hit |= ((a & 0xffff) == (b & 0xffff));
    if (op & 0x10) hit |= ((a & 0xffff) < (b & 0xffff));
    if (op & 0x08) hit |= ((a & 0xffff) > (b & 0xffff));
    if (op & 0x04) hit |= ((a & 0xffff) != (b & 0xffff));
    if (op & 0x02) hit |= ((int)(int16_t)(a & 0xffff) <= (int)(int16_t)(b & 0xffff));
    if (op & 0x01) hit |= ((int)(int16_t)(a & 0xffff) >= (int)(int16_t)(b & 0xffff));
    return (a & 0xffff0000u) | (hit ? 0xffffu : 0u);
}

/* FUN_0001606e @ 0x1606e: evalúa una expresión: D6 = primer argumento (0x15f6c) y luego,
 * mientras el byte operador (a6)+ no tenga el bit7, aplica 0x15fd4 con el siguiente
 * argumento. Devuelve D6 y actualiza a6/a0. */
uint32_t script_expr(uint8_t *img, uint32_t *script, uint32_t *ptr_out) {
    uint32_t p = *script;
    uint32_t ptr = 0;
    uint32_t acc = script_arg(img, &p, &ptr);
    for (;;) {
        uint32_t op = img[p++];
        if (op & 0x80)
            break;
        uint32_t arg = script_arg(img, &p, &ptr);
        acc = script_alu(img, acc, arg, op);
    }
    *script = p;
    *ptr_out = ptr;
    return acc & 0xffff;
}

/* FUN_00016088 @ 0x16088: lee una dirección (0x15f6c) y el valor (0x1606e) y guarda:
 * word si el bit31 de la dirección está puesto, byte si no. */
void script_store(uint8_t *img, uint32_t *script) {
    uint32_t p = *script + 1;
    uint32_t addr, addr2;
    uint32_t addr_val = script_arg(img, &p, &addr);
    uint32_t value = script_expr(img, &p, &addr2);
    if (addr_val & 0x80000000u)
        wr16(img + addr, value & 0xffff);
    else
        img[addr] = value & 0xff;
    *script = p;
}

/* FUN_000160aa @ 0x160aa: evalúa una expresión; si D6 == 0 vuelve una posición y salta a
 * 0x1350c (a6 = TEMPL + word), si no avanza 2. */
void script_push(uint8_t *img, uint32_t *a6);
/* 0x160aa — salta a plantilla (0x1350c) o avanza 2 si D6 != 0 */
void script_jump_zero(uint8_t *img, uint32_t *script) {
    uint32_t p = *script + 1;
    uint32_t ptr;
    uint32_t cond = script_expr(img, &p, &ptr);
    if ((cond & 0xffff) == 0) {
        p -= 1;
        script_push(img, &p);
    } else {
        p += 2;
    }
    *script = p;
}
