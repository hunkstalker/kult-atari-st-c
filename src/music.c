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

void psg_trap3(uint8_t *img, uint32_t value, uint32_t base, uint32_t off,
                 uint32_t *value_out);

/* FUN_0003aa7c @ 0x3aa7c: escribe el registro D6 con D0 y verifica releyendo $ff8800.
 * En el modelo el chip devuelve lo escrito, asi que la verificacion pasa a la primera;
 * aun asi el READ es un evento del ledger y hay que emitirlo. */
void psg_write_verify(uint8_t *img, uint32_t reg, uint32_t value) {
    (void)img;
    psg_port_write(reg & 0xff, (uint8_t)value);
    (void)psg_port_read(reg & 0xff);
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
void psg_shadow(uint8_t *img, uint32_t value, uint32_t base, uint32_t off,
                 uint32_t *value_out) {
    uint32_t reg;
    if (base & 0x8000) {
        reg = off;
        reg = (reg & 0xffff0000u) | ((reg << 1) & 0xffffu);
        reg = (reg & 0xffffff00u) | ((reg + (base & 0xffu)) & 0xffu);
        wr16(img + MUSIC_SHADOW + (uint32_t)(int32_t)(int16_t)(reg & 0xffffu),
             (uint16_t)value);
    } else {
        reg = base;
        reg = (reg & 0xffffff00u) | ((reg + (off & 0xffu)) & 0xffu);
        img[MUSIC_SHADOW + (uint32_t)(int32_t)(int16_t)(reg & 0xffffu)] = (uint8_t)value;
    }
    if (value_out) *value_out = value;
}

/* FUN_0003a85e @ 0x3a85e: silencia los tres canales emitiendo `trap #3` con D0=0 y
 * D2=8 (registros de volumen 8/9/10), D7 de 0 a 2. El original fija D2 con `move.w`
 * (conserva la mitad alta) y D0/D7 con `moveq` (32 bits). */
void music_silence(uint8_t *img, uint32_t base_in, uint32_t *value_out, uint32_t *base_out,
                 uint32_t *off_out) {
    uint32_t value = 0, base = (base_in & 0xffff0000u) | 8u, off = 0;
    for (int i = 0; i < 3; i++) {
        psg_trap3(img, value, base, off, 0);
        if (i != 2)
            off = (off + 1) & 0xffffffffu;   /* addq.w #1,off entre traps */
    }
    if (value_out) *value_out = value;
    if (base_out) *base_out = base;
    if (off_out) *off_out = off;
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
uint32_t music_install(uint8_t *img, uint32_t *old_out) {
    mfp_setup(img);
    wr32(img + 0x8c, 0x3aa38);
    uint32_t old = be32(img + 0x90);
    wr32(img + 0x90, 0x3aab6);
    if (old_out) *old_out = old;
    return old;
}

/* Codelets del despacho (no estan en el indice de 406): cada uno fija un parametro del
 * estado global y sale con C=0. */
static void disp_3a59c(uint8_t *img, uint32_t state, uint32_t value) {
    img[state + 0x1c] = (uint8_t)value;
    img[state + 0x1b] = (uint8_t)value;
}
static void disp_3a5aa(uint8_t *img, uint32_t state, uint32_t value) {
    img[state + 0x1d] = (uint8_t)(value & 0x0fu);
}
static void disp_3a5b8(uint8_t *img, uint32_t state, uint32_t value) {
    img[state + 0x1e] = (uint8_t)value;
}

/* FUN_0003a57c @ 0x3a57c: recorre la tabla A0 ({word clave, long handler}, fin en 0) y
 * salta al handler cuya clave es D1. Devuelve el carry resultante: 0 encontrado, 1
 * agotada la tabla (`ori.b #1,ccr`). */
uint32_t music_cmd_table(uint8_t *img, uint32_t table, uint32_t key, uint32_t value,
                     uint32_t state) {
    for (;;) {
        uint32_t k = be16(img + table);
        table += 2;
        if (k == 0)
            return 1;
        if (k == (key & 0xffffu)) {
            uint32_t handler = be32(img + table);
            if (handler == 0x3a59c)
                disp_3a59c(img, state, value);
            else if (handler == 0x3a5aa)
                disp_3a5aa(img, state, value);
            else if (handler == 0x3a5b8)
                disp_3a5b8(img, state, value);
            return 0;
        }
        table += 4;
    }
}

/* FUN_0003a62e @ 0x3a62e: carga una cancion. `selector` indexa la cabecera `base`
 * (0x3b1ac): word de offset al bloque; de ahi salen la duracion, las tablas del estado
 * (4/8/0xc/0x10), el puntero de cancion (canal[0]) y los flags/volumenes iniciales.
 * Termina silenciando (0x3a85e) y limpiando el contador de estado. D2 entra sin tocar. */
void music_load(uint8_t *img, uint32_t selector, uint32_t base, uint32_t d2) {
    uint32_t state = MUSIC_STATE;
    uint32_t off = selector & 0xffffu;
    img[state + 0x1a] = 0;
    off = be16(img + base + ((off << 1) & 0xffffu));
    uint32_t p = base + (uint32_t)(int32_t)(int16_t)(off & 0xffffu);
    wr32(img + state + 0xc, p);
    uint32_t p0 = p;
    off = be16(img + p); p += 2;
    img[state + 0x1b] = (uint8_t)off;
    img[state + 0x1c] = (uint8_t)off;
    off = be16(img + p); p += 2;
    wr32(img + state + 4, p0 + (uint32_t)(int32_t)(int16_t)(off & 0xffffu));
    off = be16(img + p); p += 2;
    wr32(img + state + 8, p0 + (uint32_t)(int32_t)(int16_t)(off & 0xffffu));
    off = be16(img + p); p += 2;
    wr32(img + 0x3afbc, p0 + (uint32_t)(int32_t)(int16_t)(off & 0xffffu));
    off = be16(img + p); p += 2;
    wr32(img + 0x3b00c, p0 + (uint32_t)(int32_t)(int16_t)(off & 0xffffu));
    off = be16(img + p); p += 2;
    wr32(img + 0x3b05c, p0 + (uint32_t)(int32_t)(int16_t)(off & 0xffffu));
    wr32(img + state + 0x10, p);
    img[state + 0x1d] = 0x0f;
    img[0x3b1a2] = 0x3f;
    img[0x3b1aa] = 0x3f;
    for (uint32_t ch = 0; ch < 3; ch++) {
        uint32_t chan = 0x3afbc + ch * 0x50;
        wr16(img + chan + 0x26, (uint16_t)(be16(img + chan + 0x26) | 0x0040u));
        img[chan + 0x2b] = 0x0f;
        img[chan + 0x2f] = 0x0f;
    }
    music_silence(img, d2, 0, 0, 0);
    wr16(img + state + 0x18, 0);
}

/* FUN_0003a734 @ 0x3a734: procesa el comando D1 (word) con valor D5 (word) sobre los
 * canales que seleccionan D1[0..2] (si son 0, los tres). Devuelve D0 = 0xffff o el
 * minimo 0x24 visto. Antes despacha D1 a 0x3a57c (comandos 0x400/0x800/0x1000). */
uint32_t music_cmd(uint8_t *img, uint32_t value, uint32_t cmd) {
    uint32_t minv = 0xffff;
    value &= 0xffffu;
    cmd &= 0xffffu;
    if (music_cmd_table(img, 0x3afa8, cmd, value, MUSIC_STATE) == 0)
        return minv;
    uint32_t mask = cmd & 0x0fu;
    if (mask == 0)
        mask = 0x0fu;
    uint32_t chan = 0x3afbc;
    uint32_t i = 0;
    for (;;) {
        uint32_t bit = mask & 1u;
        mask >>= 1;
        if (bit) {
            if (cmd & 0x8000u)
                img[chan + 0x2f] = (uint8_t)(value & 0x0fu);
            if (cmd & 0x2000u) {
                img[chan + 0x2b] = (uint8_t)(value & 0x0fu);
                img[chan + 0x2f] = (uint8_t)(value & 0x0fu);
            }
            if (cmd & 0x0100u) {
                img[chan + 0x2b] = 0;
                img[chan + 0x2f] = img[MUSIC_STATE + 0x1d];
            }
            if (cmd & 0x0200u)
                img[chan + 0x2f] = 0;
            if (cmd & 0x0040u) {
                uint32_t bits = img[0x3aec0 + i];
                bits = (uint8_t)((bits << 3) | bits);
                img[0x3b1a2] |= (uint8_t)bits;
                wr16(img + chan + 0x26, (uint16_t)(be16(img + chan + 0x26) | 0x0040u));
            }
            if (cmd & 0x0080u) {
                wr16(img + chan + 0x26, 0);
                wr16(img + chan + 0x26, (uint16_t)(be16(img + chan + 0x26) | 0x0080u));
                wr16(img + chan + 0x26, (uint16_t)(be16(img + chan + 0x26) | 0x0001u));
                img[MUSIC_STATE + 0x1a] = 0xff;
                uint32_t bits = img[0x3aec0 + i];   /* solo `t`: sin lsl */
                img[0x3b1a2] &= (uint8_t)~bits;
            }
            if (cmd & 0x0020u)
                wr16(img + chan + 0x26, (uint16_t)(be16(img + chan + 0x26) | 0x0020u));
            if (cmd & 0x0010u) {
                if (!(img[chan + 0x27] & 0x40u)) {
                    uint32_t period = be16(img + chan + 0x24);
                    if (minv >= period)
                        minv = period;
                }
            }
        }
        chan += 0x50;
        i += 1;
        if (i == 3)
            break;
    }
    return minv;
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
uint32_t music_note(uint8_t *img, uint32_t voice, uint32_t period) {
    uint32_t idx = img[voice + 0xa];
    uint32_t instr = be32(img + voice + 4) + (idx & 0xffffu);
    uint8_t flags = img[voice + 0xb];
    img[voice + 0xb] = (uint8_t)(flags & ~0x01u);
    if (flags & 0x01u) {
        idx = (uint32_t)(int32_t)(int8_t)img[instr + 1];
        wr16(img + voice + 2, (uint16_t)(be16(img + voice + 2) + idx));
    }
    period = (period + be16(img + voice + 2)) & 0xffffu;
    period = (period - be16(img + voice)) & 0xffffu;
    if (period & 0x8000u)
        period = 0;
    if (img[voice + 0xb] & 0x04u)
        return period;
    img[voice + 0xc] = (uint8_t)(img[voice + 0xc] + 1);
    if (img[voice + 0xc] != img[instr + 2])
        return period;
    img[voice + 0xd] = (uint8_t)(img[voice + 0xd] + 1);
    img[voice + 0xc] = 0;
    if (img[voice + 0xd] != img[instr]) {
        img[voice + 0xb] |= 0x01u;
        return period;
    }
    img[voice + 0xd] = 0;
    idx = (uint8_t)(img[voice + 0xa] + 3);
    if (idx == 0x0c) {
        if (img[voice + 0xb] == 0) {
            img[voice + 0xb] |= 0x04u;
            return period;
        }
        img[voice + 0xd] = 0;
        idx = (uint32_t)(int32_t)(int8_t)img[voice + 8];
        wr16(img + voice, (uint16_t)(be16(img + voice) + idx));
        idx = 3;
    }
    img[voice + 0xa] = (uint8_t)idx;
    img[voice + 0xb] |= 0x01u;
    return period;
}

/* Handlers de comando del tick (tabla 0x3aeac). Los cuatro primeros continuan en
 * 0x3ab3a (por eso el tick los llama y salta); 0x3ae68 termina con rts. */

/* cmd 0 (0x3ad5a): transposicion desde el stream. */
void music_h1(uint8_t *img, uint32_t chan) {
    uint32_t p = be32(img + chan + 8);
    img[chan + 0x2a] = img[p];
    wr32(img + chan + 8, p + 1);
}

/* cmd 2 (0x3ad6a): cambia el instrumento de la voz 2 (channel[0x14]) con la tabla del
 * estado [8]; flag 0. */
void music_h2(uint8_t *img, uint32_t chan, uint32_t state) {
    uint32_t p = be32(img + chan + 8);
    uint32_t idx = img[p];
    p += 1;
    wr32(img + chan + 8, p);
    p = be32(img + state + 8) + (((idx & 0xffu) * 13u) & 0xffffu);
    uint32_t voice = be32(img + chan + 0x14);
    img[voice + 0xb] = 0;
    img[voice + 8] = 0;
    img[voice + 0xa] = 0;
    wr16(img + voice + 2, 0);
    wr16(img + voice + 0, 0);
    img[voice + 0xd] = 0;
    img[voice + 0xc] = 0;
    img[voice + 0xb] |= 0x01u;
    wr32(img + voice + 4, p + 1);
}

/* cmd 1 (0x3adb4): como el anterior pero con el flag 0x80 (loop) en la voz 2. */
void music_h3(uint8_t *img, uint32_t chan, uint32_t state) {
    uint32_t p = be32(img + chan + 8);
    uint32_t idx = img[p];
    p += 1;
    wr32(img + chan + 8, p);
    p = be32(img + state + 8) + (((idx & 0xffu) * 13u) & 0xffffu);
    uint32_t voice = be32(img + chan + 0x14);
    idx = img[p];
    p += 1;
    img[voice + 0xb] = 0x80;
    img[voice + 8] = 0;
    img[voice + 0xa] = 0;
    wr16(img + voice + 2, 0);
    wr16(img + voice + 0, 0);
    img[voice + 0xd] = 0;
    img[voice + 0xc] = 0;
    img[voice + 0xb] |= 0x01u;
    wr32(img + voice + 4, p);
}

/* cmd 3 (0x3ae00): envolvente/volumen de la voz (channel[0x18]) a partir de dos bytes;
 * limpia el bit de canal en A0[6] via la tabla de bits. */
void music_h4(uint8_t *img, uint32_t chan, uint32_t i) {
    uint32_t p = be32(img + chan + 8);
    uint32_t b0 = img[p];
    uint32_t b1 = img[p + 1];
    wr32(img + chan + 8, p + 2);
    p = be32(img + chan + 0x18);
    uint32_t v = b0 & 0x3fu;
    img[p + 3] = (uint8_t)v;
    img[p + 0] = (uint8_t)v;
    v = b1 & 0x3fu;
    img[p + 4] = (uint8_t)v;
    b0 = ((b0 << 4) | (b0 >> 4)) & 0xffu;      /* rol.b #4,b0 */
    b0 &= 0x0cu;
    b1 = ((b1 << 2) | (b1 >> 6)) & 0xffu;      /* rol.b #2,b1 */
    b1 &= 0x03u;
    b0 |= b1;
    img[p + 1] = (uint8_t)b0;
    img[p + 2] = (uint8_t)b0;
    img[p + 5] = 0xff;
    if (img[p + 3] < img[p + 4])
        img[p + 5] = 0x01;
    {
        uint32_t bits = img[0x3aec0 + i];
        bits = (uint8_t)(bits << 3);   /* solo lsl.w #3 */
        img[p + 6] &= (uint8_t)~bits;
    }
}

/* cmd 4 (0x3ae68): volumen directo (channel[0x2b]) y lo refleja en el stream. No
 * reescribe channel[8] (el original consume el byte con (a0)+ sin guardarlo). */
void music_h5(uint8_t *img, uint32_t chan) {
    uint32_t p = be32(img + chan + 8);
    uint32_t vol = img[p] & 0x0fu;
    p += 1;
    img[chan + 0x2b] = (uint8_t)vol;
    img[p + 0x2f] = (uint8_t)vol;
}

/* FUN_0003aab6 @ 0x3aab6: handler del `trap #4` (vector 0x24). Un tick de un canal:
 * fade de volumen, nota o comando (transposicion, instrumentos, envolvente, duracion),
 * fin de patron/cancion y el bloque de canal enlazado. A3 = canal, A6 = estado global,
 * D7 = indice de canal. Termina en el `rte` 0x3ad58 (aqui, return). */
void music_tick(uint8_t *img, uint32_t chan, uint32_t state, uint32_t i) {
    uint32_t p = 0, b = 0, v, bits;
    img[0x484] &= (uint8_t)~0x01u;
    img[0x484] &= (uint8_t)~0x04u;
    if (img[chan + 0x27] & 0x40u)
        return;
    wr32(img + state + 0x14, be32(img + chan + 0x18));
    if ((uint32_t)img[chan + 0x2f] != img[chan + 0x2b]) {
        uint8_t c = (uint8_t)(img[chan + 0x2c] + 1);
        img[chan + 0x2c] = c;
        if (c > img[state + 0x1e]) {
            img[chan + 0x2c] = 0;
            if (img[chan + 0x2f] > img[chan + 0x2b])
                img[chan + 0x2b]++;
            else
                img[chan + 0x2b]--;
        }
    }
    {
        uint8_t f = img[chan + 0x27];        img[chan + 0x27] = (uint8_t)(f & ~0x01u);
        if (f & 0x01u)
            goto L3abb4;
    }
    if (img[state + 0x1b] == 0)
        img[chan + 0x28]--;
    if (img[chan + 0x28] != 0)
        goto L3ad14;
    img[chan + 0x28] = img[chan + 0x29];
L3ab3a:
    p = be32(img + chan + 8);
    b = img[p];
    p += 1;
    wr32(img + chan + 8, p);
    if (b != 0xff)
        goto L3abe2;
    /* fin de patron: lee el puntero de secuencia channel[0xc] */
L3ab48:
    p = be32(img + chan + 0xc);
    b = img[p];
    p += 1;
    if (b != 0xff)
        goto L3abc0;
    if (img[chan + 0x27] & 0x20u)
        goto L3abb4;
    {
        uint32_t linked = be32(img + chan + 4);
        if ((img[linked + 0x27] & 0x02u) && (img[chan + 0x26] & 0x02u)) {
            wr16(img + linked + 0x26, (uint16_t)(be16(img + linked + 0x26) | 0x0100u));
            img[linked + 0x2b] = 0;
        }
        img[linked + 0x27] = (uint8_t)(img[linked + 0x27] & ~0x02u);
        wr16(img + chan + 0x26, (uint16_t)(be16(img + chan + 0x26) | 0x0040u));
        bits = img[0x3aec0 + i];
        psg_shadow(img, 0, 8, i, 0);
        bits = (uint8_t)((bits << 3) | bits);
        img[be32(img + chan + 0x18) + 6] |= (uint8_t)bits;
    }
    return;
L3abb4:
    wr32(img + chan + 0xc, be32(img + chan));
    wr16(img + chan + 0x24, 0);
    goto L3ab48;
L3abc0:
    wr32(img + chan + 0xc, p);
    wr16(img + chan + 0x24, (uint16_t)(be16(img + chan + 0x24) + 1));
    b &= 0xffu;
    p = be32(img + state + 0x10);
    p = (uint32_t)(int32_t)(int16_t)be16(img + p + ((b << 1) & 0xffffu));
    p += be32(img + state + 0xc);
    wr32(img + chan + 8, p);
    goto L3ab3a;
L3abe2:
    wr32(img + chan + 8, p);
    b &= 0xffu;
    if (b & 0x80u)
        goto L3ac5c;
    /* nota: periodo de la tabla (con transposicion salvo nota 0) */
    p = 0x3aec4;
    if (b != 0)
        b = (b + img[chan + 0x2a]) & 0xffu;
    b = (b << 1) & 0xffffu;
    wr16(img + chan + 0x22, be16(img + p + b));
    wr16(img + chan + 0x20, be16(img + p + b));
    for (uint32_t i = 0; i < 2; i++) {
        uint32_t v = be32(img + chan + 0x10 + i * 4);
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
    if (b & 0x40u)
        goto L3acb2;
    if (b & 0x20u)
        goto L3ac68;
    /* duracion desde la tabla 0x3af9c */
    b &= 0x0fu;
    img[chan + 0x29] = img[0x3af9c + b];
    img[chan + 0x28] = img[0x3af9c + b];
    goto L3ab3a;
L3ac68:
    /* volumen/envolvente directos a channel[0x18] */
    bits = (uint8_t)(img[0x3aec0 + i] << 3);   /* solo lsl.w #3 */
    {
        uint32_t voice = be32(img + chan + 0x18);
        b &= 0x1fu;
        if (b != 0)
            img[voice + 6] &= (uint8_t)~bits;
        else
            img[voice + 6] |= (uint8_t)bits;
        img[voice + 1] = 0;
        img[voice + 0] = (uint8_t)b;
    }
    goto L3ab3a;
L3acb2:
    if (b & 0x20u) {
        uint32_t idx = b & 7u;
        if (idx == 0) { music_h1(img, chan); goto L3ab3a; }
        if (idx == 1) { music_h3(img, chan, state); goto L3ab3a; }
        if (idx == 2) { music_h2(img, chan, state); goto L3ab3a; }
        if (idx == 3) { music_h4(img, chan, i); goto L3ab3a; }
        if (idx == 4) { music_h5(img, chan); return; }
        return;   /* fuera de la tabla de 5 entradas */
    }
    /* instrumento de la voz 1 (channel[0x10]) con la tabla del estado [4] */
    p = be32(img + state + 4);
    b &= 0x1fu;
    p += (b * 13u) & 0xffffu;
    {
        uint32_t voice = be32(img + chan + 0x10);
        img[voice + 0xb] = (uint8_t)(img[p] & 0x80u);
        img[voice + 8] = (uint8_t)(img[p] & 0x7fu);
        p += 1;
        img[voice + 0xa] = 0;
        wr16(img + voice + 2, 0);
        wr16(img + voice + 0, 0);
        img[voice + 0xd] = 0;
        img[voice + 0xc] = 0;
        img[voice + 0xb] |= 0x01u;
        wr32(img + voice + 4, p);
    }
    goto L3ab3a;
L3ad14:
    b = 0;
    p = be32(img + chan + 0x10);
    b = music_note(img, p, b);
    v = img[chan + 0x2b];
    if (v > img[state + 0x1d])
        v = img[state + 0x1d];
    v = (0x0fu - v) & 0xffffu;
    if (b < v)
        b = 0;
    else
        b = (b - v) & 0xffffu;
    psg_shadow(img, b, 8, i, 0);
    p = be32(img + chan + 0x14);
    b = be16(img + chan + 0x20);
    b = music_note(img, p, b);
    psg_shadow(img, b, 0x8000, i, 0);
    (void)0;
}
