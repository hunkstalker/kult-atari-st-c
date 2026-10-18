/* icons.c — dibujo/restauración de los iconos de pista (tabla 0x1a776).
 *
 * draw_hint_icons @ 0x16556: blitea los 16 descriptores de la tabla con blit_sprite.
 * FUN_00016514 @ 0x16514 + FUN_0001652e @ 0x1652e: restaura esas 16 zonas con 0x12190.
 * FUN_000164d4 @ 0x164d4 y FUN_000164f6 @ 0x164f6: despachadores por el modo $1acc7.
 */
#include <stdint.h>

#include "machine.h"
#include "addrs.h"

void blit_sprite(uint8_t *img, uint32_t a1);
void mask_row_blit(uint8_t *img, uint32_t a0, uint32_t d2, uint32_t d3, uint32_t d7);
void copy_screen_rev(uint8_t *img);
void run_text_table(uint8_t *img, uint32_t a6, uint32_t d0, uint32_t d1);
void box_vline(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3);
void box_hline(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3);
uint32_t save_rect_body(uint8_t *img, uint32_t a1, uint32_t d2, uint32_t d3, uint32_t a0,
                     uint32_t d7);
uint32_t screen_address(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t *d7out);

/* FUN_000164a4 @ 0x164a4: guarda el rect de pantalla del objeto A5 en el descriptor
 * A1, enlaza A1 en la tabla de iconos (slot A2) y devuelve el nuevo A1 (fin del
 * buffer). La fase D7 la calcula 0x10fec (fase de pixel); A1/A2/A5 los hila 0x16460.
 * El original salva A2 en la pila alrededor de 0x12590 porque ese cuerpo machaca A2;
 * en C A2 es local del llamante. */
uint32_t icon_table_entry(uint8_t *img, uint32_t a1, uint32_t a2, uint32_t a5) {
    wr32(img + a2, a1);
    uint32_t d0 = img[a5];
    uint32_t d1 = img[a5 + 2];
    uint32_t d2 = (uint32_t)((img[a5 + 1] - d0) & 0xff);
    uint32_t d3 = (uint32_t)((img[a5 + 3] - d1) & 0xff);
    uint32_t d7;
    uint32_t a0 = screen_address(img, d0, d1, &d7);
    a0 -= 0x8000;
    return save_rect_body(img, a1, d2, d3, a0, d7);
}

/* FUN_00016460 @ 0x16460: reconstruye la tabla de iconos de pista (0x1a776, 16
 * entradas): la pone a 0 y, por cada objeto con bit6 en byte4, guarda su rect en
 * buffers consecutivos desde 0x3a556 (A1) y enlaza el descriptor en el slot. */
void icon_table(uint8_t *img) {
    uint32_t a1 = 0x3a556;
    uint32_t a2 = A_COMMAND_HINTS_LIST;
    for (int i = 0; i <= 0xf; i++) {
        wr32(img + a2, 0);
        a2 += 4;
    }
    a2 = A_COMMAND_HINTS_LIST;
    uint32_t a5 = be32(img + A_OBJECT_LIST_BEGIN);
    uint32_t end = be32(img + A_OBJECT_LIST_END);
    do {
        if (img[a5 + 4] & 0x40)
            a1 = icon_table_entry(img, a1, a2, a5);
        a2 += 4;
        a5 += 8;
    } while (a5 != end);
}

void icon_apply(uint8_t *img, uint32_t a2) {
    if (be32(img + a2) == 0)
        return;
    uint32_t a1 = be32(img + a2);
    uint32_t d2 = img[a1++];
    uint32_t d3 = img[a1++];
    uint32_t a0 = be32(img + a1);
    a1 += 4;
    uint32_t d7 = be16(img + a1);
    a1 += 2;
    (void)a1;                                        /* a3 = (a1)+, no lo usa 0x12190 */
    mask_row_blit(img, (a0 + 0x8000) & 0xffffffffu, d2, d3, d7);
}

void hint_icons(uint8_t *img) {
    uint32_t a2 = 0x1a776;
    for (uint32_t d0 = 0x0f; ; d0 = (d0 - 1) & 0xffff) {
        icon_apply(img, a2);
        a2 += 4;
        if (d0 == 0) break;
    }
}

void draw_hint_icons(uint8_t *img) {
    uint32_t a2 = 0x1a776;
    for (uint32_t d0 = 0x0f; ; d0 = (d0 - 1) & 0xffff) {
        blit_sprite(img, be32(img + a2));
        a2 += 4;
        if (d0 == 0) break;
    }
}

/* FUN_000164d4 @ 0x164d4: copia front->back si el modo $1acc7 es 0x29 o está en [0x16,0x19). */
void icon_state_a(uint8_t *img) {
    uint32_t d0 = img[0x1acc7];
    if (d0 == 0x29 || (d0 >= 0x16 && d0 < 0x19))
        copy_screen_rev(img);
}

/* FUN_000164f6 @ 0x164f6: si el modo $1acc7 está en [0x16,0x19) recorre ARPLA (0x13f56). */
void icon_state_b(uint8_t *img, uint32_t a6, uint32_t d0) {
    uint32_t d1 = img[0x1acc7];
    if (d1 >= 0x16 && d1 < 0x19)
        run_text_table(img, a6, d0, d1);
}

/* FUN_00018ce0 @ 0x18ce0: recorre D0+1 entradas de 6 bytes en A0; si byte0 == 0x80 lo
 * pasa a 0x40 y copia $1acd2 a byte1. */
void icon_anim_c(uint8_t *img, uint32_t a0, uint32_t d0) {
    for (;;) {
        if (img[a0] == 0x80) {
            img[a0] = 0x40;
            img[a0 + 1] = img[0x1acd2];
        }
        a0 += 6;
        d0 = (d0 - 1) & 0xffff;
        if (d0 == 0xffff) break;
    }
}

/* FUN_00018c8e @ 0x18c8e: recorre la tabla de 0x3f entradas de 6 bytes en 0x1a8f0 y
 * limpia las que valen 0x80; a6++ y limpia $1acf7. */
void icon_anim_a(uint8_t *img, uint32_t *a6) {
    uint32_t a0 = 0x1a8f0;
    for (int i = 0; i < 0x3f; i++) {
        if (img[a0] == 0x80)
            img[a0] = 0;
        a0 += 6;
    }
    *a6 += 1;
    img[0x1acf7] = 0;
}

/* FUN_00018cb2 @ 0x18cb2: tres pasadas de 0x18ce0 sobre las tablas 0x1a95c/0x1a9d4/0x1aa3a. */
void icon_anim_b(uint8_t *img, uint32_t *a6) {
    *a6 += 1;
    icon_anim_c(img, 0x1a95c, 3);
    img[0x1acf7] = 0;
    icon_anim_c(img, 0x1a9d4, 0xd);
    icon_anim_c(img, 0x1aa3a, 1);
}

/* FUN_00018316 @ 0x18316: lee tres words de 0x16230 y los guarda en A1+4/+6/+8. */
uint32_t pal_next_word(uint8_t *img);
void icon_list_build(uint8_t *img, uint32_t a1) {
    wr16(img + a1 + 4, pal_next_word(img) & 0xffff);
    wr16(img + a1 + 6, pal_next_word(img) & 0xffff);
    wr16(img + a1 + 8, pal_next_word(img) & 0xfff);
}

/* FUN_000182f0 @ 0x182f0: 256 veces: limpia el long en 0xd8f0+k*0xa y llama 0x18316. */
void icon_list_reset(uint8_t *img) {
    uint32_t a1 = 0xd8f0;
    for (uint32_t d1 = 0xff;; d1 = (d1 - 1) & 0xffff) {
        wr32(img + a1, 0);
        icon_list_build(img, a1);
        a1 += 0xa;
        if (d1 == 0) break;
    }
}

/* FUN_00018d28 @ 0x18d28: dibuja el marco del objeto/hint seleccionado.
 * A2 = *(0x1a81e) apunta a la celda que guarda el descriptor [u8 w][u8 h][long dest]
 * [word color]. Copia 0x1a81a->0x1a7d2 y (0x1acc9-1)->0x1ab4a. Convierte dest a
 * coordenadas de pantalla (resta el back buffer, divide por 0xa0) y dibuja dos
 * segmentos horizontales (0x12ffa) y dos verticales (0x13216) y luego cae en 0x1652e
 * para restaurar/repintar. */
void box_vline(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3);
void box_hline(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t d2, uint32_t d3);
void draw_selection_frame(uint8_t *img) {
    uint32_t a2 = be32(img + A_SELECTED_ICON_PTR);
    if (be32(img + a2) == 0)
        return;
    wr32(img + A_SEL_1A7D2, be32(img + A_SELECTED_OBJECT));
    img[A_SEL_1AB4A] = (uint8_t)(img[A_SEL_1ACC9] - 1);

    uint32_t a1 = be32(img + a2);
    uint32_t d2 = img[a1++];
    uint32_t d3 = img[a1++];
    uint32_t d0 = be32(img + a1);
    a1 += 4;
    uint32_t dest = d0 - be32(img + A_SCREEN_PTR_B);

    uint32_t d1 = (dest / 0xa0) & 0xffff;                       /* divu.w #$a0 */
    uint32_t rem = dest % 0xa0;                                 /* resto en la word alta */
    d0 = (((rem << 1) & 0xfff0) | be16(img + a1)) & 0xffff;
    d2 = ((d2 << 2) - 1) & 0xffff;
    d3 = (d3 - 1) & 0xffff;

    box_vline(img, d0, d1, 0, d3);
    box_hline(img, d0, d1, 0, d2);
    box_vline(img, (d0 + d2) & 0xffff, d1, 0, d3);
    box_hline(img, d0, (d1 + d3) & 0xffff, 0, d2);
    icon_apply(img, a2);
}
