/* palette.c — paleta del shifter (familia 0x135da / 0x162ce..0x16300).
 *
 * Estado (imagen plana + vistas documentadas; ver PLAN.md §4, opción a):
 *   A_PALETTE_PTR   (0x1a466, long) : puntero a la tabla de 16 words que ve el
 *                                     shifter; es el argumento de XBIOS Setpalette.
 *   A_PAL_TABLE     (0x1a10a)       : páginas de color, 5 words por entrada
 *                                     (A_PAL_TABLE + índice*0xa).
 *   A_PAL_INDEX_CUR (0x1acf4, byte) : índice de la paleta actual.
 *
 * Las direcciones con nombre viven en include/addrs.h; el acceso a memoria es
 * big-endian (be16/be32/wr16/wr32 de machine.h), como el 68000.
 *
 * XBIOS Setpalette (0x16300) no escribe imagen: su efecto está en el shifter,
 * así que la prueba diferencial lo comprueba por el ledger de eventos del kit
 * (`os_setpalette` -> OS_EVENT_SETPALETTE), que es lo que emite el oráculo.
 * 0x162e2 sí escribe (los 5 words) y además cae, sin `rts`, en 0x16300.
 *
 * Cadena real de control (cada entrada cae en la siguiente):
 *   0x162d8 -> 0x162da -> 0x162e2 -> 0x16300
 *   0x162ce -> 0x162e2 -> 0x16300
 *   0x135da -> 0x16300
 */
#include <stdint.h>

#include "machine.h"
#include "os.h"
#include "addrs.h"

/* 0x16300 — XBIOS Setpalette(*(A_PALETTE_PTR)): trap #14, función 6.
 * Efecto de hardware (off-image); el candidato lo registra por os_setpalette(). */
void xbios_setpalette(uint8_t *img) {
    os_setpalette(be32(img + A_PALETTE_PTR));
}

/* 0x135da — `bsr.w 0x16300; rts`: alias público del Setpalette. */
void set_palette(uint8_t *img) {
    xbios_setpalette(img);
}

/* 0x162e2 — fija una entrada de paleta: copia los 5 words de
 * A_PAL_TABLE + d0*0xa a *(A_PALETTE_PTR)+2 y, al no tener `rts`, cae en 0x16300
 * (aplica la paleta). Es la entrada que usan 0x162da y 0x162ce. */
void set_palette_entry(uint8_t *img, uint32_t d0) {
    uint32_t src = A_PAL_TABLE + (d0 & 0xffff) * 0xa;   /* mulu.w #$a,d0 */
    uint32_t dst = be32(img + A_PALETTE_PTR) + 2;
    for (uint32_t i = 0; i < 5; i++)
        wr16(img + dst + i * 2, be16(img + src + i * 2));
    xbios_setpalette(img);
}

/* 0x162da — d0 = byte A_PAL_INDEX_CUR (0x1acf4) y cae en 0x162e2. */
void set_palette_current(uint8_t *img) {
    set_palette_entry(img, img[A_PAL_INDEX_CUR] & 0xff);
}

/* 0x162ce — d0 = A6[1] (índice tomado del guion), avanza A6 +2 y cae en 0x162e2.
 * Devuelve el A6 resultante para poder comprobarlo como registro de salida. */
uint32_t set_palette_from_entry(uint8_t *img, uint32_t a6) {
    uint32_t index = img[a6 + 1] & 0xff;
    a6 += 2;
    set_palette_entry(img, index);
    return a6;
}

/* 0x162d8 — `addq.l #1,a6` y cae en 0x162da (paleta actual). Devuelve el A6
 * resultante (avanza el guion un byte). */
uint32_t set_palette_advance(uint8_t *img, uint32_t a6) {
    a6 += 1;
    set_palette_current(img);
    return a6;
}
