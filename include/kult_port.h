/* kult_port.h — ayudas del port que no son piezas del original.
 *
 * `cpu_delay`: sustituye los bucles `dbf` que solo consumen tiempo (reveal,
 * animacion, ...). En el diferencial es un no-op (lo que se compara es la
 * imagen, no el tiempo); el build nativo lo reemplaza para ceder frames.
 */
#ifndef KULT_PORT_H
#define KULT_PORT_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void wait_vbl(uint8_t *img);

/* KULT_FRAME_GATE: retardo (VBLs) por construccion de banco de frames. */
static inline void frame_gate_delay(uint8_t *img) {
    static int fg = -1;
    if (fg < 0) { const char *e = getenv("KULT_FRAME_GATE"); fg = e ? atoi(e) : 0; }
    for (int i = 0; i < fg; i++) wait_vbl(img);
}

static inline void cpu_delay(uint32_t units) { (void)units; }

/* Logger de diagnostico del motor nativo: no-op en el diferencial. */
static inline FILE *kult_dbgfile(void) { return NULL; }

#endif
