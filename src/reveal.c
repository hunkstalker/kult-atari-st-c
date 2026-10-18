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
#include "kult_internal.h"

uint32_t screen_address_raw(uint8_t *, uint32_t, uint32_t, uint32_t *);

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
void reveal_dot(uint8_t *img, uint32_t dest, uint32_t mask, uint32_t phase) {
    uint32_t src = dest - 0x8000u;
    uint16_t k = (uint16_t)(phase & 15);
    uint16_t m = (uint16_t)mask;
    m = (uint16_t)((m >> k) | (m << (16 - k)));   /* ror.w phase,mask */
    uint16_t n = (uint16_t)~m;
    for (int i = 0; i < 4; i++) {
        uint16_t v = (uint16_t)(be16(img + src) & m);
        uint16_t d = be16(img + dest);
        wr16(img + dest, (uint16_t)((d & n) | v));
        src += 2;
        dest += 2;
    }
}

/* FUN_00012e2e @ 0x12e2e */
void reveal_region(uint8_t *img, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    x = (x << 2) & 0xffff;
    w = (w << 2) & 0xffff;
    wr16(img + A_REV_X0, (uint16_t)x);
    wr16(img + A_REV_Y0, (uint16_t)y);
    wr16(img + A_REV_W, (uint16_t)w);
    uint32_t total = (uint16_t)(w * h);         /* mulu.w w,h */
    wr16(img + A_REV_TOTAL, (uint16_t)total);

    uint32_t step = img[A_ANIM_1A577] & 0xff;
    uint32_t x0 = be16(img + A_REV_X0);
    uint32_t y0 = be16(img + A_REV_Y0);
    uint32_t width = be16(img + A_REV_W);
    uint32_t delay = be16(img + A_ANIM_1A56A);

    uint32_t acc = 0;
    for (;;) {
        uint32_t w = width ? width : 0x10000;      /* divu.w: cociente en word bajo */
        uint32_t row = acc / w, col = acc % w;
        uint32_t phase;
        uint32_t addr = screen_address_raw(img, (x0 + col) & 0xffff, (y0 + row) & 0xffff, &phase);
        reveal_dot(img, addr, 0x8000, phase);
        cpu_delay(reveal_gate(delay));           /* dbf de espera del original (KULT_REVEAL_GATE) */
        acc = (acc + step) & 0xffff;
        if (acc == total) break;
        if (acc > total) acc = (acc - total) & 0xffff;
    }
}


/* FUN_00012fb6 @ 0x12fb6: escribe 4 words en A0: para cada uno, AND con la máscara
 * invertida (rotada de 0x8000 por D7) y OR con la parte correspondiente de D0. Puro
 * bucle de píxeles. */
void masked_word4(uint8_t *img, uint32_t dest, uint32_t bits, uint32_t phase) {
    uint32_t mask = ror16(0x8000, phase);
    uint32_t inv = (~mask) & 0xffff;
    bits = ror16(bits, phase);
    for (int i = 0; i < 4; i++) {
        uint32_t v = (be16(img + dest) & inv) | (bits & mask);
        wr16(img + dest, v & 0xffff);
        dest += 2;
        bits = rol16(bits, 4);
    }
}

/* FUN_00012f28 @ 0x12f28: revelado pixel a pixel del elemento. Tras 128a4 reescala
 * x/ancho <<2, fija el total (alto*ancho) en 0x1a49e y recorre las posiciones en
 * pasos de 0x11 (mod total) pintando el bit (0x12fb6) con retardo entre pasos. */
uint32_t elem_command_wrap(uint8_t *, uint32_t);
uint32_t elem_blit_vbl(uint8_t *, uint32_t);

/* 0x12f28 — reveal pixelado del elemento (dbf 0x64 / skip $1a570) */
uint32_t reveal_pixel(uint8_t *img, uint32_t script) {
    if (img[A_SKIP_TRANSITION] != 0)
        return elem_blit_vbl(img, script);
    script = elem_command_wrap(img, script);
    uint32_t x = be16(img + A_ELEM_1A532);
    uint32_t w = be16(img + A_ELEM_1A52E);
    uint32_t h = be16(img + A_ELEM_1A52C);
    wr16(img + A_ELEM_1A532, (x << 2) & 0xffff);
    wr16(img + A_ELEM_1A52E, (w << 2) & 0xffff);
    uint32_t total = (h * ((w << 2) & 0xffff)) & 0xffff;
    wr16(img + A_DAT_1A49E, total);

    uint32_t src = be32(img + A_ELEM_1A528);
    uint32_t width = be16(img + A_ELEM_1A52E);
    uint32_t acc = 0;
    for (;;) {
        uint32_t bit = acc & 3;
        uint32_t off = (acc >> 1) & 0xfffffffeu;
        uint32_t div = width ? width : 0x10000;
        uint32_t row = acc / div, col = acc % div;
        uint32_t phase;
        uint32_t addr = screen_address_raw(img, (be16(img + A_ELEM_1A532) + col) & 0xffff,
                                          (be16(img + A_ELEM_1A530) + row) & 0xffff, &phase);
        uint32_t v = (be16(img + src + off) << bit) & 0xffff;
        masked_word4(img, addr, v, phase);
        cpu_delay(reveal_gate(0x64));            /* dbf 0x64 del original */
        acc = (acc + 0x11) & 0xffff;
        if (acc == total)
            break;
        if (acc > total)
            acc = (acc - total) & 0xffff;
    }
    return script;
}
