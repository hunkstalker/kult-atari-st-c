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
uint32_t icon_table_entry(uint8_t *img, uint32_t desc, uint32_t slot, uint32_t obj) {
    wr32(img + slot, desc);
    uint32_t x = img[obj];
    uint32_t y = img[obj + 2];
    uint32_t w = (uint32_t)((img[obj + 1] - x) & 0xff);
    uint32_t h = (uint32_t)((img[obj + 3] - y) & 0xff);
    uint32_t phase;
    uint32_t addr = screen_address(img, x, y, &phase);
    addr -= 0x8000;
    return save_rect_body(img, desc, w, h, addr, phase);
}

/* FUN_00016460 @ 0x16460: reconstruye la tabla de iconos de pista (0x1a776, 16
 * entradas): la pone a 0 y, por cada objeto con bit6 en byte4, guarda su rect en
 * buffers consecutivos desde 0x3a556 (A1) y enlaza el descriptor en el slot. */
void icon_table(uint8_t *img) {
    uint32_t buf = 0x3a556;
    uint32_t slot = A_COMMAND_HINTS_LIST;
    for (int i = 0; i <= 0xf; i++) {
        wr32(img + slot, 0);
        slot += 4;
    }
    slot = A_COMMAND_HINTS_LIST;
    uint32_t obj = be32(img + A_OBJECT_LIST_BEGIN);
    uint32_t end = be32(img + A_OBJECT_LIST_END);
    do {
        if (img[obj + 4] & 0x40)
            buf = icon_table_entry(img, buf, slot, obj);
        slot += 4;
        obj += 8;
    } while (obj != end);
}

/* 0x1652e — icon apply */
void icon_apply(uint8_t *img, uint32_t slot) {
    if (be32(img + slot) == 0)
        return;
    uint32_t desc = be32(img + slot);
    uint32_t cols = img[desc++];
    uint32_t rows = img[desc++];
    uint32_t dest = be32(img + desc);
    desc += 4;
    uint32_t phase = be16(img + desc);
    desc += 2;
    (void)desc;                                        /* a3 = (desc)+, no lo usa 0x12190 */
    mask_row_blit(img, (dest + 0x8000) & 0xffffffffu, cols, rows, phase);
}

/* 0x16514 — hint icons */
void hint_icons(uint8_t *img) {
    uint32_t slot = 0x1a776;
    for (uint32_t i = 0x0f; ; i = (i - 1) & 0xffff) {
        icon_apply(img, slot);
        slot += 4;
        if (i == 0) break;
    }
}

/* 0x16556 — ctx: itera 16 entradas (DAT_0001a776) llamando FUN_000124f8 */
void draw_hint_icons(uint8_t *img) {
    uint32_t slot = 0x1a776;
    for (uint32_t i = 0x0f; ; i = (i - 1) & 0xffff) {
        blit_sprite(img, be32(img + slot));
        slot += 4;
        if (i == 0) break;
    }
}

/* FUN_000164d4 @ 0x164d4: copia front->back si el modo $1acc7 es 0x29 o está en [0x16,0x19). */
void icon_state_a(uint8_t *img) {
    uint32_t mode = img[0x1acc7];
    if (mode == 0x29 || (mode >= 0x16 && mode < 0x19))
        copy_screen_rev(img);
}

/* FUN_000164f6 @ 0x164f6: si el modo $1acc7 está en [0x16,0x19) recorre ARPLA (0x13f56). */
void icon_state_b(uint8_t *img, uint32_t script, uint32_t arg) {
    uint32_t mode = img[0x1acc7];
    if (mode >= 0x16 && mode < 0x19)
        run_text_table(img, script, arg, mode);
}

/* FUN_00018ce0 @ 0x18ce0: recorre D0+1 entradas de 6 bytes en A0; si byte0 == 0x80 lo
 * pasa a 0x40 y copia $1acd2 a byte1. */
void icon_anim_c(uint8_t *img, uint32_t p, uint32_t count) {
    for (;;) {
        if (img[p] == 0x80) {
            img[p] = 0x40;
            img[p + 1] = img[0x1acd2];
        }
        p += 6;
        count = (count - 1) & 0xffff;
        if (count == 0xffff) break;
    }
}

/* FUN_00018c8e @ 0x18c8e: recorre la tabla de 0x3f entradas de 6 bytes en 0x1a8f0 y
 * limpia las que valen 0x80; a6++ y limpia $1acf7. */
void icon_anim_a(uint8_t *img, uint32_t *script) {
    uint32_t p = 0x1a8f0;
    for (int i = 0; i < 0x3f; i++) {
        if (img[p] == 0x80)
            img[p] = 0;
        p += 6;
    }
    *script += 1;
    img[0x1acf7] = 0;
}

/* FUN_00018cb2 @ 0x18cb2: tres pasadas de 0x18ce0 sobre las tablas 0x1a95c/0x1a9d4/0x1aa3a. */
void icon_anim_b(uint8_t *img, uint32_t *script) {
    *script += 1;
    icon_anim_c(img, 0x1a95c, 3);
    img[0x1acf7] = 0;
    icon_anim_c(img, 0x1a9d4, 0xd);
    icon_anim_c(img, 0x1aa3a, 1);
}

/* FUN_00018316 @ 0x18316: lee tres words de 0x16230 y los guarda en A1+4/+6/+8. */
uint32_t pal_next_word(uint8_t *img);
/* 0x18316 — y 0x18342 (A5 = 0/0x8000): recorre 256 descriptores de 0xa bytes en 0xd8f0. Borra dos longs de pantalla con la mascara 0x7fff7fff rotada por la fase (word +2... */
void icon_list_build(uint8_t *img, uint32_t desc) {
    wr16(img + desc + 4, pal_next_word(img) & 0xffff);
    wr16(img + desc + 6, pal_next_word(img) & 0xffff);
    wr16(img + desc + 8, pal_next_word(img) & 0xfff);
}

/* FUN_000182f0 @ 0x182f0: 256 veces: limpia el long en 0xd8f0+k*0xa y llama 0x18316. */
void icon_list_reset(uint8_t *img) {
    uint32_t desc = 0xd8f0;
    for (uint32_t i = 0xff;; i = (i - 1) & 0xffff) {
        wr32(img + desc, 0);
        icon_list_build(img, desc);
        desc += 0xa;
        if (i == 0) break;
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
/* 0x18d28 — marco del objeto/hint seleccionado (0x1a81e) */
void draw_selection_frame(uint8_t *img) {
    uint32_t slot = be32(img + A_SELECTED_ICON_PTR);
    if (be32(img + slot) == 0)
        return;
    wr32(img + A_SEL_1A7D2, be32(img + A_SELECTED_OBJECT));
    img[A_SEL_1AB4A] = (uint8_t)(img[A_SEL_1ACC9] - 1);

    uint32_t desc = be32(img + slot);
    uint32_t w = img[desc++];
    uint32_t h = img[desc++];
    uint32_t ptr = be32(img + desc);
    desc += 4;
    uint32_t dest = ptr - be32(img + A_SCREEN_PTR_B);

    uint32_t row = (dest / 0xa0) & 0xffff;                       /* divu.w #$a0 */
    uint32_t rem = dest % 0xa0;                                 /* resto en la word alta */
    ptr = (((rem << 1) & 0xfff0) | be16(img + desc)) & 0xffff;
    w = ((w << 2) - 1) & 0xffff;
    h = (h - 1) & 0xffff;

    box_vline(img, ptr, row, 0, h);
    box_hline(img, ptr, row, 0, w);
    box_vline(img, (ptr + w) & 0xffff, row, 0, h);
    box_hline(img, ptr, (row + h) & 0xffff, 0, w);
    icon_apply(img, slot);
}
