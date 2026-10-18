/* mode.c — modo de escena y lista de objetos (bloque 0x1442c..0x14588).
 *
 * FUN_0001442c @ 0x1442c (cmd idx 74): guarda 0x1acc7, corre 0x1443e y lo restaura.
 * FUN_0001443e @ 0x1443e: fija 0x1acc6 = A6[1] (el valor anterior va a 0x1accb),
 *   escribe el marcador en los 8 bytes de 0x1a695 (0xe5 si 0x1ad32 != 0, 0xdd si
 *   A6[1] == 0x81, 0xde si == 0x82; en cualquier otro caso no toca el buffer),
 *   A6 += 2, camina la tabla de modos (0x14496) y reinicia el estado de comandos
 *   (0x14588). Devuelve A6 y expone A4/A5 porque 0x1460a los recorre despues de
 *   la llamada (la lista de objetos [A5, A4)).
 * FUN_00014496 @ 0x14496: camina la tabla 0x1ad38 por 0x1acc6 (1-based) hasta que
 *   0xd8eb == 0x52 (el original reintenta; con otro valor gira indefinidamente),
 *   publica los 5 bytes de cabecera de la entrada en 0x1acd2/0x1acc7/0x1ab48+
 *   0x1ab45/0x1acf4/0x1ab4b y, si 0x1ab4b != 0, limpia 0x1ab62..0x1abf7 y
 *   empaqueta los objetos (flags word + hasta 5 words segun bits 4..0) a 5 words
 *   por objeto. Deja 0x1a7ca/0x1a762/0x1a7d2 = fin de la lista, 0x1a7ce = fin de
 *   la entrada (los 8-byte entries de [A5, A4) los consume 0x16582/0x166ec) y
 *   limpia 0x1ab4a/0x1acd3/0x1aceb/0x1abf8/0x1acae/0x1acaa/0x1ace7.
 * FUN_00014588 @ 0x14588: reinicia los globals de comandos (0x1ab5x..0x1acf9).
 */
#include <stdint.h>

#include "machine.h"
#include "addrs.h"

void walk_entry(uint8_t *img, uint32_t a4, uint32_t d1, uint32_t *a4o,
                  uint32_t *a5o);

void command_state_reset(uint8_t *img) {
    wr16(img + A_DAT_1ABFA, 0);
    wr16(img + A_DAT_1AB5C, 0xa01c);
    wr16(img + A_DAT_1AB5E, 0);
    wr16(img + A_DAT_1AB56, 0x00f7);
    wr16(img + A_DAT_1AB50, 0x9048);
    wr16(img + A_DAT_1AB52, 0xa01d);
    wr16(img + A_DAT_1AB54, 0);
    wr16(img + A_DAT_1AB58, 0x9048);
    wr16(img + A_DAT_1AB5A, 0x9048);
    img[A_DAT_1AD09] = 0;
    img[A_DAT_1AD11] = 0;
    img[A_DAT_1AD0B] = 0;
    img[A_DAT_1AD27] = 0;
    img[A_DAT_1ACD1] = 0;
    img[A_DAT_1ACF9] = 0;
    img[A_DAT_1ACFA] = 0;
    img[A_DAT_1ACCE] = 0;
    img[A_DAT_1ACFF] = 0;
}

void scene_mode_load(uint8_t *img, uint32_t *a4_out, uint32_t *a5_out) {
    uint32_t a4 = A_MODE_TABLE, a5;
    do {
        uint32_t d1 = ((uint32_t)img[A_MODE_FLAG] - 1) & 0xffff;
        walk_entry(img, A_MODE_TABLE, d1, &a4, &a5);
    } while (img[A_DAT_D8EB] != 0x52);

    img[A_DAT_1ACD2] = img[a5++];
    img[A_DAT_1ACC7] = img[a5++];
    img[A_1AB48] = img[a5];
    img[A_TEXT_SEL_A] = img[a5++];
    img[A_PAL_INDEX_CUR] = img[a5++];
    img[A_DAT_1AB4B] = img[a5++];

    uint32_t count = img[A_DAT_1AB4B];
    if (count != 0) {
        for (uint32_t i = 0; i < 0x4b; i++)
            wr16(img + A_DAT_1AB62 + i * 2, 0);
        static const uint32_t field[5] = {0, 2, 4, 6, 8};
        uint32_t a3 = A_DAT_1AB62;
        uint32_t n = count - 1;
        for (;;) {
            uint32_t flags = be16(img + a5);
            a5 += 2;
            for (int bit = 4; bit >= 0; bit--) {
                if (flags & (1u << bit)) {
                    wr16(img + a3 + field[4 - bit], be16(img + a5));
                    a5 += 2;
                }
            }
            a3 += 10;
            if (n == 0)
                break;
            n--;
        }
    }

    wr32(img + A_OBJECT_LIST_BEGIN, a5);
    wr32(img + A_DAT_1A762, a5);
    wr32(img + A_OBJECT_LIST_END, a4);
    wr32(img + A_SEL_1A7D2, a5);
    img[A_SEL_1AB4A] = 0;
    img[A_DAT_1ACD3] = 0;
    img[A_DAT_1ACEB] = 0;
    wr16(img + A_DAT_1ABF8, 0);
    wr16(img + A_DAT_1ACAE, 0);
    wr16(img + A_DAT_1ACAA, 0);
    img[A_DAT_1ACE7] = 0;

    *a4_out = a4;
    *a5_out = a5;
}

uint32_t scene_mode_set(uint8_t *img, uint32_t a6, uint32_t *a4_out, uint32_t *a5_out) {
    uint32_t d0 = img[a6 + 1];
    img[A_DAT_1ACCB] = img[A_MODE_FLAG];
    img[A_MODE_FLAG] = (uint8_t)d0;
    if (img[A_DAT_1AD32] != 0 || d0 == 0x81 || d0 == 0x82) {
        uint8_t marker = img[A_DAT_1AD32] != 0 ? 0xe5 : (d0 == 0x81 ? 0xdd : 0xde);
        for (int i = 0; i < 8; i++)
            img[A_DAT_1A695 + i] = marker;
    }
    a6 += 2;
    scene_mode_load(img, a4_out, a5_out);
    command_state_reset(img);
    return a6;
}

uint32_t cmd74_wrap(uint8_t *img, uint32_t a6) {
    uint8_t saved = img[A_DAT_1ACC7];
    uint32_t a4, a5;
    a6 = scene_mode_set(img, a6, &a4, &a5);
    img[A_DAT_1ACC7] = saved;
    return a6;
}

/* ---- cluster 0x14602/0x1460a/0x146d0 (modo de escena) ----
 *
 * 0x1460a: con $1a570 == 0 y ($1ad0f & 0x38) == 8 corre el formulario 0x141fa;
 *   guarda $1ad0f & 7 en $1acc8, toca las animaciones 0x87/0x96 (0x188ce), camina
 *   el modo (0x1443e -> A4/A5), busca en la lista de objetos [A5,A4) el byte
 *   4 == ($1acc8|0x88) y publica el indice en $1acc9 ($1a572 |= indice), repinta
 *   texto/iconos (0x13f86, 0x13f56), la tabla de iconos (0x16460), hints/pistas
 *   (0x17094/0x172ca), acciones (0x17480) y la lista de iconos (0x16582).
 * 0x146d0: `addq.l #1,a6` y cae en el cuerpo 0x146d2 (que 0x14602 comparte tras
 *   0x1460a): restaura los iconos 1..3, transicion 0x15934/copy_screen, para el
 *   sonido, modo 0x16 -> sonido 0x83 + espera, 0x14748, paleta, 0x1857c/0x18944,
 *   formulario inverso 0x1424c con $1acc9 y los iconos de pista (0x16556). */
void anim_mode(uint8_t *img, uint32_t a6);
void sample_find_body(uint8_t *img);
void icon_repeat(uint8_t *img);
void rot_action(uint8_t *img);
void scene_text_box(uint8_t *img);
void run_text_table(uint8_t *img, uint32_t a6, uint32_t d0, uint32_t d1);
void textform_fwd(uint8_t *img, uint32_t a6, uint32_t d0);
void textform_rev(uint8_t *img, uint32_t a6, uint32_t d0);
void icon_table(uint8_t *img);
void hint_state_a(uint8_t *img, uint32_t a1);
void hint_state_b(uint8_t *img);
void update_icon_list(uint8_t *img);
void draw_hint_icons(uint8_t *img);
void elem_mode_blit(uint8_t *img);
void set_palette_current(uint8_t *img);
void copy_rows_a(uint8_t *img);
void copy_screen(uint8_t *img);
void sound_play_a(uint8_t *img, uint32_t d4);
void sound_stop(uint8_t *img);
void hint_blit_2(uint8_t *img, uint32_t *a6);
void hint_blit_stroke(uint8_t *img, uint32_t *a6);
void hint_blit_3(uint8_t *img, uint32_t *a6);

uint32_t scene_mode_apply(uint8_t *img, uint32_t a6) {
    img[A_DAT_1A572] = 0;
    uint32_t d1 = 0;
    if (img[A_SKIP_TRANSITION] == 0) {
        uint32_t t = img[A_DAT_1AD0F] & 0x38;
        if (t == 0) {
            d1 = img[A_DAT_1AD0F] & 7;
        } else if (t == 8) {
            img[A_DAT_1A572] = 1;
            textform_fwd(img, a6, img[A_SEL_1ACC9]);
            d1 = img[A_DAT_1AD0F] & 7;
        }
    }
    img[A_DAT_1ACC8] = (uint8_t)d1;
    anim_mode(img, a6);
    uint32_t a4, a5;
    a6 = scene_mode_set(img, a6, &a4, &a5);
    img[A_DAT_1ACFE] = img[A_DAT_1ACD2];
    d1 = img[A_DAT_1ACC8];
    uint32_t d2 = 0;
    if (d1 != 0) {
        d1 = (d1 | 0x88) & 0xff;
        uint32_t d2b = 1;
        for (;;) {
            if (img[a5 + 4] == d1) {
                d2 = d2b;
                break;
            }
            d2b = (d2b + 1) & 0xff;
            a5 += 8;
            if (a5 == a4)
                break;
        }
    }
    img[A_SEL_1ACC9] = (uint8_t)d2;
    img[A_DAT_1A572] |= (uint8_t)d2;
    scene_text_box(img);
    d1 = img[A_DAT_1AD25];
    if (d1 != 0) {
        run_text_table(img, a6, 0, d1);
        img[A_DAT_1AD25] = 0;
    }
    icon_table(img);
    hint_state_a(img, 0);
    hint_state_b(img);
    rot_action(img);
    update_icon_list(img);
    img[A_DAT_1AD0F] = 0;
    return a6;
}

static void body_146d2(uint8_t *img, uint32_t a6) {
    uint32_t a6b = a6;
    hint_blit_2(img, &a6b);
    hint_blit_stroke(img, &a6b);
    hint_blit_3(img, &a6b);
    if (img[A_DAT_1A572] == 0 && img[A_SKIP_TRANSITION] == 0)
        copy_rows_a(img);
    copy_screen(img);
    img[A_FRAME_1AB4F] = 0;
    sound_stop(img);
    if (img[A_DAT_1ACD2] == 0x16) {
        sound_play_a(img, 0x83);
        img[A_FRAME_1AB4F] = 1;
    }
    elem_mode_blit(img);
    set_palette_current(img);
    sample_find_body(img);
    icon_repeat(img);
    if (img[A_SEL_1ACC9] != 0)
        textform_rev(img, a6, img[A_SEL_1ACC9]);
    draw_hint_icons(img);
}

uint32_t scene_mode_draw(uint8_t *img, uint32_t a6) {
    a6 += 1;
    body_146d2(img, a6);
    return a6;
}

void scene_mode_draw_wrap(uint8_t *img, uint32_t a6) {
    a6 = scene_mode_apply(img, a6);
    body_146d2(img, a6);
}
