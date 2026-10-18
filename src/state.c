/* state.c — helpers de estado pequeños (limpiezas de flags/globales).
 *
 *   clear_flag         @ 0x172ba : limpia el byte indirecto *(0x1a806) y la word 0x1acaa.
 *   clear_object_flags @ 0x15192 : limpia el bit 7 del byte 1 de 41 entradas de 0x1aa72.
 *   set_cursor_clip    @ 0x14a98 : fija el clip y el sprite de cursor desde tablas por D0.
 *   fix_sample_flag    @ 0x14bdc : cambia 0x81 -> 0x80 en las 0x3f entradas de 0x1a8f0.
 *
 * Todo el efecto es la imagen.
 */
#include <stdint.h>

#include "machine.h"
#include "addrs.h"

/* 0x172ba — limpia el byte apuntado por *(0x1a806) y la word 0x1acaa. El
 * oráculo descarta escrituras fuera de la imagen; el candidato indexa crudo, así
 * que acota con el tamaño de imagen (0x100000). */
void clear_flag(uint8_t *img) {
    uint32_t p = be32(img + A_PTR_1A806);
    if (p < 0x100000u)
        img[p] = 0;
    wr16(img + A_W_1ACAA, 0);
}

/* 0x15192 — limpia el bit 7 del byte 1 de cada una de las 41 entradas de 5 bytes
 * en 0x1aa72, y el flag 0x1acd3. */
void clear_object_flags(uint8_t *img) {
    uint32_t a0 = 0x1aa72;
    for (int i = 0; i < 0x29; i++) {              /* move.w #$28,d0; dbf -> 41 iteraciones */
        img[a0 + 1] &= 0x7f;
        a0 += 5;
    }
    img[0x1acd3] = 0;
}

/* 0x14a98 — fija el origen de recorte (0x19700/0x19702) y el puntero del sprite
 * de cursor (0x199ca) desde la tabla 0x196dc (stride 4) y 0x1949c (stride 64),
 * indexadas por D0. */
void set_cursor_clip(uint8_t *img, uint32_t a0, uint32_t d0) {
    (void)a0;
    uint32_t off = (d0 << 2) & 0xffffffffu;
    uint32_t p = 0x196dc + off;
    wr16(img + 0x19700, be16(img + p));
    wr16(img + 0x19702, be16(img + p + 2));
    wr32(img + 0x199ca, 0x1949c + ((off << 4) & 0xffffffffu));
}

/* 0x14bdc — recorre 0x3f entradas de 6 bytes en 0x1a8f0 y cambia 0x81 por 0x80. */
void fix_sample_flag(uint8_t *img) {
    uint32_t a0 = 0x1a8f0;
    for (int i = 0; i < 0x3f; i++) {
        if (img[a0] == 0x81)
            img[a0] = 0x80;
        a0 += 6;
    }
}
