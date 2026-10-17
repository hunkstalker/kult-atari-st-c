/* input.c — paquete IKBD y espera de frame.
 *
 *   ikbd_packet_copy @ 0x13686 : copia 5 bytes de A0 (paquete IKBD) a
 *                                A_IKBD_PACKET (0x1a44f..0x1a453).
 *   wait_frame       @ 0x13698 : espera el paquete del IKBD, fija los botones
 *                                (bit 2 = clic) y llama a GEMDOS Crawio(0xff).
 *
 * XBIOS Ikbdws(0x19) es no-op en el kit; la llegada del paquete la modela una
 * escritura programada (`schedule`) en el sitio del compare 0x136b0 y el sondeo
 * lo cuenta sched_poll8 (una vez por iteración), como la VBL de 0x13594.
 */
#include <stdint.h>

#include "machine.h"
#include "addrs.h"
#include "os.h"
#include "sched.h"

/* 0x13686 — copia el paquete IKBD de 5 bytes desde A0. */
void ikbd_packet_copy(uint8_t *img, uint32_t a0) {
    for (int i = 0; i < 5; i++)
        img[A_IKBD_PACKET + i] = img[a0 + i];
}

/* 0x13698 — wait_frame. */
void wait_frame(uint8_t *img) {
    img[A_IKBD_PACKET] = 0xff;                       /* move.b #$ff,$1a44f */
    /* XBIOS Ikbdws (trap #14/$19): no-op en el kit (ni imagen ni ledger). */
    while (sched_poll8(img, A_IKBD_PACKET, 0x136b0) == 0xff)
        ;                                            /* espera el paquete del IKBD */

    img[A_INPUT_BUTTONS] = img[A_IKBD_PACKET];       /* $1a44f -> $1a456 */
    uint32_t d0 = 0;
    if (img[A_INPUT_BUTTONS] & 0x01) {               /* btst #0 */
        img[A_INPUT_BUTTONS] |= 0x04;                /* clic */
        d0 = 0xff;
    }
    img[A_SKIP_TRANSITION] = (uint8_t)d0;

    if (os_crawio(img, 0x00ff) != 0)                 /* trap #1/$6: Crawio(0xff) */
        img[A_INPUT_BUTTONS] |= 0x04;                /* hubo movimiento -> clic */
}
