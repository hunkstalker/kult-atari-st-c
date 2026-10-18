/* select.c — seleccion de samples y su ventana (0x15492/0x15510).
 *
 * 0x15492 recorre la tabla de 0x3f entradas de samples (0x1a8f0, 6 B) y para cada
 * una cuyo word cumple `(word & D2) == D3` copia el registro a 0x1a6ae+4/6/8
 * (nombre, longitud y color 0x3f-indice) y lo dibuja con 0x15510. La primera
 * coincidencia abre la ventana (0x1276c), arranca el sample (0x18f8e con D4=0x14)
 * y toma el flag de $1a488; al final publica el contador en $1a510.
 */
#include <stdint.h>

#include "machine.h"
#include "addrs.h"

uint32_t screen_address(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t *d7out);
void pattern_fill(uint8_t *img, uint32_t d2w, uint32_t d3w, uint32_t d7w,
                 uint32_t d6w, uint32_t a0);
void sound_play_a(uint8_t *img, uint32_t d4);
void text_blit_ovl_b(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d4);

/* FUN_00015510 @ 0x15510: imprime el registro A2 (nombre en A2[0], fila en A2[2])
 * con 0x11020 y D4; avanza A2 0xa y devuelve D6 incrementado (el original lo hace
 * en los registros, y 0x15492 encadena registros de 10 bytes). */
uint32_t sample_select2(uint8_t *img, uint32_t *reg_io, uint32_t index, uint32_t count) {
    uint32_t reg = *reg_io;
    uint32_t name = img[reg];
    uint32_t row = img[reg + 2];
    text_blit_ovl_b(img, name, row, index);
    *reg_io = (reg + 0xa) & 0xffffffffu;
    return (count + 1) & 0xffff;
}

/* FUN_00015492 @ 0x15492: recorre la tabla 0x1a8f0 y publica el registro elegido.
 * Devuelve D6 (el contador que el original deja en el registro y en $1a510). */
uint32_t sample_select(uint8_t *img, uint32_t mask, uint32_t value) {
    uint32_t i = 0x3e;
    uint32_t entry = 0x1a8f0;
    uint32_t reg = 0x1a6ae;
    uint32_t count = 0;
    do {
        uint32_t match = be16(img + entry) & (mask & 0xffff);
        if ((match & 0xffff) == (value & 0xffff)) {
            if (count == 0) {
                /* El `movem` del original restaura D6 tras la ventana: el valor de
                 * $1a488 solo entra en el blit, no queda en el contador. */
                uint32_t win = be16(img + 0x1a488);
                uint32_t phase = 0;
                uint32_t addr = screen_address(img, 0x3a, 0x38, &phase);
                pattern_fill(img, 0x10, 0x40, phase, win, addr);
                sound_play_a(img, 0x14);
            }
            img[reg + 4] = img[entry + 3];
            wr16(img + reg + 6, be16(img + entry + 4));
            img[reg + 8] = (uint8_t)((0x3f - (i & 0xff)) & 0xff);
            count = sample_select2(img, &reg, img[entry + 2], count);
        }
        entry += 6;
    } while ((i = (i - 1) & 0xffff) != 0xffff);
    wr16(img + 0x1a510, (uint16_t)count);
    return count;
}

/* ================= menu de samples: 0x14db2 y sus entradas =================
 *
 * 0x14db2 guarda el marco (0x1258a), pinta la lista (0x15492) y, si hay samples,
 * entra en el bucle de espera: frame + cursor (0x10522), paquete IKBD (0x13698),
 * hit-test de la lista (0x14a98 + 0x14966), nombre seleccionado (0x10a88) y frame
 * (0x1031c) hasta el clic (bit 2 de $1a456). Cierra restaurando el marco (0x124f2),
 * arranca el Timer A (0x18f98 con D4=0x14) y fija $1ad29 segun el nombre elegido.
 * 0x14d76/0x14d90/0x149da son las entradas que fijan mascara/filtro y despachan. */

uint32_t save_rect(uint8_t *img, uint32_t d2, uint32_t d3, uint32_t a0, uint32_t d7);
void restore_rect(uint8_t *img);
void cursor_restore(uint8_t *img);
void sound_play_b(uint8_t *img, uint32_t d4);
void wait_frame_cursor(uint8_t *img);
void wait_frame(uint8_t *img);
void set_cursor_clip(uint8_t *img, uint32_t a0, uint32_t d0);
void action_bar_hit_test(uint8_t *img, uint32_t d3);
void overlay_b_draw(uint8_t *img, uint32_t a6);
void handle_frame_input(uint8_t *img);
void text_end(uint8_t *img);
void run_command_keep_sp(uint8_t *img, uint32_t a6);

/* FUN_00014db2 @ 0x14db2: marco + lista de samples + bucle de espera + Timer A. */
void sample_menu(uint8_t *img, uint32_t script, uint32_t mask, uint32_t value) {
    wr16(img + 0x1a50e, 0);
    uint32_t phase = 0;
    uint32_t addr = screen_address(img, 0x3a, 0x38, &phase);
    save_rect(img, 0x10, 0x40, addr, phase);
    sample_select(img, mask, value);
    if (be16(img + 0x1a510) != 0) {
        wait_frame_cursor(img);
        for (;;) {
            wait_frame(img);
            set_cursor_clip(img, 0, 0);
            action_bar_hit_test(img, be16(img + 0x1a510));
            if (img[0x1ab46] != img[0x1ab47])
                overlay_b_draw(img, script);
            handle_frame_input(img);
            if (!(img[0x1a456] & 0x04u))
                continue;
            cursor_restore(img);
            break;
        }
    }
    restore_rect(img);
    sound_play_b(img, 0x14);
    img[0x1ad29] = 0x01;
    {
        uint32_t sel = be32(img + 0x1a75e);
        uint8_t c = img[sel + 3];
        if (c != 0x6c && c != 0x73 && c != 0x75)
            img[0x1ad29] = 0x00;
    }
}

/* FUN_00014d76 @ 0x14d76: entrada del menu (D2/D3 = 0x8000, filtro 0x00ff). */
void sample_menu_entry_a(uint8_t *img, uint32_t script) {
    cursor_restore(img);
    wr16(img + 0x1a488, 0x00ff);
    sample_menu(img, script, 0x8000, 0x8000);
    run_command_keep_sp(img, script);
}

/* FUN_00014d90 @ 0x14d90: entrada del menu con el filtro de $1acd2. */
void sample_menu_entry_b(uint8_t *img, uint32_t script) {
    cursor_restore(img);
    uint32_t value = 0x4000u | img[0x1acd2];
    wr16(img + 0x1a488, 0x000f);
    sample_menu(img, script, 0x40ff, value);
    run_command_keep_sp(img, script);
}

/* FUN_000149da @ 0x149da: entrada del menu de acciones; devuelve el A6 avanzado (el
 * original lo deja en el registro). */
uint32_t action_menu(uint8_t *img, uint32_t script) {
    if (img[0x1acec] >= 0x3f) {
        text_end(img);
        return script;
    }
    uint32_t saved = be32(img + 0x1a4e0);
    wr16(img + 0x1a488, 0xf0ff);
    sample_menu(img, script, 0xff00, 0x8000);
    uint32_t state = 1;
    if (be16(img + 0x1a510) == 0)
        goto stored;
    state++;
    if (be16(img + 0x1a50e) == 0)
        goto stored;
    state++;
    if (img[0x1ace9] < 6)
        goto stored;
    if (img[0x1ace9] >= 0x1b)
        goto stored;
    wr16(img + 0x1a50e, 0xc204);
    run_command_keep_sp(img, script);
    {
        uint32_t sel = be32(img + 0x1a75e);
        img[sel] = 0;
    }
    sample_menu(img, script, 0xff00, 0x1000);
    state = 4;
    if (be16(img + 0x1a50e) == 0)
        goto stored;
    state++;
    if (img[0x1acd7] >= 0x80)
        goto stored;
    state++;
    {
        uint32_t sel = be32(img + 0x1a75e);
        img[sel - 6] = 0x10;
        img[sel] = 0;
    }
stored:
    img[0x1ad0e] = (uint8_t)state;
    wr32(img + 0x1a4e0, saved);
    return script + 1;
}

/* FUN_00014ac6 @ 0x14ac6: menu de samples (variante principal). Corre 0x14db2
 * (marco+lista+espera de clic+Timer A) y segun $1a510/$1a50e y el registro
 * 0x1a75e contra 0x1a8f0+(idx-1)*6 fija el comando en 0x1a50e
 * (0xc1bc/0xc1c0/0x9140/c04b/c1ba/c1bb/c1b9/c1bd) y lo despacha (0x103ae); el
 * 0xc1bd repite el bucle. Cierra con 0x14bdc y devuelve A6 = entry+1. */
void run_command_keep_sp(uint8_t *img, uint32_t a6);
void fix_sample_flag(uint8_t *img);
void sample_menu(uint8_t *img, uint32_t a6, uint32_t d2, uint32_t d3);

/* 0x14ac6 — Un handler de byte de la VM (tabla 0x19f62): recibe a6 y devuelve el a6 nuevo. El avance de A6 es el neto del handler original (los `addq`/push-pop), dato qu... */
uint32_t sample_menu_a(uint8_t *img, uint32_t script) {
    uint32_t saved = be32(img + 0x1a4e0);
    uint32_t mask = 0xfe00, value = 0x8000;
    wr16(img + 0x1a488, 0xf0ff);
    for (;;) {
        sample_menu(img, script, mask, value);
        if (be16(img + 0x1a510) == 0) {
            wr16(img + 0x1a50e, 0xc1bc);
            goto L_14ac6_d58;
        }
        if (be16(img + 0x1a50e) == 0) {
            wr16(img + 0x1a50e, 0xc1c0);
            goto L_14ac6_d58;
        }
        {
            uint32_t entry = be32(img + 0x1a812);
            wr16(img + 0x1a50e, 0x9140);
            uint32_t n = img[entry + 4];
            if (n == 0)
                goto L_14ac6_d5c;
            n = ((n - 1) & 0xff) * 6;
            uint32_t slot = 0x1a8f0 + n;
            uint32_t reg = be32(img + 0x1a75e);
            if (img[reg] == 0x81) {
                wr16(img + 0x1a50e, 0xc1c0);
                goto L_14ac6_d58;
            }
            uint32_t cur3 = img[reg + 3];
            if (cur3 == img[slot + 3]) {
                wr16(img + 0x1a50e, 0xc1c0);
                goto L_14ac6_d58;
            }
            if (cur3 == 0x6d || cur3 == 0x84 || cur3 == 0x6c)
                goto L_14ac6_b8a;
            if (img[0x1acd7] >= 0x9a) {
                img[reg] = 0x81;
                wr16(img + 0x1a50e, 0xc1bd);
                run_command_keep_sp(img, script);
                continue;
            }
        L_14ac6_b8a:
            img[reg] = 0x20;
            img[slot] = 0x80;
            img[entry + 4] = img[0x1ace9];
            wr16(img + 0x1a50e, 0xc04b);
            if (img[reg + 3] == 0x84)
                goto L_14ac6_d6e;
            wr16(img + 0x1a50e, 0xc1ba);
            if (img[slot + 3] == 0x68)
                goto L_14ac6_d58;
            wr16(img + 0x1a50e, 0xc1bb);
            if (img[slot + 3] == 0x6b)
                goto L_14ac6_d58;
            wr16(img + 0x1a50e, 0xc1b9);
            goto L_14ac6_d58;
        }
    L_14ac6_d58:
        run_command_keep_sp(img, script);
    L_14ac6_d5c:
        fix_sample_flag(img);
        wr32(img + 0x1a4e0, saved);
        return script + 1;
    L_14ac6_d6e:
        img[0x1acf7] = (uint8_t)(img[0x1acf7] - 1);
        goto L_14ac6_d58;
    }
}

/* FUN_00014bfa @ 0x14bfa: variante del menu de samples (comando inicial 0x9099).
 * Como 0x14ac6 pero exige que la entrada elegida este en {0x68,0x7a,0x6b,0x69},
 * corre 0xc1c6 (que puede fijar $1ad04) y comparte las colas. A6 = entry+1. */
uint32_t sample_menu_b(uint8_t *img, uint32_t script) {
    uint32_t saved = be32(img + 0x1a4e0);
    uint32_t mask = 0xfe00, value = 0x8000;
    wr16(img + 0x1a50e, 0x9099);
    run_command_keep_sp(img, script);
    wr16(img + 0x1a488, 0xf0ff);
    for (;;) {
        sample_menu(img, script, mask, value);
        if (be16(img + 0x1a510) == 0) {
            wr16(img + 0x1a50e, 0xc1c5);
            goto L_14bfa_d58;
        }
        if (be16(img + 0x1a50e) == 0)
            goto L_14bfa_d5c;
        {
            uint32_t entry = be32(img + 0x1a812);
            wr16(img + 0x1a50e, 0x9140);
            uint32_t n = img[entry + 4];
            if (n == 0)
                goto L_14bfa_d58;
            n = ((n - 1) & 0xff) * 6;
            uint32_t slot = 0x1a8f0 + n;
            uint32_t reg = be32(img + 0x1a75e);
            if (img[reg] == 0x81) {
                wr16(img + 0x1a50e, 0xc1c0);
                goto L_14bfa_d58;
            }
            uint32_t b3 = img[slot + 3];
            if (!(b3 == 0x68 || b3 == 0x7a || b3 == 0x6b || b3 == 0x69))
                goto L_14bfa_d58;
            uint32_t cur3 = img[reg + 3];
            if (cur3 == b3)
                goto L_14bfa_cc6;
            if (!(cur3 == 0x6d || cur3 == 0x6c || cur3 == 0x84)) {
                if (img[0x1acd7] < 0x9a)
                    goto L_14bfa_ce6;
            }
        L_14bfa_cc6:
            img[reg] = 0x81;
            wr16(img + 0x1a50e, 0xc1bd);
            run_command_keep_sp(img, script);
            continue;
        L_14bfa_ce6:
            img[0x1ad04] = 0;
            wr16(img + 0x1a50e, 0xc1c6);
            run_command_keep_sp(img, script);
            if (img[0x1ad04] == 0)
                goto L_14bfa_d5c;
            img[reg] = 0x20;
            img[slot] = 0x80;
            img[entry + 4] = img[0x1ace9];
            wr16(img + 0x1a50e, 0xc04b);
            if (img[reg + 3] == 0x84)
                goto L_14bfa_d6e;
            wr16(img + 0x1a50e, 0xc1ba);
            if (img[slot + 3] == 0x68)
                goto L_14bfa_d58;
            wr16(img + 0x1a50e, 0xc1bb);
            if (img[slot + 3] == 0x6b)
                goto L_14bfa_d58;
            wr16(img + 0x1a50e, 0xc1b9);
            goto L_14bfa_d58;
        }
    L_14bfa_d58:
        run_command_keep_sp(img, script);
    L_14bfa_d5c:
        fix_sample_flag(img);
        wr32(img + 0x1a4e0, saved);
        return script + 1;
    L_14bfa_d6e:
        img[0x1acf7] = (uint8_t)(img[0x1acf7] - 1);
        goto L_14bfa_d58;
    }
}
