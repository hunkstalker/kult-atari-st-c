/* stubs.c — funciones que son un `rts` pelado (no-ops verificables).
 * 0x135e0, 0x160c0, 0x16dba, 0x19312. */
#include <stdint.h>

/* 0x135e0 — `rts` (stub sin efecto) */
void noop_135e0(uint8_t *img) { (void)img; }
/* 0x160c0 — `rts` (stub sin efecto) */
void noop_160c0(uint8_t *img) { (void)img; }
/* 0x16dba — `rts` (stub sin efecto) */
void noop_16dba(uint8_t *img) { (void)img; }
/* 0x19312 — `rts` (stub sin efecto; FDC fuera de alcance) */
void noop_19312(uint8_t *img) { (void)img; }
