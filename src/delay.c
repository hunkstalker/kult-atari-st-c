/* delay.c — esperas de VBL y retardo por ciclos del original.
 *
 *   wait_vbl      @ 0x13594 : gira hasta que cambie el contador de VBL 0x1acd4.
 *   wait_vbl_even @ 0x16d1e : gira hasta que cambie y el nuevo valor sea par.
 *
 * El contador lo incrementa el handler de VBL (fuera de la imagen), así que la
 * liberación se declara con el modelo de escrituras programadas del kit
 * (`schedule`/`wait_sites`) y el sondeo se cuenta con sched_poll8 una vez por
 * iteración (TRAP_MODEL.md Fase 8).
 */
#include <stdint.h>

#include "addrs.h"
#include "sched.h"

/* 0x13594 — espera de VBL. (El original deja D0 con el valor anterior del
 * contador; ningún llamante lo consume, así que el out-param no se modela.) */
void wait_vbl(uint8_t *img) {
    uint8_t prev = img[A_DAT_1ACD4];
    while (sched_poll8(img, A_DAT_1ACD4, 0x1359a) == prev)
        ;
}

/* 0x16d1e — espera de VBL + contador par (paso de los wipes de texto). */
void wait_vbl_even(uint8_t *img) {
    for (;;) {
        uint8_t prev = img[A_DAT_1ACD4];
        while (sched_poll8(img, A_DAT_1ACD4, 0x16d24) == prev)
            ;
        if (!(img[A_DAT_1ACD4] & 1))
            break;
    }
}
