/* cursor.c — guardado/restauración del área bajo el cursor (16×16).
 *
 *   cursor_draw    @ 0x10526 : guarda el área 16×16 bajo el cursor y pinta el
 *                              sprite (con máscara y fase de píxel D7).
 *   cursor_restore @ 0x1060a : restaura el área guardada.
 *
 * Todo el efecto es la imagen. La dirección base la da screen_address_raw
 * (0x10fee), igual que en el original (`bsr 0x10fee`).
 */
#include <stdint.h>

#include "machine.h"
#include "addrs.h"
#include "kult_internal.h"

uint32_t screen_address_raw(uint8_t *, uint32_t, uint32_t, uint32_t *);

/* 0x10526 — guarda las 16 filas de 16 B bajo el cursor en A_CURSOR_SAVE_BUF+8 y
 * pinta el sprite. El 4º word de cada media fila se OR-ea solo si el flag
 * A_CURSOR_HOTSPOT (0x1a48a) es distinto de 0. */
void cursor_draw(uint8_t *img) {
    uint32_t sprite = be32(img + A_CURSOR_SPRITE_PTR);
    uint32_t save = A_CURSOR_SAVE_BUF + 8;
    uint32_t cx = be16(img + A_CURSOR_X), clx = be16(img + A_CLIP_X);
    uint32_t cy = be16(img + A_CURSOR_Y), cly = be16(img + A_CLIP_Y);
    uint32_t dx = (cx >= clx) ? (cx - clx) : 0;   /* sub.w + bcc/clr.w */
    uint32_t dy = (cy >= cly) ? (cy - cly) : 0;
    uint32_t phase;
    uint32_t dest = screen_address_raw(img, dx, dy, &phase);
    wr32(img + A_CURSOR_SAVE_DEST, dest);
    dest += 8;

    uint32_t src = sprite;
    for (int row = 0; row < 16; row++) {
        uint16_t w1 = be16(img + src); src += 2;
        uint16_t w2 = be16(img + src); src += 2;
        uint32_t m1 = ror32(((uint32_t)w1 << 16) | 0xffffu, phase);
        uint32_t m2 = ror32((uint32_t)w2 << 16, phase);
        for (int half = 0; half < 2; half++) {
            for (int i = 0; i < 4; i++) {
                uint32_t off = half ? (uint32_t)i * 2 - 8 : (uint32_t)i * 2;
                uint32_t ad = dest + off, sd = save + off;
                uint16_t v = be16(img + ad);
                wr16(img + sd, v);
                v = (uint16_t)(v & (m1 & 0xffff));
                if (!(i == 3 && img[A_CURSOR_HOTSPOT] == 0))
                    v = (uint16_t)(v | (m2 & 0xffff));
                wr16(img + ad, v);
            }
            m1 = (m1 << 16) | (m1 >> 16);
            m2 = (m2 << 16) | (m2 >> 16);
        }
        dest += 0xa0;
        save += 0x10;
    }
}

/* 0x1060a — restaura 16 filas de 16 B desde A_CURSOR_SAVE_BUF al destino
 * guardado en A_CURSOR_SAVE_DEST (stride 0xa0: 16 B + 0x90). */
void cursor_restore(uint8_t *img) {
    uint32_t dest = be32(img + A_CURSOR_SAVE_DEST);
    uint32_t src = A_CURSOR_SAVE_BUF;
    for (int row = 0; row < 16; row++) {
        for (int i = 0; i < 4; i++) {
            wr32(img + dest, be32(img + src));
            dest += 4;
            src += 4;
        }
        dest += 0x90;
    }
}
