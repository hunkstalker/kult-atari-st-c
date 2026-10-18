/* sound.c — envoltorio de escritura del PSG (0x1929a).
 *
 * FUN_0001929a @ 0x1929a (ym_write): `move.w #$2700,sr` (sin efecto en imagen),
 * selecciona el registro D0 escribiendo $ff8800, y para el registro 7 (mixer) hace
 * un read-modify-write: lee el puerto de DATOS $ff8802, fusiona los bits 0-5 de D1 y
 * conserva los bits 6-7 (direccion de los puertos A/B). Despues escribe D1 en el
 * registro. En hardware real, leer $ff8802 es bus abierto (0xFF): el chip solo
 * responde por $ff8800; el kit sirve OS_PSG_DATA_READ (ver os.h/TRAP_MODEL.md).
 *
 * El candidato no tiene un "select" propio (su API selecciona y accede en la misma
 * llamada), asi que un select con D0 > 0x0f diverge del original — que lo rechaza
 * dejando el latch anterior y escribiendo el dato ahi. Ningun llamante lo hace.
 */
#include <stdint.h>

#include "machine.h"
#include "addrs.h"
#include "os.h"
#include "psg.h"
#include "hw.h"

void load_eche_bank_wrap(uint8_t *img, uint32_t d0);
uint32_t advance_text_entry(uint8_t *img, uint32_t a1, uint32_t d4);

/* 0x1929a — escribe registro/dato en $FF8800/$FF8802 (reg 7 = mixer) */
void ym_write(uint8_t *img, uint32_t reg_in, uint32_t data, uint32_t *reg_out,
                uint32_t *data_out) {
    (void)img;
    uint32_t reg = reg_in & 0xff;
    if (reg == 7) {
        uint32_t bus = OS_PSG_DATA_READ;
        data = ((data ^ bus) & 0x3f) ^ bus;
    }
    psg_port_write(reg, (uint8_t)data);
    if (reg_out) *reg_out = reg_in;
    if (data_out) *data_out = data;
}

/* FUN_00019282 @ 0x19282: silencia/resetea el PSG: escribe D1 (0 inicial) en los
 * registros 0xd..0 y luego 0x18 en el mixer (registro 7). D1 NO se recarga en el
 * bucle: ym_write lo pisa con el resultado del RMW al pasar por el registro 7, y los
 * registros 6..0 reciben ese valor (el ledger del oraculo lo confirma). */
void ym_reset(uint8_t *img) {
    uint32_t data = 0;
    for (uint32_t reg = 0xd;; reg = (reg - 1) & 0xffff) {
        ym_write(img, reg, data, 0, &data);
        if (reg == 0)
            break;
    }
    ym_write(img, 7, 0x18, 0, 0);
}

/* ---- driver de sonido (0x18fc4..0x1909e) ------------------------------------------------
 *
 * Las MFP son escrituras a registros fuera de la imagen: van por hw_write8 (ledger del
 * kit). La lectura de $fffa19 del stop es la del modelo (0 tras el clr), asi que el
 * bucle no se transcribe. */

/* FUN_00018fc6 @ 0x18fc6: cuerpo de sound_timer_start con D1 como byte de recarga (el
 * `clr.b d1` de 0x18fc4 es la unica diferencia). Elige el banco de samples (tabla de
 * punteros 0x1a4e8 si D4 bit7, y carga el recurso con 0x1066e si $1a57c != 0xff),
 * avanza la entrada (0x10f8e deja A2 en $1a4dc), arma el Timer A del MFP y publica
 * los punteros 0x192ca/0x192ce/0x192c6/0x192c2 y el vector $134 = 0x1910e. */
static void sound_start_body(uint8_t *img, uint32_t reload, uint32_t bank) {
    uint32_t p = 0x55b0a;
    if (bank & 0x80) {
        uint32_t idx = (bank & 0x70) >> 2;
        bank &= 0x0f;
        p = be32(img + 0x1a4e8 + idx);
        idx = (idx >> 2) & 0xff;
        if (img[0x1a57c] != 0xff) {
            load_eche_bank_wrap(img, idx);
            p = 0x64c8e;
        }
    }
    p = advance_text_entry(img, p, bank);
    wr32(img + 0x192ca, be32(img + A_CUR_STR_END));
    img[0x192ce] = img[p];
    p++;
    hw_write8(0xfffa19, 0);
    hw_write8(0xfffa13, 0x20);
    hw_write8(0xfffa07, 0x20);
    hw_write8(0xfffa19, 2);
    if ((reload & 0xff) != 0)
        img[p] = (uint8_t)reload;
    hw_write8(0xfffa1f, img[p]);
    p++;
    wr32(img + 0x192c6, p);
    wr32(img + 0x192c2, p);
    wr32(img + 0x134, 0x1910e);
}

/* FUN_00018fc4 @ 0x18fc4: entrada canonica (D1 = 0). */
void sound_timer_start(uint8_t *img, uint32_t bank) {
    sound_start_body(img, 0, bank);
}

/* FUN_00018fc6 @ 0x18fc6: entrada con D1 explicito (la usa 0x18f7a). */
void sound_timer_start_body(uint8_t *img, uint32_t reload, uint32_t bank) {
    sound_start_body(img, reload, bank);
}

/* FUN_00019084 @ 0x19084: para el Timer A: TACR=0, IERB=0, IERA=0 y limpia 0x192d0.
 * El `tst.b $fa19; bne` espera a que TACR quede a 0; el modelo lo sirve 0. */
void sound_timer_stop(uint8_t *img) {
    hw_write8(0xfffa19, 0);
    hw_write8(0xfffa13, 0);
    hw_write8(0xfffa07, 0);
    img[0x192d0] = 0;
}

/* FUN_0001909e @ 0x1909e: apaga el sonido: limpia 0x484, resetea el PSG (0x19282),
 * silencia el mixer (ym_write 7 con 0x3f) y para el Timer A (0x19084). */
void sound_off(uint8_t *img) {
    img[0x484] = 0;
    ym_reset(img);
    ym_write(img, 7, 0x3f, 0, 0);
    sound_timer_stop(img);
}

/* FUN_0001906a @ 0x1906a: arranca el Timer A (0x18fc4), apunta el vector $134 a la
 * variante 0x190f6 y reinicia el puntero de sample a 0x192ca. */
void sound_vector(uint8_t *img, uint32_t bank) {
    sound_timer_start(img, bank);
    wr32(img + 0x134, 0x190f6);
    wr32(img + 0x192c6, be32(img + 0x192ca));
}

/* ---- envoltorios XBIOS Supexec (0x18f6e..0x18fa4) ----
 *
 * Todos saltan el `trap #14/$26` cuando 0x1ab4f != 0 (el original tambien): el
 * candidato ejecuta la rutina destino directamente. Cada envoltorio elige una
 * entrada distinta; 0x18f7a usa 0x18fc6 (D1 explicito) y 0x18f8e la canonica 0x18fc4. */
void supexec_b(uint8_t *img, uint32_t reload, uint32_t bank) {
    if (img[A_FRAME_1AB4F] == 0)
        sound_timer_start_body(img, reload, bank);
}
/* 0x18f84 — sound stop supexec */
void sound_stop_supexec(uint8_t *img) {
    if (img[A_FRAME_1AB4F] == 0)
        sound_off(img);
}
/* 0x18f8e — ciclo de "modo/vista" $1acca (0..3). Cada rama fija $1acc6, corre 0x14496 y pinta con los blitters 111b6/114ba/117fe/11afa (o sus envoltorios). Los casos 1/3... */
void sound_play_a(uint8_t *img, uint32_t bank) {
    if (img[A_FRAME_1AB4F] == 0)
        sound_timer_start(img, bank);
}
/* 0x18f98 — sound play b */
void sound_play_b(uint8_t *img, uint32_t bank) {
    if (img[A_FRAME_1AB4F] == 0)
        sound_vector(img, bank);
}
/* 0x18fa2 — supexec a */
void supexec_a(uint8_t *img, uint32_t *script) {
    if (img[A_FRAME_1AB4F] == 0)
        sound_timer_stop(img);
    *script += 1;
}
/* 0x18fa4 — sound stop */
void sound_stop(uint8_t *img) {
    if (img[A_FRAME_1AB4F] == 0)
        sound_timer_stop(img);
}

/* FUN_00018f6e @ 0x18f6e: handler de byte que lee D4/D1 de A6[1]/A6[2], A6 += 3 y cae
 * en 0x18f7a. */
uint32_t supexec_wrap(uint8_t *img, uint32_t script) {
    uint32_t bank = img[script + 1];
    uint32_t reload = img[script + 2];
    script += 3;
    supexec_b(img, reload, bank);
    return script;
}

/* ---- ISR del Timer A (0x190b8 / 0x190f6 / 0x1910e) ----
 *
 * Las tres entradas comparten la cola 0x19146 y el epilogo 0x1913a (bclr #5,$fa0f +
 * rte). El candidato las modela como funciones que retornan; el oraculo se compara con
 * stop_pc=0x19144 (el rte). `movep.l d0,0(a0)` con A0 = $ff8800 escribe
 * $ff8800/$ff8802/$ff8804/$ff8806: dos registros PSG por long, con los pares
 * [reg1][val1][reg2][val2] de la tabla 0x19184. */

static void pcm_movep(uint8_t *img, uint32_t idx) {
    /* La base del `move.l (d16,pc,idx.w),idx` es la direccion de la PALABRA DE EXTENSION
     * (0x19130 + 0x52 = 0x19182), no la de la instruccion siguiente: los longs de la
     * tabla van en 0x19182 + (byte & 0xfc). */
    uint32_t v = be32(img + 0x19182 + (idx & 0xfc));
    psg_port_write((v >> 24) & 0xff, (v >> 16) & 0xff);
    psg_port_write((v >> 8) & 0xff, v & 0xff);
}

/* Cuerpo de 0x190b8: carga el siguiente bloque de la lista de samples (palabra 0 nula
 * -> parar). Sin el epilogo, que comparten las tres entradas. */
static void pcm_body_190b8(uint8_t *img) {
    uint32_t p = be32(img + 0x1930e);
    if (be16(img + p) != 0) {
        hw_write8(0xfffa1f, img[p + 1]);
        p += 2;
        uint32_t off = be16(img + p); p += 2;
        wr32(img + 0x192c6, (off + be32(img + 0x192be)) & 0xffffffffu);
        off = be16(img + p); p += 2;
        wr32(img + 0x192ca, (off + be32(img + 0x192be)) & 0xffffffffu);
        wr32(img + 0x1930e, p);
    } else {
        img[0x192cf] = 0;
        sound_timer_stop(img);
    }
}

/* Cola 0x19146: reenganche, repeticion (rebobina y, si 0x192d0, decrementa el TADR)
 * o parada. */
static void pcm_tail(uint8_t *img) {
    if (img[0x192cf] != 0) {
        pcm_body_190b8(img);
        return;
    }
    if (img[0x192ce] == 0) {
        img[0x192cf] = 0;
        sound_timer_stop(img);
        return;
    }
    wr32(img + 0x192c6, be32(img + 0x192c2));
    if (img[0x192d0] != 0)
        hw_sub8(0xfffa1f, 1);
}

/* FUN_000190b8 @ 0x190b8: carga el siguiente bloque de samples (entrada propia). */
void sound_isr_load(uint8_t *img) {
    pcm_body_190b8(img);
    hw_bclr8(0xfffa0f, 5);
}

/* FUN_0001910e @ 0x1910e: ISR PCM hacia delante (vector $134). */
void sound_isr_fwd(uint8_t *img) {
    uint32_t p = be32(img + 0x192c6);
    if (p < be32(img + 0x192ca)) {
        uint32_t byte = img[p++];
        wr32(img + 0x192c6, p);
        pcm_movep(img, byte & 0xfc);
    } else {
        pcm_tail(img);
    }
    hw_bclr8(0xfffa0f, 5);
}

/* FUN_000190f6 @ 0x190f6: ISR PCM hacia atras (vector $134, variante 0x1906a). */
void sound_isr_back(uint8_t *img) {
    uint32_t p = be32(img + 0x192c6);
    if (p >= be32(img + 0x192c2)) {
        uint32_t byte = img[--p];
        wr32(img + 0x192c6, p);
        pcm_movep(img, byte & 0xfc);
    } else {
        pcm_tail(img);
    }
    hw_bclr8(0xfffa0f, 5);
}
