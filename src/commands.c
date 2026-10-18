/* commands.c — comandos de escena de la familia 0x151b0 (sin esperas).
 *
 * Todos empiezan con el umbral 0x14f24 (que puede saltarse el resto del llamante:
 * devuelve 2), fijan `the_command` ($1a50e) desde globals y despachan con
 * run_command_keep_sp (0x103ae). 0x15428 ademas imprime el nombre y espera tecla.
 */
#include <stdint.h>

#include "machine.h"
#include "addrs.h"

uint32_t anim_threshold(uint8_t *img, uint32_t a6, uint32_t d0);
void run_command_keep_sp(uint8_t *img, uint32_t a6);
uint32_t obj_wait_a0(uint8_t *img);
uint32_t text_index_arg(uint8_t *img, uint32_t a5, uint32_t d4, uint32_t *a6);
void text_window(uint8_t *img, uint32_t a5, uint32_t a6);
uint32_t prompt_wait_entry(uint8_t *img, uint32_t a6);
void hint_blit_stroke(uint8_t *img, uint32_t *a6);
void set_palette_entry(uint8_t *img, uint32_t d0);
void set_palette_current(uint8_t *img);
void icon_state_a(uint8_t *img);
void icon_state_b(uint8_t *img, uint32_t a6, uint32_t d0);
uint32_t anim_entry_b(uint8_t *img, uint32_t a6, uint32_t d0, uint32_t d1, uint32_t d2);
void supexec_b(uint8_t *img, uint32_t d1, uint32_t d4);
void sound_play_a(uint8_t *img, uint32_t d4);
void sound_stop(uint8_t *img);
void set_cursor_clip(uint8_t *img, uint32_t a0, uint32_t d0);
void obj_wait_hit(uint8_t *img, uint32_t d2, uint32_t d3);
void wait_vbl(uint8_t *img);
void wait_frame(uint8_t *img);
uint32_t elem_blit_vbl(uint8_t *img, uint32_t a6);
void print_char(uint8_t *img, uint32_t d0);
void hint_blit_2(uint8_t *img, uint32_t *a6);
void text_blit_ovl_b(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d4);
void masked_save_blit(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t d2, uint32_t d3,
                 uint32_t d7);
void mask_rows_handler(uint8_t *img);
void cursor_restore(uint8_t *img);
void overlay_b_draw(uint8_t *img, uint32_t a6);
void wait_frame_cursor(uint8_t *img);
void handle_frame_input(uint8_t *img);
int  rect_hit_test_a(uint8_t *img, uint32_t a4);
void clear_object_flags(uint8_t *img);
uint32_t screen_address(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t *d7out);

/* FUN_00015052 @ 0x15052: comando de seleccion (tabla 0x1ab62). */
uint32_t obj_action_select(uint8_t *img, uint32_t a6) {
    if (anim_threshold(img, a6, 1) == 2)
        return a6;
    if (img[0x1ad09] != 0) {
        wr16(img + 0x1a50e, be16(img + 0x1abfe));
        run_command_keep_sp(img, a6);
        return a6;
    }
    obj_wait_a0(img);
    if (img[0x1acc9] != 0) {
        uint32_t d0 = ((uint32_t)(img[0x1acc9] - 1) & 0xffu) * 0xau;
        wr16(img + 0x1a50e, be16(img + 0x1ab62 + d0 + 4));
        if (be16(img + 0x1a50e) != 0) {
            run_command_keep_sp(img, a6);
            return a6;
        }
    }
    wr16(img + 0x1a50e, be16(img + 0x1ab52));
    run_command_keep_sp(img, a6);
    return a6;
}

/* FUN_00014f74 @ 0x14f74 (cmd idx 10): rejilla de texto 0x16abe + espera de clic
 * (0x167be) + repintados, y despacho por la tabla de objetos o por el umbral
 * (0x9005) en la cola 0x15022. */
uint32_t grid_bands(uint8_t *img, uint32_t a6);
void copy_screen(uint8_t *img);
static void tail_15022(uint8_t *img);

/* 0x14f74 — grid wait */
uint32_t grid_wait(uint8_t *img, uint32_t a6) {
    if (anim_threshold(img, a6, 3) == 2)
        return a6;
    if (img[A_DAT_1AD09] != 0) {
        wr16(img + A_THE_COMMAND, be16(img + 0x1abfc));
        run_command_keep_sp(img, a6);
        return a6;
    }
    icon_state_a(img);
    a6 = grid_bands(img, a6);
    {
        uint32_t p = be32(img + A_PALETTE_PTR);
        if (be16(img + p + 2) != 0)
            set_palette_entry(img, 0x0f);
    }
    set_cursor_clip(img, 0, 2);
    obj_wait_hit(img, 0, 0);
    sound_play_a(img, 0xe0);
    copy_screen(img);
    {
        uint32_t p = be32(img + A_PALETTE_PTR);
        if (be16(img + p + 2) != 0)
            set_palette_current(img);
    }
    icon_state_b(img, a6, 0);
    if (img[A_SEL_1ACC9] != 0) {
        uint32_t d0 = ((uint32_t)(img[A_SEL_1ACC9] - 1) & 0xffu) * 0xau;
        wr16(img + A_THE_COMMAND, be16(img + A_DAT_1AB62 + d0 + 8));
        if (be16(img + A_THE_COMMAND) != 0) {
            tail_15022(img);
            return a6;
        }
    }
    wr16(img + A_THE_COMMAND, be16(img + 0x1ab50));
    tail_15022(img);
    return a6;
}

/* FUN_00014f5e @ 0x14f5e (cmd idx 9): umbral 2, ciclo de paleta 0x16310 y flag
 * $1ad0f. */
void palette_cycle(uint8_t *img);

/* 0x14f5e — anim threshold2 */
uint32_t anim_threshold2(uint8_t *img, uint32_t a6) {
    (void)anim_threshold(img, a6, 2);
    palette_cycle(img);
    img[A_DAT_1AD0F] = 0xff;
    return a6;
}

/* FUN_00014e66 @ 0x14e66 (cmd idx 1): menu de overlay: guarda el rect (0x1258a),
 * blitea el icono (0x11020 con d4=3) y gira esperando frame (0x13698) + hit-test de
 * acciones (0x147c2) + frame (0x1031c) hasta el clic; restaura cursor y rect
 * (0x1060a/0x124f2) y despacha (the_command queda a 0). */
void overlay_b_draw(uint8_t *img, uint32_t a6);
uint32_t save_rect(uint8_t *img, uint32_t d2, uint32_t d3, uint32_t a0, uint32_t d7);
void cursor_restore(uint8_t *img);
void restore_rect(uint8_t *img);
void wait_frame_cursor(uint8_t *img);
uint32_t cursor_select_a(uint8_t *img);
void wait_frame(uint8_t *img);
void set_cursor_clip(uint8_t *img, uint32_t a0, uint32_t d0);
void handle_frame_input(uint8_t *img);
uint32_t screen_address(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t *d7out);

/* 0x14e66 — overlay menu */
uint32_t overlay_menu(uint8_t *img, uint32_t a6) {
    wr16(img + A_THE_COMMAND, 0);
    uint32_t d7;
    uint32_t a0 = screen_address(img, 0x46, 0x28, &d7);
    (void)save_rect(img, 4, 0x80, a0, d7);
    text_blit_ovl_b(img, 0x46, 0x28, 3);
    wait_frame_cursor(img);
    for (;;) {
        wait_frame(img);
        set_cursor_clip(img, 0, 0);
        (void)cursor_select_a(img);
        uint32_t d0 = img[A_TEXT_SEL_B];
        if (d0 != 0x64) {
            d0 = (d0 + 0x6d) & 0xff;
            img[A_TEXT_SEL_B] = (uint8_t)d0;
        }
        if (d0 != img[A_1AB47])
            overlay_b_draw(img, a6);
        handle_frame_input(img);
        if (!(img[A_INPUT_BUTTONS] & 0x04))
            continue;
        break;
    }
    cursor_restore(img);
    restore_rect(img);
    run_command_keep_sp(img, a6);
    return a6;
}

/* FUN_00014ee8 @ 0x14ee8 (cmd idx 135): toma el parametro A6[1] de la entrada de
 * objeto seleccionada en la tabla 0x1ab62 (5 words, +2*param) o, si es 0, de la
 * tabla de reserva 0x1ab50, y despacha. */
uint32_t cmd135(uint8_t *img, uint32_t a6) {
    uint32_t d0 = ((uint32_t)(img[A_SEL_1ACC9] - 1) & 0xffu) * 0xau;
    uint32_t d1 = ((uint32_t)img[a6 + 1] << 1) & 0xffff;
    wr16(img + A_THE_COMMAND, be16(img + A_DAT_1AB62 + d0 + d1));
    if (be16(img + A_THE_COMMAND) != 0) {
        run_command_keep_sp(img, a6);
        return a6;
    }
    wr16(img + A_THE_COMMAND, be16(img + A_DAT_1AB50 + d1));
    run_command_keep_sp(img, a6);
    return a6;
}

/* FUN_000150ae @ 0x150ae: accion de objeto (tabla 0x1ab62 con stride 0xa, word en +2).
 * Con $1ad09 == 0x12 sale por rts sin tocar $1a772/`the_command`; si no, marca el
 * objeto seleccionado (bit7 de $1a772[1]), copia la seleccion a $1acd3/$1ace8 y, o
 * bien despacha 0x90aa, o cae en la salida 0x1513a (animacion 0x159e2 con d1=0x27 y
 * los parametros del objeto 0x1a81a, repintado 0x164f6 y despacho). */
static void tail_150ae(uint8_t *img, uint32_t a6) {
    uint32_t a0 = be32(img + A_SELECTED_OBJECT);
    uint32_t d0 = (uint32_t)img[a0] & 0xff;
    uint32_t d2 = ((uint32_t)img[a0 + 2] - 0xa) & 0xffff;
    d0 = (d0 + 2) & 0xffff;
    anim_entry_b(img, a6, d0, 0x27, d2);
    icon_state_b(img, a6, d0);
    run_command_keep_sp(img, a6);
}

/* 0x150ae — obj action */
uint32_t obj_action(uint8_t *img, uint32_t a6) {
    if (anim_threshold(img, a6, 2) == 2)
        return a6;
    if (img[A_DAT_1AD09] != 0)
        goto l150fe;
    icon_state_a(img);
    obj_wait_a0(img);
    if (img[A_SEL_1ACC9] == 0) {
        wr16(img + A_THE_COMMAND, be16(img + A_DAT_1AB5C));
        img[A_DAT_1ACD3] = 0;
        run_command_keep_sp(img, a6);
        return a6;
    }
    supexec_b(img, 0x32, 0x18);
    {
        uint32_t d0 = ((uint32_t)(img[A_SEL_1ACC9] - 1) & 0xffu) * 0xau;
        uint16_t cmd = be16(img + A_DAT_1AB62 + d0 + 2);
        wr16(img + A_THE_COMMAND, cmd);
        if (cmd != 0)
            goto l1513a;
    }
l150fe:
    if (img[A_DAT_1AD09] == 0x12) {
        img[A_DAT_1ACD3] = 1;
        img[A_DAT_1ACE8] = 2;
        return a6;                             /* el original sale por rts */
    }
    {
        uint32_t a0 = be32(img + A_ELEM_CLASS);
        img[a0 + 1] |= 0x80;
    }
    img[A_DAT_1ACD3] = img[A_SEL_1ACC9];
    img[A_DAT_1ACE8] = 2;
    wr16(img + A_THE_COMMAND, 0);
    if (img[A_DAT_1AD09] != 0) {
        wr16(img + A_THE_COMMAND, 0x90aa);
        run_command_keep_sp(img, a6);
        return a6;
    }
l1513a:
    tail_150ae(img, a6);
    return a6;
}

/* FUN_000152ee @ 0x152ee: accion de objeto con espera de clic (0x167be). Tras la
 * espera repinta (0x164d4), suena 0x19, anima con el cursor (0x159e2 d1=0x27) y
 * despacha la seleccion (entrada 0x1ab62+6) o el comando por defecto 0x1ab5a.
 * Con $1ad09 != 0 despacha 0x1ac00 directamente. */
uint32_t cursor_anim_wait(uint8_t *img, uint32_t a6) {
    if (anim_threshold(img, a6, 3) == 2)
        return a6;
    if (img[A_DAT_1AD09] != 0) {
        wr16(img + A_THE_COMMAND, be16(img + 0x1ac00));
        run_command_keep_sp(img, a6);
        return a6;
    }
    set_cursor_clip(img, 0, 4);
    obj_wait_hit(img, 0, 0);
    icon_state_a(img);
    sound_play_a(img, 0x19);
    {
        uint32_t d0 = (be16(img + A_CURSOR_X) >> 2) & 0xffff;
        uint32_t d2 = be16(img + A_CURSOR_Y);
        anim_entry_b(img, a6, d0, 0x27, d2);
        icon_state_b(img, a6, d0);
    }
    if (img[A_SEL_1ACC9] != 0) {
        uint32_t off = ((uint32_t)(img[A_SEL_1ACC9] - 1) & 0xffu) * 0xau;
        uint16_t cmd = be16(img + A_DAT_1AB62 + off + 6);
        wr16(img + A_THE_COMMAND, cmd);
        if (cmd != 0) {
            run_command_keep_sp(img, a6);
            return a6;
        }
    }
    wr16(img + A_THE_COMMAND, be16(img + A_DAT_1AB5A));
    run_command_keep_sp(img, a6);
    return a6;
}

/* FUN_00015428 @ 0x15428: imprime un nombre y espera tecla (Cnecin). D4 sale de
 * $1ac04 o, si $1ad09 == 0, de $1ab56 (override 0x113 cuando $1acec >= 0x3f y
 * $1acd2 < 0x16). Con el bit 15 de D4 es un comando y se despacha; si no,
 * posiciona con el banco 0x3c534, imprime (0x10b6c), espera (0x1228a) y blitea
 * los iconos 0x1232e. */
uint32_t name_prompt(uint8_t *img, uint32_t a6) {
    if (anim_threshold(img, a6, 4) == 2)
        return a6;
    uint32_t d4 = be16(img + 0x1ac04);
    if (img[A_DAT_1AD09] == 0) {
        d4 = be16(img + 0x1ab56);
        if (img[0x1acec] >= 0x3f && img[0x1acd2] < 0x16)
            d4 = 0x113;
    }
    if (d4 & 0x8000u) {
        wr16(img + A_THE_COMMAND, (uint16_t)d4);
        run_command_keep_sp(img, a6);
        return a6;
    }
    {
        uint32_t a5 = text_index_arg(img, 0x3c534, d4, &a6);
        text_window(img, a5, 0x1996c);
    }
    (void)prompt_wait_entry(img, 0x1996c);
    {
        uint32_t a6b = 0x1996d;              /* A6 ya avanzado por 0x1228a */
        hint_blit_stroke(img, &a6b);
    }
    return a6;
}

/* FUN_000151b0 @ 0x151b0: transicion de sala por filas: intercambia longs de la
 * pantalla (grupos de 2 longs, 0x10fec) y espera una VBL por fila (0x13594). En
 * cada fila busca el objeto de la clase 0x28 cuya fila coincide con D4, lo marca
 * (bit 7 de +4), suena (0x1b/0x1a) y lo anima (0x159e2 d1=0x26). Al final repinta
 * (0x164f6), para el sonido (0x18fa4), restaura la paleta y despacha $1ab54.
 * Con $1ad09 != 0 solo despacha $1ac08 sin tocar pantalla. */
uint32_t row_transition(uint8_t *img, uint32_t a6) {
    if (anim_threshold(img, a6, 1) == 2)
        return a6;
    wr16(img + A_THE_COMMAND, be16(img + 0x1ac08));
    if (img[A_DAT_1AD09] != 0) {
        run_command_keep_sp(img, a6);
        return a6;
    }
    icon_state_a(img);
    {
        uint32_t p = be32(img + A_PALETTE_PTR);
        if (be16(img + p + 2) != 0)
            set_palette_entry(img, 0x13);
    }
    sound_play_a(img, 0x1a);

    uint32_t d0 = (uint32_t)img[A_DAT_1AB40] & 0xff;
    uint32_t d1 = (uint32_t)img[A_DAT_1AB42] & 0xff;
    uint32_t d4 = d1;
    uint32_t d7;
    uint32_t a3 = screen_address(img, d0, d1, &d7);
    uint32_t d2 = ((((uint32_t)img[A_DAT_1AB41] - (uint32_t)img[A_DAT_1AB40]) & 0xffu) >> 2);
    d2 = (d2 - 1) & 0xffff;
    uint32_t d3 = ((uint32_t)img[A_DAT_1AB43] - (uint32_t)img[A_DAT_1AB42]) & 0xffu;
    d3 = (d3 - 1) & 0xffff;

    for (;;) {
        uint32_t a4 = a3;
        for (uint32_t d5 = d2;; d5 = (d5 - 1) & 0xffff) {
            uint32_t t = be32(img + a4);
            wr32(img + a4, be32(img + a4 + 4));
            wr32(img + a4 + 4, t);
            d0 = t;                            /* el swap deja D0 con el long movido */
            a4 += 8;
            if (d5 == 0)
                break;
        }
        wait_vbl(img);
        a4 = a3;
        for (uint32_t d5 = d2;; d5 = (d5 - 1) & 0xffff) {
            uint32_t t = be32(img + a4);
            wr32(img + a4, be32(img + a4 + 4));
            wr32(img + a4 + 4, t);
            d0 = t;
            a4 += 8;
            if (d5 == 0)
                break;
        }
        {
            uint32_t oa = be32(img + A_OBJECT_LIST_BEGIN);
            uint32_t oe = be32(img + A_OBJECT_LIST_END);
            for (;;) {
                d0 = (d0 & 0xffffff00u) | ((uint32_t)img[oa + 4] & 0xb8u);
                if ((img[oa + 4] & 0xb8u) == 0x28u && img[oa + 2] == (uint8_t)d4) {
                    img[oa + 4] |= 0x80;
                    sound_play_a(img, 0x1b);
                    d0 = (d0 & 0xffff0000u) | ((uint32_t)img[oa] & 0xff);
                    uint32_t d2f = (d2 & 0xffff0000u) | ((uint32_t)img[oa + 2] & 0xff);
                    anim_entry_b(img, a6, d0, 0x26, d2f);
                    sound_play_a(img, 0x1a);
                    break;                     /* el original salta al cierre de fila */
                }
                oa += 8;
                if (oa == oe)
                    break;
            }
        }
        a3 += 0xa0;
        d4 = (d4 + 1) & 0xff;                  /* addq.b #1,d4 */
        d3 = (d3 - 1) & 0xffff;
        if (d3 == 0xffff)
            break;
    }

    icon_state_b(img, a6, d0);
    sound_stop(img);
    {
        uint32_t p = be32(img + A_PALETTE_PTR);
        if (be16(img + p + 2) != 0)
            set_palette_current(img);
    }
    wr16(img + A_THE_COMMAND, be16(img + A_DAT_1AB54));
    run_command_keep_sp(img, a6);
    return a6;
}

/* FUN_00015594 @ 0x15594 (cmd idx 36) y FUN_000155c4 @ 0x155c4 (idx 37): cortan el
 * sonido (0x18fa4) y cargan/guardan la sala (0x16ed8/0x16fb4). El estado sale por
 * CCR (carry = error): si falla despachan 0x918f/0x9190 y si no 0x90aa. */
int32_t load_scena(uint8_t *img);
int32_t save_scena(uint8_t *img);

/* 0x15594 — load room */
uint32_t load_room(uint8_t *img) {
    sound_stop(img);
    int32_t status = load_scena(img);
    wr16(img + A_THE_COMMAND, status ? 0x918f : 0x90aa);
    run_command_keep_sp(img, 0);
    return 0;
}

/* 0x155c4 — save room */
uint32_t save_room(uint8_t *img) {
    sound_stop(img);
    int32_t status = save_scena(img);
    wr16(img + A_THE_COMMAND, status ? 0x9190 : 0x90aa);
    run_command_keep_sp(img, 0);
    return 0;
}

/* FUN_000155dc @ 0x155dc (cmd idx 7): reloj. Inicializa el cursor (0x134da con
 * A6 = 0x19710) y repite: cursor de texto a (0x41,0x78) y base de dibujo a 0,
 * espera VBL (0x13594), imprime HH:MM:SS desde $1ab60 con 0x10838 (el separador
 * 0x1a solo si el resto es impar), espera frame (0x13698) y sale con el clic
 * (bit 2 de $1a456); al final restaura los iconos (0x12368). */
uint32_t clock(uint8_t *img) {
    uint32_t a6 = elem_blit_vbl(img, 0x19710);
    for (;;) {
        wr16(img + A_CURSOR_TEXT_X, 0x41);
        wr16(img + A_CURSOR_TEXT_Y, 0x78);
        wr32(img + A_SCREEN_DRAW_BASE, 0);
        wait_vbl(img);
        uint32_t t = be16(img + A_DAT_1AB60);
        uint32_t rem = t % 0xe10;
        print_char(img, ((t / 0xe10) + 0x10) & 0xff);
        print_char(img, (rem & 1) ? 0x1a : 0);
        print_char(img, ((rem / 0x258) + 0x10) & 0xff);
        print_char(img, (((rem % 0x258) / 0x3c) + 0x10) & 0xff);
        wait_frame(img);
        if (!(img[A_INPUT_BUTTONS] & 0x04))
            continue;
        break;
    }
    hint_blit_2(img, &a6);
    return a6;
}

/* FUN_0001566a @ 0x1566a: anillo de iconos de accion. A6[1] es una mascara de 8
 * bits; por cada bit dibuja un rect [x0,x0+2,y0,y0+8] en 0x19ad6 (base $1a575/
 * $1a576 y tabla 0x19af6) y lo blitea (0x11020 con d4=1). Luego repite: espera
 * frame (0x13698), hit-test de los rects (0x1478a), publica el comando de 3 bytes
 * del script en $1ab46/`the_command`, procesa el frame (0x1031c); con clic
 * restaura el cursor (0x1060a), reblitea el icono pulsado (0x18f8e + 3x 0x11020)
 * y cierra con 0x12436. Devuelve A6 avanzado 2 + 3*(nº de bits). */
uint32_t icon_ring(uint8_t *img, uint32_t a6) {
    img[A_1AB44] = img[A_TEXT_SEL_A];
    a6 += 1;
    uint32_t a5 = a6;
    if (img[0x1a575] == 0xff) {
        uint32_t d0 = (be16(img + A_CURSOR_X) >> 2) & 0xffff;
        uint32_t d1 = d0;
        uint32_t t = (d0 + 0x0c) & 0xffff;
        t = (t - 0x4a) & 0xffff;
        if ((d0 + 0x0c) >= 0x4a)
            d1 = (d1 - t) & 0xffff;
        if ((d1 & 0xff) < 8)
            d1 = 8;
        img[0x1a575] = (uint8_t)d1;
        uint32_t y = be16(img + A_CURSOR_Y);
        uint32_t d2 = (y + 0x2d) & 0xffff;
        d2 = (d2 - 0xb4) & 0xffff;
        if ((y + 0x2d) >= 0xb4)
            y = (y - d2) & 0xffff;
        img[0x1a576] = (uint8_t)y;
    }
    {
        uint32_t d1 = (uint32_t)img[0x1a576];
        uint32_t d0 = (uint32_t)img[0x1a575];
        uint32_t a1 = 0x28ad4;
        uint32_t d3 = img[a1++];
        uint32_t d2 = img[a1++];
        wr32(img + A_DAT_1A534, (d2 << 1) & 0xffff);
        uint32_t d7;
        uint32_t a0 = screen_address(img, d0, d1, &d7);
        masked_save_blit(img, a0, a1, d2, d3, d7);
    }
    sound_play_a(img, 0x12);

    uint32_t a4 = 0x19ad6;
    uint32_t a1 = 0x19af6;
    uint32_t d2 = (uint32_t)img[0x1a575];
    uint32_t d3 = (uint32_t)img[0x1a576];
    uint32_t count = 0xffff;                    /* d4: nº de bits - 1 */
    uint32_t mask = img[a5++];
    for (uint32_t d6 = 7;; d6 = (d6 - 1) & 0xffff) {
        uint32_t bit = mask & 1;
        mask >>= 1;
        if (bit) {
            uint32_t x0 = (d2 + (uint32_t)img[a1]) & 0xff;
            img[a4] = (uint8_t)x0;
            img[a4 + 1] = (uint8_t)(x0 + 2);
            uint32_t y0 = (d3 + (uint32_t)img[a1 + 1]) & 0xff;
            img[a4 + 2] = (uint8_t)y0;
            img[a4 + 3] = (uint8_t)(y0 + 8);
            count = (count + 1) & 0xffff;
            a4 += 4;
        }
        a1 += 2;
        if (d6 == 0)
            break;
    }
    a4 = 0x19ad6;
    for (uint32_t i = 0; i <= count; i++) {
        uint32_t d0 = (uint32_t)img[a4];
        uint32_t d1 = (uint32_t)img[a4 + 2];
        text_blit_ovl_b(img, d0, d1, 1);
        a4 += 4;
    }
    set_cursor_clip(img, 0, 0);
    wait_frame_cursor(img);

    for (;;) {
        wait_frame(img);
        uint32_t a2 = a5 - 3;
        uint32_t d3c = count;
        a4 = 0x19ad2;
        for (;;) {
            a4 += 4;
            a2 += 3;
            if (rect_hit_test_a(img, a4))
                break;
            if (d3c == 0) {
                d3c = 0xffff;
                break;
            }
            d3c = (d3c - 1) & 0xffff;
        }
        uint32_t d0 = 0x64;
        uint32_t d1 = 0xffff;
        img[A_1AB48A] = 1;
        if (d3c != 0xffff) {
            img[A_1AB48A] = 0;
            d0 = (uint32_t)img[a2];
            d1 = (((uint32_t)img[a2 + 1] << 8) | (uint32_t)img[a2 + 2]) & 0xffff;
        }
        img[A_TEXT_SEL_B] = (uint8_t)d0;
        wr16(img + A_THE_COMMAND, d1);
        if (img[A_TEXT_SEL_B] != img[A_1AB47])
            overlay_b_draw(img, a6);
        handle_frame_input(img);
        if (!(img[A_INPUT_BUTTONS] & 0x04))
            continue;
        break;
    }
    cursor_restore(img);
    if (be16(img + A_THE_COMMAND) != 0xffff) {
        wait_vbl(img);
        sound_play_a(img, 0x13);
        uint32_t d0 = (uint32_t)img[a4];
        uint32_t d1 = (uint32_t)img[a4 + 2];
        text_blit_ovl_b(img, d0, d1, 0x18);
        text_blit_ovl_b(img, d0, d1, 0x02);
        text_blit_ovl_b(img, d0, d1, 0x19);
    }
    mask_rows_handler(img);
    return (a6 + 1 + 3 * ((count + 1) & 0xffff)) & 0xffffffffu;
}

/* FUN_00015888 @ 0x15888 (cmd idx 98): bucle del anillo de 0x1566a. Guarda el fin
 * de plantilla ($1a4e0) y el script (A6), y repite 0x1566a; con `the_command` a
 * 0xffff (sin acierto) restaura el fin de plantilla y sale. Los comandos con
 * nibble 0xa/0xc y el pendiente $1abf8 cortan el bucle y despachan (el original
 * hace `addq.l #4,a7` + `bra 0x103ae`, asi que el despacho sustituye al bucle).
 * Si no, despacha, cuenta $1ace7 y, tras pasar el 0x2f3c de 0x19316, comprueba el
 * umbral $1acf2/$1ace8 (con 0x15192 al agotarse). */
uint32_t icon_ring_loop(uint8_t *img, uint32_t a6) {
    wr32(img + 0x1a4be, be32(img + A_TEMPL_PTR));
    wr32(img + 0x1a4c2, a6);
    img[0x1a575] = 0xff;
    for (;;) {
        do {
            a6 = icon_ring(img, a6);
            if (be16(img + A_THE_COMMAND) == 0xffff) {
                wr32(img + A_TEMPL_PTR, be32(img + 0x1a4be));
                return a6;
            }
            uint32_t nib = be16(img + A_THE_COMMAND) & 0xf000u;
            if (nib == 0xc000u || nib == 0xa000u) {
                run_command_keep_sp(img, a6);
                return a6;
            }
            run_command_keep_sp(img, a6);
            img[A_DAT_1ACE7] = (uint8_t)(img[A_DAT_1ACE7] + 1);
        } while (be16(img + 0x19316) != 0x2f3c);
        if (img[A_DAT_1AD09] == 0 && img[A_DAT_1ACF2] < img[A_DAT_1ACE7]) {
            wr16(img + A_THE_COMMAND, be16(img + A_DAT_1ABF8));
            if (be16(img + A_THE_COMMAND) != 0) {
                run_command_keep_sp(img, a6);
                return a6;
            }
        }
        a6 = be32(img + 0x1a4c2);
        img[A_DAT_1ACE8] = (uint8_t)(img[A_DAT_1ACE8] - 1);
        if (img[A_DAT_1ACE8] == 0)
            clear_object_flags(img);
    }
}

/* FUN_00015382 @ 0x15382: comando de seleccion con paleta 0x11 y modo 0x9000. */
uint32_t palette_cmd(uint8_t *img, uint32_t a6) {
    if (anim_threshold(img, a6, 8) == 2)
        return a6;
    img[0x1ad03] = 1;
    {
        uint32_t a0 = be32(img + 0x1a466);
        if (be16(img + a0 + 2) != 0)
            set_palette_entry(img, 0x11);
    }
    if (img[0x1ad09] != 0) {
        wr16(img + 0x1a50e, be16(img + 0x1ac02));
        run_command_keep_sp(img, a6);
        return a6;
    }
    obj_wait_a0(img);
    if (img[0x1acc9] != 0) {
        uint32_t d0 = ((uint32_t)(img[0x1acc9] - 1) & 0xffu) * 0xau;
        uint32_t d1 = be16(img + 0x1ab62 + d0 + 8);
        wr16(img + 0x1a50e, (uint16_t)d1);
        d1 &= 0xf000u;
        if (d1 == 0x9000u)
            goto restore;
        if (be16(img + 0x1a50e) != 0) {
            run_command_keep_sp(img, a6);
            return a6;
        }
    }
    wr16(img + 0x1a50e, be16(img + 0x1ab58));
restore:
    {
        uint32_t a0 = be32(img + 0x1a466);
        if (be16(img + a0 + 2) != 0)
            set_palette_current(img);
    }
    img[0x1ad03] = 0;
    run_command_keep_sp(img, a6);
    return a6;
}

/* FUN_00015022 (codelet compartido): despacha; si el modo es valido y no es 1,
 * convierte en el comando 0x9005. */
static void tail_15022(uint8_t *img) {
    if (img[0x1acec] < 0x3f && img[0x1acd2] < 0x16 && img[0x1acd2] != 1)
        wr16(img + 0x1a50e, 0x9005);
    run_command_keep_sp(img, 0);
}

/* FUN_00015530 @ 0x15530: comando de modo/temporizador (dos despachos y cola 0x15022). */
uint32_t mode_queue(uint8_t *img, uint32_t a6) {
    img[0x1aceb] = (uint8_t)(img[0x1aceb] + 1);
    wr16(img + 0x1ab60, (uint16_t)(be16(img + 0x1ab60) + 0x12c));
    wr16(img + 0x1a50e, be16(img + 0x1acaa));
    run_command_keep_sp(img, a6);
    wr16(img + 0x1a50e, be16(img + 0x1acae));
    run_command_keep_sp(img, a6);
    img[0x1ace7] = img[0x1acf2];
    wr16(img + 0x1a50e, be16(img + 0x1ab5e));
    if (be16(img + 0x1a50e) != 0) {
        tail_15022(img);
        return a6;
    }
    if (be16(img + 0x1abf8) != 0)
        return a6;
    wr16(img + 0x1a50e, 0x9005);
    run_command_keep_sp(img, a6);
    return a6;
}
