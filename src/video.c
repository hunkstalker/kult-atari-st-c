

/* ===== funciones restantes importadas de video.c (viejo) ===== */
/* video.c — the palette setter.
 *
 * FUN_00016300 @ 0x16300: push *(0x1a466) and call XBIOS Setpalette ($6). The effect is
 * off-image (the shifter), so it is verified by the ordered OS-event ledger: the candidate
 * goes through the kit's os_setpalette() door, which records the same event the oracle's
 * trap does.
 */
#include <stdint.h>

#include "machine.h"
#include "os.h"
#include "sched.h"
#include "addrs.h"



/* FUN_00013594 @ 0x13594: espera de VBL. Lee el contador $1acd4 y gira hasta que
 * cambie. El cambio lo produce la VBL (fuera de la imagen); en el diferencial lo
 * modela una escritura programada en el sitio de espera 0x1359a (`schedule`), y el
 * sondeo cuenta una llegada por iteración (`sched_poll8`). */


/* FUN_000162e2 @ 0x162e2: copy the D0'th 5-word entry of the palette table at
 * 0x1a10a into *(0x1a466)+2 and issue XBIOS Setpalette(*(0x1a466)).
 * FUN_000162da @ 0x162da seeds D0 from the byte at 0x1acf4; FUN_000162ce @ 0x162ce
 * seeds it from A6[1]. */






/* FUN_00016210 @ 0x16210: bump the byte at 0x1a579, index the byte table at 0x2741e
 * by the new value, and store that byte into 0x1acd7. Devuelve el byte en D0 (lo usan
 * 0x17a1a y compania); los llamantes que no lo necesitan lo ignoran. */
uint32_t pal_next_byte(uint8_t *img) {
    uint32_t i = (uint8_t)(img[A_DAT_1A579] + 1);
    img[A_DAT_1A579] = (uint8_t)i;
    uint32_t v = img[A_TBL_2741E + i];
    img[A_DAT_1ACD7] = (uint8_t)v;
    return v;
}

/* FUN_00016230 @ 0x16230: same bump, but read a big-endian word from 0x2741e[index]
 * and return it in D0. */
uint32_t pal_next_word(uint8_t *img) {
    uint32_t i = (uint8_t)(img[A_DAT_1A579] + 1);
    img[A_DAT_1A579] = (uint8_t)i;
    uint32_t a0 = A_TBL_2741E + i;
    return ((uint32_t)img[a0] << 8) | img[a0 + 1];
}

/* FUN_0001627a @ 0x1627a: only when the low byte of (0x1ab60 - 0x1a526) reaches
 * 0xb4, latch 0x1a526 = 0x1ab60 and clear bit 6 of each of the 63 six-byte entries
 * at 0x1a8f0 whose byte0 has bit 6 set, byte1 != 0x1acd2 and byte1 < 0x34. */
void pal_threshold(uint8_t *img) {
    uint32_t d0 = (be16(img + A_DAT_1AB60) - be16(img + A_DAT_1A526)) & 0xffff;
    if ((d0 & 0xff) < 0xb4)
        return;
    wr16(img + A_DAT_1A526, be16(img + A_DAT_1AB60));
    uint32_t a0 = A_LIST_1A8F0;
    uint8_t d1 = img[A_DAT_1ACD2];
    for (int i = 0; i < 0x3f; i++) {
        uint8_t b0 = img[a0];
        if ((b0 & 0x40) && img[a0 + 1] != d1 && img[a0 + 1] < 0x34)
            img[a0] = (uint8_t)(b0 & 0xbf);
        a0 += 6;
    }
}

/* FUN_000162d8 @ 0x162d8: `addq.l #1,a6` y cae en 0x162da (paleta por 0x1acf4). */
void set_palette_current(uint8_t *img);


/* FUN_00016d1e @ 0x16d1e: espera de VBL (contador 0x1acd4): gira hasta que cambie y
 * luego hasta que el nuevo valor sea par (bit 0 == 0). Modelada con sched_poll8 en el
 * compare 0x16d24. */


/* FUN_00016310 @ 0x16310: ciclo de paleta. Copia los 16 words de la paleta actual
 * (*(0x1a466)) a 0xd8f0, gira los 5 colores del buffer (cada componente R/G/B
 * incrementa con tope 7/0x70/0x700) hasta que una pasada no cambie (VBL + Setpalette
 * por pasada), despacha $1abfa, y luego acerca los 5 colores al target
 * 0x1a10a[$1acf4] (decrementando componentes, VBL + Setpalette por pasada) hasta
 * igualarlo. */
uint32_t advance_text_entry(uint8_t *img, uint32_t a1, uint32_t d4);
void sound_play_b(uint8_t *img, uint32_t d4);
void set_palette(uint8_t *img);
void wait_vbl(uint8_t *img);
void run_command_keep_sp(uint8_t *img, uint32_t a6);

/* 0x16310 — (cmd idx 9): umbral 2, ciclo de paleta 0x16310 y flag $1ad0f. */
void palette_cycle(uint8_t *img) {
    sound_play_b(img, 0x17);
    {
        uint32_t a0 = be32(img + A_PALETTE_PTR);
        uint32_t a1 = 0xd8f0;
        for (int i = 0; i < 0x10; i++) {
            wr16(img + a1, be16(img + a0));
            a0 += 2;
            a1 += 2;
        }
    }
    for (;;) {                                   /* fase 1: girar hasta el wrap */
        uint32_t a0 = be32(img + A_PALETTE_PTR) + 2;
        uint32_t d5 = 0;
        for (int i = 0; i <= 4; i++) {
            uint32_t d0 = be16(img + a0);
            if (d0 != 0x777) {
                d5 = 0xffff;
                uint32_t r = d0 & 0xf;   if (r != 7)     r++;
                uint32_t g = d0 & 0xf0;  if (g != 0x70)  g += 0x10;
                uint32_t b = d0 & 0xf00; if (b != 0x700) b += 0x100;
                d0 = (r | g | b) & 0xffff;
            }
            wr16(img + a0, d0);
            a0 += 2;
        }
        if (d5 == 0)
            break;
        wait_vbl(img);
        {
            uint32_t d0b = 0xffff;
            while (d0b != 0xffff)
                d0b = (d0b - 1) & 0xffff;
        }
        set_palette(img);
    }
    sound_play_b(img, 0x17);
    wr16(img + A_THE_COMMAND, be16(img + A_DAT_1ABFA));
    run_command_keep_sp(img, 0);
    for (;;) {                                   /* fase 2: acercar al target */
        uint32_t a0 = A_PAL_TABLE + ((uint32_t)img[A_PAL_INDEX_CUR] * 0xa);
        uint32_t a1 = 0xd8f2;
        for (int i = 0; i <= 4; i++) {
            wr16(img + a1, be16(img + a0));
            a0 += 2;
            a1 += 2;
        }
        a0 = be32(img + A_PALETTE_PTR) + 2;
        a1 = 0xd8f2;
        uint32_t d5 = 0;
        for (int i = 0; i <= 4; i++) {
            uint32_t d0 = be16(img + a0);
            uint32_t d1 = d0, d2 = d0;
            uint32_t d4 = be16(img + a1);
            if (d0 != d4) {
                d5 = 0xffff;
                uint32_t r = d0 & 0xf;   if (r != (d4 & 0xf))     r = (r - 1) & 0xffff;
                uint32_t g = d1 & 0xf0;  if (g != (d4 & 0xf0))    g = (g - 0x10) & 0xffff;
                uint32_t b = d2 & 0xf00; if (b != (d4 & 0xf00))   b = (b - 0x100) & 0xffff;
                d0 = (r | g | b) & 0xffff;
            }
            wr16(img + a0, d0);
            a0 += 2;
            a1 += 2;
        }
        if (d5 == 0)
            return;
        wait_vbl(img);
        set_palette(img);
    }
}

/* FUN_000135da @ 0x135da: `bsr 0x16300` + rts (alias de set_palette). */


/* FUN_000135e2 @ 0x135e2 (crt0_init): arranque. XBIOS Getrez -> $1a558 (low-res),
 * Physbase -> $1a45e/$1a462(-0x8000)/$1a466(-0x20), GEMDOS Super(0) -> guarda el
 * vector $456 en $1a4d4 y lo apunta a 0x160c2, $502 = 0x1019a, $1a579 = $4bd,
 * Super(restore), XBIOS Kbdvbase -> $1a446 (handler mouse a 0x13686 guardando el
 * anterior en $1a44a) e Ikbdws(0x19) (sin efecto en imagen). */

void crt0_init(uint8_t *img) {
    wr16(img + 0x1a558, 0);                       /* XBIOS Getrez: low-res */
    uint32_t d0 = OS_SCREEN_BASE;                 /* XBIOS Physbase */
    wr32(img + 0x1a45e, d0);
    d0 = (d0 - 0x8000) & 0xffffffff;
    wr32(img + 0x1a462, d0);
    d0 = (d0 - 0x20) & 0xffffffff;
    wr32(img + 0x1a466, d0);
    uint32_t ss = 0;
    os_super(OS_SUPER_ENTER, &ss);                /* GEMDOS Super(0) */
    uint32_t a0 = be32(img + 0x456);
    wr32(img + 0x1a4d4, be32(img + a0));
    wr32(img + a0, 0x160c2);
    wr32(img + 0x502, 0x1019a);
    img[0x1a579] = img[0x4bd];
    os_super(ss, &d0);                            /* Super(token) */
    a0 = (OS_KBDVBASE + 0x10) & 0xffffffff;       /* XBIOS Kbdvbase + 0x10 */
    wr32(img + 0x1a446, a0);
    wr32(img + 0x1a44a, be32(img + a0));
    wr32(img + a0, 0x13686);
    /* XBIOS Ikbdws(0x19, 0x1a459, 4): el modelo no lo registra (no hay efecto en
     * imagen), asi que el candidato no emite nada. */
}
