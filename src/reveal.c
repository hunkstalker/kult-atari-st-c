/* reveal.c — el efecto "reveal" pixelado.
 *
 * FUN_00012ef6 @ 0x12ef6: mezcla 4 palabras leídas de A0-0x8000 en A0 bajo la
 * máscara D4 rotada a derechas por el bit de fase D7 (D5 = ~máscara). Es el "dot"
 * que el reveal dibuja. Entrada también desde 0x12edc con D4=0x8000.
 *
 * FUN_00012e2e @ 0x12e2e: recorre los pixels de la región en pasos de D2 (byte en
 * 0x1a577), calcula (x,y) del pixel con un `divu` por el ancho, dibuja el dot con
 * FUN_00012ef6 y ejecuta un retardo (D3=0x1a56a) para que la revelación se vea paso
 * a paso. D0/D2 se usan ya desplazados <<2 (0x10fee no re-escala).
 */
#include <stdint.h>
#include <stdlib.h>

#include "machine.h"
#include "addrs.h"
#include "kult_port.h"

uint32_t screen_address_raw(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t *d7out);

/* KULT_REVEAL_GATE: multiplicador (%) del retardo por punto del reveal, que es
 * el que fija la cadencia de los "frames de retrato" (p. ej. el habla del
 * Maestro: un frame por reveal completado). 100 = valor actual. */
static uint32_t reveal_gate(uint32_t units) {
    static int gm = -1;
    if (gm < 0) { const char *e = getenv("KULT_REVEAL_GATE"); gm = e ? atoi(e) : 100; }
    if (gm <= 0) gm = 100;
    return (uint32_t)((uint64_t)units * (uint32_t)gm / 100u);
}

/* FUN_00012ef6 @ 0x12ef6 */
void reveal_dot(uint8_t *img, uint32_t a0, uint32_t d4, uint32_t d7) {
    uint32_t a3 = a0 - 0x8000u;
    uint16_t k = (uint16_t)(d7 & 15);
    uint16_t m = (uint16_t)d4;
    m = (uint16_t)((m >> k) | (m << (16 - k)));   /* ror.w d7,d4 */
    uint16_t n = (uint16_t)~m;
    for (int i = 0; i < 4; i++) {
        uint16_t v = (uint16_t)(be16(img + a3) & m);
        uint16_t d = be16(img + a0);
        wr16(img + a0, (uint16_t)((d & n) | v));
        a3 += 2;
        a0 += 2;
    }
}

/* FUN_00012e2e @ 0x12e2e */
void reveal_region(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3) {
    d0 = (d0 << 2) & 0xffff;
    d2 = (d2 << 2) & 0xffff;
    wr16(img + A_REV_X0, (uint16_t)d0);
    wr16(img + A_REV_Y0, (uint16_t)d1);
    wr16(img + A_REV_W, (uint16_t)d2);
    uint32_t total = (uint16_t)(d2 * d3);         /* mulu.w d2,d3 */
    wr16(img + A_REV_TOTAL, (uint16_t)total);

    uint32_t step = img[A_ANIM_1A577] & 0xff;
    uint32_t x0 = be16(img + A_REV_X0);
    uint32_t y0 = be16(img + A_REV_Y0);
    uint32_t width = be16(img + A_REV_W);
    uint32_t delay = be16(img + A_ANIM_1A56A);

    uint32_t acc = 0;
    for (;;) {
        uint32_t w = width ? width : 0x10000;      /* divu.w: cociente en word bajo */
        uint32_t q = acc / w, r = acc % w;
        uint32_t d7;
        uint32_t a0 = screen_address_raw(img, (x0 + r) & 0xffff, (y0 + q) & 0xffff, &d7);
        reveal_dot(img, a0, 0x8000, d7);
        cpu_delay(reveal_gate(delay));           /* dbf de espera del original (KULT_REVEAL_GATE) */
        acc = (acc + step) & 0xffff;
        if (acc == total) break;
        if (acc > total) acc = (acc - total) & 0xffff;
    }
}


static uint32_t ror16b(uint32_t v, uint32_t n) {
    n &= 15;
    return n ? (((v >> n) | (v << (16 - n))) & 0xffff) : (v & 0xffff);
}
static uint32_t rol16b(uint32_t v, uint32_t n) {
    n &= 15;
    return n ? (((v << n) | (v >> (16 - n))) & 0xffff) : (v & 0xffff);
}

/* FUN_00012fb6 @ 0x12fb6: escribe 4 words en A0: para cada uno, AND con la máscara
 * invertida (rotada de 0x8000 por D7) y OR con la parte correspondiente de D0. Puro
 * bucle de píxeles. */
void func_12fb6(uint8_t *img, uint32_t a0, uint32_t d0, uint32_t d7) {
    uint32_t d4 = ror16b(0x8000, d7);
    uint32_t d5 = (~d4) & 0xffff;
    d0 = ror16b(d0, d7);
    for (int i = 0; i < 4; i++) {
        uint32_t v = (be16(img + a0) & d5) | (d0 & d4);
        wr16(img + a0, v & 0xffff);
        a0 += 2;
        d0 = rol16b(d0, 4);
    }
}

/* FUN_00012f28 @ 0x12f28: revelado pixel a pixel del elemento. Tras 128a4 reescala
 * x/ancho <<2, fija el total (alto*ancho) en 0x1a49e y recorre las posiciones en
 * pasos de 0x11 (mod total) pintando el bit (0x12fb6) con retardo entre pasos. */
uint32_t func_128a4(uint8_t *img, uint32_t a6);
uint32_t elem_blit_vbl(uint8_t *img, uint32_t a6);

uint32_t reveal_pixel(uint8_t *img, uint32_t a6) {
    if (img[A_SKIP_TRANSITION] != 0)
        return elem_blit_vbl(img, a6);
    a6 = func_128a4(img, a6);
    uint32_t x = be16(img + A_ELEM_1A532);
    uint32_t w = be16(img + A_ELEM_1A52E);
    uint32_t h = be16(img + A_ELEM_1A52C);
    wr16(img + A_ELEM_1A532, (x << 2) & 0xffff);
    wr16(img + A_ELEM_1A52E, (w << 2) & 0xffff);
    uint32_t total = (h * ((w << 2) & 0xffff)) & 0xffff;
    wr16(img + A_DAT_1A49E, total);

    uint32_t a1 = be32(img + A_ELEM_1A528);
    uint32_t width = be16(img + A_ELEM_1A52E);
    uint32_t acc = 0;
    for (;;) {
        uint32_t bit = acc & 3;
        uint32_t off = (acc >> 1) & 0xfffffffeu;
        uint32_t div = width ? width : 0x10000;
        uint32_t q = acc / div, r = acc % div;
        uint32_t d7;
        uint32_t a0 = screen_address_raw(img, (be16(img + A_ELEM_1A532) + r) & 0xffff,
                                          (be16(img + A_ELEM_1A530) + q) & 0xffff, &d7);
        uint32_t v = (be16(img + a1 + off) << bit) & 0xffff;
        func_12fb6(img, a0, v, d7);
        cpu_delay(reveal_gate(0x64));            /* dbf 0x64 del original */
        acc = (acc + 0x11) & 0xffff;
        if (acc == total)
            break;
        if (acc > total)
            acc = (acc - total) & 0xffff;
    }
    return a6;
}
