/* kult_internal.h — helpers internos compartidos por los módulos del core.
 *
 * Rotaciones de 32/16 bits y transferencias `movep` (bytes alternos), que la
 * importación dejó duplicadas en varios módulos. Todas son `static inline`
 * (no exportan símbolo).
 */
#ifndef KULT_INTERNAL_H
#define KULT_INTERNAL_H

#include <stdint.h>

/* Rotaciones (el 68000 toma el count mod 32/16). */
static inline uint32_t ror32(uint32_t v, uint32_t n) {
    n &= 31;
    return n ? ((v >> n) | (v << (32 - n))) : v;
}
static inline uint32_t rol32(uint32_t v, uint32_t n) {
    n &= 31;
    return n ? ((v << n) | (v >> (32 - n))) : v;
}
static inline uint32_t ror16(uint32_t v, uint32_t n) {
    n &= 15;
    return n ? (((v >> n) | (v << (16 - n))) & 0xffff) : (v & 0xffff);
}
static inline uint32_t rol16(uint32_t v, uint32_t n) {
    n &= 15;
    return n ? (((v << n) | (v >> (16 - n))) & 0xffff) : (v & 0xffff);
}
static inline uint32_t swap_words(uint32_t v) {
    return (v << 16) | (v >> 16);
}

/* `movep`: bytes alternos (byte alto en +0, bajo en +2, ...). */
static inline uint32_t movep_read_l(const uint8_t *img, uint32_t a) {
    return ((uint32_t)img[a] << 24) | ((uint32_t)img[a + 2] << 16) |
           ((uint32_t)img[a + 4] << 8) | img[a + 6];
}
static inline void movep_write_l(uint8_t *img, uint32_t a, uint32_t v) {
    img[a] = (v >> 24) & 0xff;
    img[a + 2] = (v >> 16) & 0xff;
    img[a + 4] = (v >> 8) & 0xff;
    img[a + 6] = v & 0xff;
}
static inline void movep_write_w(uint8_t *img, uint32_t a, uint32_t v) {
    img[a] = (v >> 8) & 0xff;
    img[a + 2] = v & 0xff;
}

/* Lectura de 4 bytes CONSECUTIVOS (la del scratch 0x55b02 leído como `move.l`). */
static inline uint32_t read_long(const uint8_t *img, uint32_t a) {
    return ((uint32_t)img[a] << 24) | ((uint32_t)img[a + 1] << 16) |
           ((uint32_t)img[a + 2] << 8) | img[a + 3];
}

#endif /* KULT_INTERNAL_H */
