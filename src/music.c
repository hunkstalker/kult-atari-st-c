/* music.c — handlers del motor de musica del PSG (0x3aa38/0x3aa8a/0x3aa7c/0x3a85e).
 *
 * Empezando por las hojas: 0x3aa7c es el write-verify del protocolo directo del chip
 * (selecciona D6, escribe D0, relee $ff8800 hasta que devuelve D0); 0x3aa38 es el
 * handler del `trap #3` (vector 0x23 = 0x8c: lo instala init_hardware con Setexc) y
 * 0x3aa8a su gemelo que escribe la tabla sombra en la imagen ($3ae7c). 0x3a85e es el
 * silenciador que emite tres `trap #3` (registros 8/9/10 a 0).
 *
 * La semantica del trap es del CPU (TRAP #n -> vector n+32) y el handler vive en la
 * imagen: el candidato no tiene traps, asi que llama a la reconstruccion del handler
 * (psg_trap3), exactamente como el oraculo llega a el via vector.
 *
 * Los accesos PSG van por psg_port_write/psg_port_read (ledger + fichero del kit). El
 * read-back del write-verify sale de lo recien escrito, asi que el bucle siempre da una
 * pasada; el ledger del oraculo registra WRITE+READ por acceso y el candidato igual.
 */
#include <stdint.h>

#include "machine.h"
#include "os.h"
#include "psg.h"
#include "hw.h"

#define MUSIC_SHADOW 0x3ae7c
#define MUSIC_STATE  0x3ae8c

void psg_trap3(uint8_t *img, uint32_t d0, uint32_t d2, uint32_t d7,
                 uint32_t *d0_out);

/* FUN_0003aa7c @ 0x3aa7c: escribe el registro D6 con D0 y verifica releyendo $ff8800.
 * En el modelo el chip devuelve lo escrito, asi que la verificacion pasa a la primera;
 * aun asi el READ es un evento del ledger y hay que emitirlo. */
void psg_write_verify(uint8_t *img, uint32_t d6, uint32_t d0) {
    (void)img;
    psg_port_write(d6 & 0xff, (uint8_t)d0);
    (void)psg_port_read(d6 & 0xff);
}

/* FUN_0003aa38 @ 0x3aa38: handler del `trap #3`. D2 es el registro base (bit 15: modo
 * par relativo, D6 = 2*D7 + D2), D7 el desplazamiento y D0 el valor. Con D2==7 hace el
 * RMW del mixer: lee el registro, conserva D0[0..5] y los bits 6-7 leidos.
 *
 * D0 se restaura al final (el original lo salva en la pila); D5/D6/A5 son scratch. */
void psg_trap3(uint8_t *img, uint32_t d0, uint32_t d2, uint32_t d7,
                 uint32_t *d0_out) {
    uint32_t d0_in = d0;
    uint32_t d6;
    if (d2 & 0x8000) {
        d6 = d7;
        d6 = (d6 & 0xffff0000u) | ((d6 << 1) & 0xffffu);          /* lsl.w #1,d6 */
        d6 = (d6 & 0xffffff00u) | ((d6 + (d2 & 0xffu)) & 0xffu);  /* add.b d2,d6 */
        psg_write_verify(img, d6, d0);                                 /* low byte */
        d0 = (d0 & 0xffff0000u) | (((d0 & 0xffffu) >> 8) & 0xffffu); /* lsr.w #8,d0 */
        d6 = (d6 & 0xffff0000u) | ((d6 + 1) & 0xffffu);           /* addq.w #1,d6 */
        psg_write_verify(img, d6, d0);                                 /* high byte */
    } else {
        d6 = d2;
        d6 = (d6 & 0xffffff00u) | ((d6 + (d7 & 0xffu)) & 0xffu);  /* add.b d7,d6 */
        if ((d2 & 0xffu) == 7) {
            uint8_t old = psg_port_read(d6 & 0xff);
            d0 = (d0 & 0xffffff00u)
               | (((d0 & 0x3fu) | (old & 0xc0u)) & 0xffu);
        }
        psg_write_verify(img, d6, d0);
    }
    if (d0_out) *d0_out = d0_in;
}

/* FUN_0003aa8a @ 0x3aa8a: gemelo del handler que escribe la tabla sombra $3ae7c en vez
 * del chip. Modo par relativo: word de D0 en 0(a5,d6.w); si no, byte. */
void psg_shadow(uint8_t *img, uint32_t d0, uint32_t d2, uint32_t d7,
                 uint32_t *d0_out) {
    uint32_t d6;
    if (d2 & 0x8000) {
        d6 = d7;
        d6 = (d6 & 0xffff0000u) | ((d6 << 1) & 0xffffu);
        d6 = (d6 & 0xffffff00u) | ((d6 + (d2 & 0xffu)) & 0xffu);
        wr16(img + MUSIC_SHADOW + (uint32_t)(int32_t)(int16_t)(d6 & 0xffffu),
             (uint16_t)d0);
    } else {
        d6 = d2;
        d6 = (d6 & 0xffffff00u) | ((d6 + (d7 & 0xffu)) & 0xffu);
        img[MUSIC_SHADOW + (uint32_t)(int32_t)(int16_t)(d6 & 0xffffu)] = (uint8_t)d0;
    }
    if (d0_out) *d0_out = d0;
}

/* FUN_0003a85e @ 0x3a85e: silencia los tres canales emitiendo `trap #3` con D0=0 y
 * D2=8 (registros de volumen 8/9/10), D7 de 0 a 2. El original fija D2 con `move.w`
 * (conserva la mitad alta) y D0/D7 con `moveq` (32 bits). */
void music_silence(uint8_t *img, uint32_t d2_in, uint32_t *d0_out, uint32_t *d2_out,
                 uint32_t *d7_out) {
    uint32_t d0 = 0, d2 = (d2_in & 0xffff0000u) | 8u, d7 = 0;
    for (int i = 0; i < 3; i++) {
        psg_trap3(img, d0, d2, d7, 0);
        if (i != 2)
            d7 = (d7 + 1) & 0xffffffffu;   /* addq.w #1,d7 entre traps */
    }
    if (d0_out) *d0_out = d0;
    if (d2_out) *d2_out = d2;
    if (d7_out) *d7_out = d7;
}

/* ================= init/MFP y despacho de comandos (0x3a556..0x3a734) =================
 *
 * 0x3a556 init_hardware -> 0x3a5c2 (Supexec(mfp_setup) + Setexc de los traps #3/#4) ->
 * 0x3a62e (carga la cancion: cabecera en 0x3b1ac) -> 0x3a734 (comandos de canal).
 * 0x3a6f4/0x3a712 paran el Timer A y silencian. 0x3a57c despacha D1 por la tabla de
 * 0x3afa8: sus tres handlers (0x3a59c/0x3a5aa/0x3a5b8) son codelets fuera del indice,
 * asi que se transcriben aqui; devuelve C=0 si encontro, C=1 si no (el llamante usa
 * `bcc`). Los Setexc del candidato son escrituras normales en la tabla de vectores. */

/* FUN_0003a604 @ 0x3a604: mfp_setup. Habilita Timer A (IERA/IMRA bit 5), lo pone en
 * delay con /? (TACR=4) y recarga TADR=0xAA; instala el reproductor 0x3a872 en el
 * vector $134 (Timer A con VR=0x40 de TOS). */
void mfp_setup(uint8_t *img) {
    hw_write8(0xfffa13, 0x20);
    hw_write8(0xfffa07, 0x20);
    hw_write8(0xfffa19, 0x04);
    hw_write8(0xfffa1f, 0xaa);
    wr32(img + 0x134, 0x3a872);
}

/* FUN_0003a5c2 @ 0x3a5c2: Supexec(0x3a604) y Setexc(0x23,0x3aa38)/Setexc(0x24,0x3aab6).
 * Devuelve en D0 el handler anterior del ultimo vector (0x90). */
uint32_t music_install(uint8_t *img, uint32_t *d0_out) {
    mfp_setup(img);
    wr32(img + 0x8c, 0x3aa38);
    uint32_t old = be32(img + 0x90);
    wr32(img + 0x90, 0x3aab6);
    if (d0_out) *d0_out = old;
    return old;
}

/* Codelets del despacho (no estan en el indice de 406): cada uno fija un parametro del
 * estado global y sale con C=0. */
static void disp_3a59c(uint8_t *img, uint32_t a6, uint32_t d5) {
    img[a6 + 0x1c] = (uint8_t)d5;
    img[a6 + 0x1b] = (uint8_t)d5;
}
static void disp_3a5aa(uint8_t *img, uint32_t a6, uint32_t d5) {
    img[a6 + 0x1d] = (uint8_t)(d5 & 0x0fu);
}
static void disp_3a5b8(uint8_t *img, uint32_t a6, uint32_t d5) {
    img[a6 + 0x1e] = (uint8_t)d5;
}

/* FUN_0003a57c @ 0x3a57c: recorre la tabla A0 ({word clave, long handler}, fin en 0) y
 * salta al handler cuya clave es D1. Devuelve el carry resultante: 0 encontrado, 1
 * agotada la tabla (`ori.b #1,ccr`). */
uint32_t music_cmd_table(uint8_t *img, uint32_t a0, uint32_t d1, uint32_t d5,
                     uint32_t a6) {
    for (;;) {
        uint32_t d3 = be16(img + a0);
        a0 += 2;
        if (d3 == 0)
            return 1;
        if (d3 == (d1 & 0xffffu)) {
            uint32_t handler = be32(img + a0);
            if (handler == 0x3a59c)
                disp_3a59c(img, a6, d5);
            else if (handler == 0x3a5aa)
                disp_3a5aa(img, a6, d5);
            else if (handler == 0x3a5b8)
                disp_3a5b8(img, a6, d5);
            return 0;
        }
        a0 += 4;
    }
}

/* FUN_0003a62e @ 0x3a62e: carga una cancion. `selector` indexa la cabecera `base`
 * (0x3b1ac): word de offset al bloque; de ahi salen la duracion, las tablas del estado
 * (4/8/0xc/0x10), el puntero de cancion (canal[0]) y los flags/volumenes iniciales.
 * Termina silenciando (0x3a85e) y limpiando el contador de estado. D2 entra sin tocar. */
void music_load(uint8_t *img, uint32_t selector, uint32_t base, uint32_t d2) {
    uint32_t a6 = MUSIC_STATE;
    uint32_t d0 = selector & 0xffffu;
    img[a6 + 0x1a] = 0;
    d0 = be16(img + base + ((d0 << 1) & 0xffffu));
    uint32_t a0 = base + (uint32_t)(int32_t)(int16_t)(d0 & 0xffffu);
    wr32(img + a6 + 0xc, a0);
    uint32_t a1 = a0;
    d0 = be16(img + a0); a0 += 2;
    img[a6 + 0x1b] = (uint8_t)d0;
    img[a6 + 0x1c] = (uint8_t)d0;
    d0 = be16(img + a0); a0 += 2;
    wr32(img + a6 + 4, a1 + (uint32_t)(int32_t)(int16_t)(d0 & 0xffffu));
    d0 = be16(img + a0); a0 += 2;
    wr32(img + a6 + 8, a1 + (uint32_t)(int32_t)(int16_t)(d0 & 0xffffu));
    d0 = be16(img + a0); a0 += 2;
    wr32(img + 0x3afbc, a1 + (uint32_t)(int32_t)(int16_t)(d0 & 0xffffu));
    d0 = be16(img + a0); a0 += 2;
    wr32(img + 0x3b00c, a1 + (uint32_t)(int32_t)(int16_t)(d0 & 0xffffu));
    d0 = be16(img + a0); a0 += 2;
    wr32(img + 0x3b05c, a1 + (uint32_t)(int32_t)(int16_t)(d0 & 0xffffu));
    wr32(img + a6 + 0x10, a0);
    img[a6 + 0x1d] = 0x0f;
    img[0x3b1a2] = 0x3f;
    img[0x3b1aa] = 0x3f;
    for (uint32_t ch = 0; ch < 3; ch++) {
        uint32_t a2 = 0x3afbc + ch * 0x50;
        wr16(img + a2 + 0x26, (uint16_t)(be16(img + a2 + 0x26) | 0x0040u));
        img[a2 + 0x2b] = 0x0f;
        img[a2 + 0x2f] = 0x0f;
    }
    music_silence(img, d2, 0, 0, 0);
    wr16(img + a6 + 0x18, 0);
}

/* FUN_0003a734 @ 0x3a734: procesa el comando D1 (word) con valor D5 (word) sobre los
 * canales que seleccionan D1[0..2] (si son 0, los tres). Devuelve D0 = 0xffff o el
 * minimo 0x24 visto. Antes despacha D1 a 0x3a57c (comandos 0x400/0x800/0x1000). */
uint32_t music_cmd(uint8_t *img, uint32_t d5, uint32_t d1) {
    uint32_t d0 = 0xffff;
    d5 &= 0xffffu;
    d1 &= 0xffffu;
    if (music_cmd_table(img, 0x3afa8, d1, d5, MUSIC_STATE) == 0)
        return d0;
    uint32_t d2 = d1 & 0x0fu;
    if (d2 == 0)
        d2 = 0x0fu;
    uint32_t a0 = 0x3afbc;
    uint32_t d7 = 0;
    for (;;) {
        uint32_t bit = d2 & 1u;
        d2 >>= 1;
        if (bit) {
            if (d1 & 0x8000u)
                img[a0 + 0x2f] = (uint8_t)(d5 & 0x0fu);
            if (d1 & 0x2000u) {
                img[a0 + 0x2b] = (uint8_t)(d5 & 0x0fu);
                img[a0 + 0x2f] = (uint8_t)(d5 & 0x0fu);
            }
            if (d1 & 0x0100u) {
                img[a0 + 0x2b] = 0;
                img[a0 + 0x2f] = img[MUSIC_STATE + 0x1d];
            }
            if (d1 & 0x0200u)
                img[a0 + 0x2f] = 0;
            if (d1 & 0x0040u) {
                uint32_t d4 = img[0x3aec0 + d7];
                d4 = (uint8_t)((d4 << 3) | d4);
                img[0x3b1a2] |= (uint8_t)d4;
                wr16(img + a0 + 0x26, (uint16_t)(be16(img + a0 + 0x26) | 0x0040u));
            }
            if (d1 & 0x0080u) {
                wr16(img + a0 + 0x26, 0);
                wr16(img + a0 + 0x26, (uint16_t)(be16(img + a0 + 0x26) | 0x0080u));
                wr16(img + a0 + 0x26, (uint16_t)(be16(img + a0 + 0x26) | 0x0001u));
                img[MUSIC_STATE + 0x1a] = 0xff;
                uint32_t d4 = img[0x3aec0 + d7];   /* solo `t`: sin lsl */
                img[0x3b1a2] &= (uint8_t)~d4;
            }
            if (d1 & 0x0020u)
                wr16(img + a0 + 0x26, (uint16_t)(be16(img + a0 + 0x26) | 0x0020u));
            if (d1 & 0x0010u) {
                if (!(img[a0 + 0x27] & 0x40u)) {
                    uint32_t d6 = be16(img + a0 + 0x24);
                    if (d0 >= d6)
                        d0 = d6;
                }
            }
        }
        a0 += 0x50;
        d7 += 1;
        if (d7 == 3)
            break;
    }
    return d0;
}

/* FUN_0003a556 @ 0x3a556: init_hardware. D2 entra sin tocar y llega hasta 0x3a85e. */
uint32_t init_hardware(uint8_t *img, uint32_t d2) {
    music_install(img, 0);
    music_load(img, 0, 0x3b1ac, d2);
    return music_cmd(img, 0, 0x00a7);
}

/* FUN_0003a712 @ 0x3a712: para el Timer A (TACR=0) y deshabilita su interrupcion. El
 * `tst.b $fffa19` del original espera a que el chip tome el 0; el modelo lo sirve 0 en
 * la primera lectura, asi que no se transcribe. */
void music_stop2(uint8_t *img) {
    (void)img;
    hw_write8(0xfffa19, 0);
    hw_write8(0xfffa13, 0);
    hw_write8(0xfffa07, 0);
}

/* FUN_0003a6f4 @ 0x3a6f4: Supexec(0x3a712) + silenciar. D2 entra sin tocar. */
void music_stop(uint8_t *img, uint32_t d2) {
    music_stop2(img);
    music_silence(img, d2, 0, 0, 0);
}

/* ================= generador de notas y tick del tracker (0x3a992/0x3aab6) ============ */

/* FUN_0003a992 @ 0x3a992: un paso del generador de envolvente de la voz A0. Avanza el
 * indice del instrumento (A0[0xa]), acumula la modulacion (A0[0] += byte con signo) y el
 * acumulador A0[2]; devuelve el periodo ajustado (D0 - A0[0]) con suelo en 0. Los flags
 * A0[0xb]: bit0 = aplica modulacion, bit2 = ciclo terminado. */
uint32_t music_note(uint8_t *img, uint32_t a0, uint32_t d0) {
    uint32_t d3 = img[a0 + 0xa];
    uint32_t a1 = be32(img + a0 + 4) + (d3 & 0xffffu);
    uint8_t f = img[a0 + 0xb];
    img[a0 + 0xb] = (uint8_t)(f & ~0x01u);
    if (f & 0x01u) {
        d3 = (uint32_t)(int32_t)(int8_t)img[a1 + 1];
        wr16(img + a0 + 2, (uint16_t)(be16(img + a0 + 2) + d3));
    }
    d0 = (d0 + be16(img + a0 + 2)) & 0xffffu;
    d0 = (d0 - be16(img + a0)) & 0xffffu;
    if (d0 & 0x8000u)
        d0 = 0;
    if (img[a0 + 0xb] & 0x04u)
        return d0;
    img[a0 + 0xc] = (uint8_t)(img[a0 + 0xc] + 1);
    if (img[a0 + 0xc] != img[a1 + 2])
        return d0;
    img[a0 + 0xd] = (uint8_t)(img[a0 + 0xd] + 1);
    img[a0 + 0xc] = 0;
    if (img[a0 + 0xd] != img[a1]) {
        img[a0 + 0xb] |= 0x01u;
        return d0;
    }
    img[a0 + 0xd] = 0;
    d3 = (uint8_t)(img[a0 + 0xa] + 3);
    if (d3 == 0x0c) {
        if (img[a0 + 0xb] == 0) {
            img[a0 + 0xb] |= 0x04u;
            return d0;
        }
        img[a0 + 0xd] = 0;
        d3 = (uint32_t)(int32_t)(int8_t)img[a0 + 8];
        wr16(img + a0, (uint16_t)(be16(img + a0) + d3));
        d3 = 3;
    }
    img[a0 + 0xa] = (uint8_t)d3;
    img[a0 + 0xb] |= 0x01u;
    return d0;
}

/* Handlers de comando del tick (tabla 0x3aeac). Los cuatro primeros continuan en
 * 0x3ab3a (por eso el tick los llama y salta); 0x3ae68 termina con rts. */

/* cmd 0 (0x3ad5a): transposicion desde el stream. */
void music_h1(uint8_t *img, uint32_t a3) {
    uint32_t a0 = be32(img + a3 + 8);
    img[a3 + 0x2a] = img[a0];
    wr32(img + a3 + 8, a0 + 1);
}

/* cmd 2 (0x3ad6a): cambia el instrumento de la voz 2 (channel[0x14]) con la tabla del
 * estado [8]; flag 0. */
void music_h2(uint8_t *img, uint32_t a3, uint32_t a6) {
    uint32_t a0 = be32(img + a3 + 8);
    uint32_t d0 = img[a0];
    a0 += 1;
    wr32(img + a3 + 8, a0);
    a0 = be32(img + a6 + 8) + (((d0 & 0xffu) * 13u) & 0xffffu);
    uint32_t a2 = be32(img + a3 + 0x14);
    img[a2 + 0xb] = 0;
    img[a2 + 8] = 0;
    img[a2 + 0xa] = 0;
    wr16(img + a2 + 2, 0);
    wr16(img + a2 + 0, 0);
    img[a2 + 0xd] = 0;
    img[a2 + 0xc] = 0;
    img[a2 + 0xb] |= 0x01u;
    wr32(img + a2 + 4, a0 + 1);
}

/* cmd 1 (0x3adb4): como el anterior pero con el flag 0x80 (loop) en la voz 2. */
void music_h3(uint8_t *img, uint32_t a3, uint32_t a6) {
    uint32_t a0 = be32(img + a3 + 8);
    uint32_t d0 = img[a0];
    a0 += 1;
    wr32(img + a3 + 8, a0);
    a0 = be32(img + a6 + 8) + (((d0 & 0xffu) * 13u) & 0xffffu);
    uint32_t a2 = be32(img + a3 + 0x14);
    d0 = img[a0];
    a0 += 1;
    img[a2 + 0xb] = 0x80;
    img[a2 + 8] = 0;
    img[a2 + 0xa] = 0;
    wr16(img + a2 + 2, 0);
    wr16(img + a2 + 0, 0);
    img[a2 + 0xd] = 0;
    img[a2 + 0xc] = 0;
    img[a2 + 0xb] |= 0x01u;
    wr32(img + a2 + 4, a0);
}

/* cmd 3 (0x3ae00): envolvente/volumen de la voz (channel[0x18]) a partir de dos bytes;
 * limpia el bit de canal en A0[6] via la tabla de bits. */
void music_h4(uint8_t *img, uint32_t a3, uint32_t d7) {
    uint32_t a0 = be32(img + a3 + 8);
    uint32_t d0 = img[a0];
    uint32_t d1 = img[a0 + 1];
    wr32(img + a3 + 8, a0 + 2);
    a0 = be32(img + a3 + 0x18);
    uint32_t d2 = d0 & 0x3fu;
    img[a0 + 3] = (uint8_t)d2;
    img[a0 + 0] = (uint8_t)d2;
    d2 = d1 & 0x3fu;
    img[a0 + 4] = (uint8_t)d2;
    d0 = ((d0 << 4) | (d0 >> 4)) & 0xffu;      /* rol.b #4,d0 */
    d0 &= 0x0cu;
    d1 = ((d1 << 2) | (d1 >> 6)) & 0xffu;      /* rol.b #2,d1 */
    d1 &= 0x03u;
    d0 |= d1;
    img[a0 + 1] = (uint8_t)d0;
    img[a0 + 2] = (uint8_t)d0;
    img[a0 + 5] = 0xff;
    if (img[a0 + 3] < img[a0 + 4])
        img[a0 + 5] = 0x01;
    {
        uint32_t d4 = img[0x3aec0 + d7];
        d4 = (uint8_t)(d4 << 3);   /* solo lsl.w #3 */
        img[a0 + 6] &= (uint8_t)~d4;
    }
}

/* cmd 4 (0x3ae68): volumen directo (channel[0x2b]) y lo refleja en el stream. No
 * reescribe channel[8] (el original consume el byte con (a0)+ sin guardarlo). */
void music_h5(uint8_t *img, uint32_t a3) {
    uint32_t a0 = be32(img + a3 + 8);
    uint32_t d0 = img[a0] & 0x0fu;
    a0 += 1;
    img[a3 + 0x2b] = (uint8_t)d0;
    img[a0 + 0x2f] = (uint8_t)d0;
}

/* FUN_0003aab6 @ 0x3aab6: handler del `trap #4` (vector 0x24). Un tick de un canal:
 * fade de volumen, nota o comando (transposicion, instrumentos, envolvente, duracion),
 * fin de patron/cancion y el bloque de canal enlazado. A3 = canal, A6 = estado global,
 * D7 = indice de canal. Termina en el `rte` 0x3ad58 (aqui, return). */
void music_tick(uint8_t *img, uint32_t a3, uint32_t a6, uint32_t d7) {
    uint32_t a0 = 0, d0 = 0, d2, d4;
    img[0x484] &= (uint8_t)~0x01u;
    img[0x484] &= (uint8_t)~0x04u;
    if (img[a3 + 0x27] & 0x40u)
        return;
    wr32(img + a6 + 0x14, be32(img + a3 + 0x18));
    if ((uint32_t)img[a3 + 0x2f] != img[a3 + 0x2b]) {
        uint8_t c = (uint8_t)(img[a3 + 0x2c] + 1);
        img[a3 + 0x2c] = c;
        if (c > img[a6 + 0x1e]) {
            img[a3 + 0x2c] = 0;
            if (img[a3 + 0x2f] > img[a3 + 0x2b])
                img[a3 + 0x2b]++;
            else
                img[a3 + 0x2b]--;
        }
    }
    {
        uint8_t f = img[a3 + 0x27];        img[a3 + 0x27] = (uint8_t)(f & ~0x01u);
        if (f & 0x01u)
            goto L3abb4;
    }
    if (img[a6 + 0x1b] == 0)
        img[a3 + 0x28]--;
    if (img[a3 + 0x28] != 0)
        goto L3ad14;
    img[a3 + 0x28] = img[a3 + 0x29];
L3ab3a:
    a0 = be32(img + a3 + 8);
    d0 = img[a0];
    a0 += 1;
    wr32(img + a3 + 8, a0);
    if (d0 != 0xff)
        goto L3abe2;
    /* fin de patron: lee el puntero de secuencia channel[0xc] */
L3ab48:
    a0 = be32(img + a3 + 0xc);
    d0 = img[a0];
    a0 += 1;
    if (d0 != 0xff)
        goto L3abc0;
    if (img[a3 + 0x27] & 0x20u)
        goto L3abb4;
    {
        uint32_t a1 = be32(img + a3 + 4);
        if ((img[a1 + 0x27] & 0x02u) && (img[a3 + 0x26] & 0x02u)) {
            wr16(img + a1 + 0x26, (uint16_t)(be16(img + a1 + 0x26) | 0x0100u));
            img[a1 + 0x2b] = 0;
        }
        img[a1 + 0x27] = (uint8_t)(img[a1 + 0x27] & ~0x02u);
        wr16(img + a3 + 0x26, (uint16_t)(be16(img + a3 + 0x26) | 0x0040u));
        d4 = img[0x3aec0 + d7];
        psg_shadow(img, 0, 8, d7, 0);
        d4 = (uint8_t)((d4 << 3) | d4);
        img[be32(img + a3 + 0x18) + 6] |= (uint8_t)d4;
    }
    return;
L3abb4:
    wr32(img + a3 + 0xc, be32(img + a3));
    wr16(img + a3 + 0x24, 0);
    goto L3ab48;
L3abc0:
    wr32(img + a3 + 0xc, a0);
    wr16(img + a3 + 0x24, (uint16_t)(be16(img + a3 + 0x24) + 1));
    d0 &= 0xffu;
    a0 = be32(img + a6 + 0x10);
    a0 = (uint32_t)(int32_t)(int16_t)be16(img + a0 + ((d0 << 1) & 0xffffu));
    a0 += be32(img + a6 + 0xc);
    wr32(img + a3 + 8, a0);
    goto L3ab3a;
L3abe2:
    wr32(img + a3 + 8, a0);
    d0 &= 0xffu;
    if (d0 & 0x80u)
        goto L3ac5c;
    /* nota: periodo de la tabla (con transposicion salvo nota 0) */
    a0 = 0x3aec4;
    if (d0 != 0)
        d0 = (d0 + img[a3 + 0x2a]) & 0xffu;
    d0 = (d0 << 1) & 0xffffu;
    wr16(img + a3 + 0x22, be16(img + a0 + d0));
    wr16(img + a3 + 0x20, be16(img + a0 + d0));
    for (uint32_t i = 0; i < 2; i++) {
        uint32_t v = be32(img + a3 + 0x10 + i * 4);
        img[v + 0xa] = 0;
        wr16(img + v + 2, 0);
        wr16(img + v + 0, 0);
        img[v + 0xd] = 0;
        img[v + 0xc] = 0;
        img[v + 0xb] |= 0x01u;
        img[v + 0xb] &= (uint8_t)~0x04u;
    }
    goto L3ad14;
L3ac5c:
    if (d0 & 0x40u)
        goto L3acb2;
    if (d0 & 0x20u)
        goto L3ac68;
    /* duracion desde la tabla 0x3af9c */
    d0 &= 0x0fu;
    img[a3 + 0x29] = img[0x3af9c + d0];
    img[a3 + 0x28] = img[0x3af9c + d0];
    goto L3ab3a;
L3ac68:
    /* volumen/envolvente directos a channel[0x18] */
    d4 = (uint8_t)(img[0x3aec0 + d7] << 3);   /* solo lsl.w #3 */
    {
        uint32_t a2 = be32(img + a3 + 0x18);
        d0 &= 0x1fu;
        if (d0 != 0)
            img[a2 + 6] &= (uint8_t)~d4;
        else
            img[a2 + 6] |= (uint8_t)d4;
        img[a2 + 1] = 0;
        img[a2 + 0] = (uint8_t)d0;
    }
    goto L3ab3a;
L3acb2:
    if (d0 & 0x20u) {
        uint32_t idx = d0 & 7u;
        if (idx == 0) { music_h1(img, a3); goto L3ab3a; }
        if (idx == 1) { music_h3(img, a3, a6); goto L3ab3a; }
        if (idx == 2) { music_h2(img, a3, a6); goto L3ab3a; }
        if (idx == 3) { music_h4(img, a3, d7); goto L3ab3a; }
        if (idx == 4) { music_h5(img, a3); return; }
        return;   /* fuera de la tabla de 5 entradas */
    }
    /* instrumento de la voz 1 (channel[0x10]) con la tabla del estado [4] */
    a0 = be32(img + a6 + 4);
    d0 &= 0x1fu;
    a0 += (d0 * 13u) & 0xffffu;
    {
        uint32_t a2 = be32(img + a3 + 0x10);
        img[a2 + 0xb] = (uint8_t)(img[a0] & 0x80u);
        img[a2 + 8] = (uint8_t)(img[a0] & 0x7fu);
        a0 += 1;
        img[a2 + 0xa] = 0;
        wr16(img + a2 + 2, 0);
        wr16(img + a2 + 0, 0);
        img[a2 + 0xd] = 0;
        img[a2 + 0xc] = 0;
        img[a2 + 0xb] |= 0x01u;
        wr32(img + a2 + 4, a0);
    }
    goto L3ab3a;
L3ad14:
    d0 = 0;
    a0 = be32(img + a3 + 0x10);
    d0 = music_note(img, a0, d0);
    d2 = img[a3 + 0x2b];
    if (d2 > img[a6 + 0x1d])
        d2 = img[a6 + 0x1d];
    d2 = (0x0fu - d2) & 0xffffu;
    if (d0 < d2)
        d0 = 0;
    else
        d0 = (d0 - d2) & 0xffffu;
    psg_shadow(img, d0, 8, d7, 0);
    a0 = be32(img + a3 + 0x14);
    d0 = be16(img + a3 + 0x20);
    d0 = music_note(img, a0, d0);
    psg_shadow(img, d0, 0x8000, d7, 0);
    (void)0;
}
