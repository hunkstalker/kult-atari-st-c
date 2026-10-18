/* textform.c — formulario de texto con blits y sonido (0x141f4..0x143e2).
 *
 * 0x142cc prepara la lista de blits del formulario: elige el registro de la tabla
 * 0x1a7b6 segun D0, avanza la tabla de entradas de texto (0x10f8e) y por cada una de
 * las 3 lineas guarda [ancho][alto][fuente][pantalla][fase] en $d8f0, actualizando la
 * caja 0x1a56c..0x1a56f y el stride 0x1a534. 0x143e2 recorre esa lista: blitea cada
 * linea (0x11050/0x11102 segun el modo en $d8f0) con espera de VBL (0x13594) y cierra
 * con el blit enmascarado de la caja (0x1215e). 0x141f4/0x141fa y 0x14246/0x1424c son
 * las entradas del script: cargan el indice de A6[1], arrancan el sample (0x18f8e con
 * D4=0x1d) y repiten el bloque desplazando el destino $d904.
 */
#include <stdint.h>

#include "machine.h"
#include "addrs.h"

uint32_t advance_text_entry(uint8_t *, uint32_t, uint32_t);
uint32_t screen_address(uint8_t *, uint32_t, uint32_t, uint32_t *);
void supexec_b(uint8_t *, uint32_t, uint32_t);
void sound_play_a(uint8_t *, uint32_t);
void text_blit_fwd(uint8_t *, uint32_t, uint32_t, uint32_t,
                     uint32_t, uint32_t, uint32_t);
void text_blit_rev(uint8_t *, uint32_t, uint32_t, uint32_t,
                     uint32_t, uint32_t, uint32_t);
void wait_vbl(uint8_t *);
void mask_restore_rows(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t);

/* FUN_000142cc @ 0x142cc: prepara la lista de blits del formulario. D0 = indice
 * (1..16) del registro en la tabla 0x1a7b6; A6 = *(tabla + ((D0-1)&0xf)*4). */
void textform_build(uint8_t *img, uint32_t index) {
    wr32(img + A_TEXT_FORM_BOX, 0xff00ff00u);
    uint32_t reg = be32(img + A_TEXT_FORM_TABLE + (((index - 1) & 0xfu) * 4));
    uint32_t list = A_TEXT_FORM_MODE;
    uint32_t mode = 0;
    if (img[reg + 1] & 0x80)
        mode = 0xffff;
    wr16(img + list, (uint16_t)mode);
    list += 2;
    for (int line = 0; line < 3; line++) {
        uint32_t entry = advance_text_entry(img, A_TEXT_ENTRIES, img[reg]);
        uint32_t x = img[reg + 1];
        uint32_t y = img[reg + 2];
        uint32_t height = img[entry];
        entry += 1;
        uint32_t width = img[entry];
        entry += 1;
        uint32_t bx = x;
        if (x & 0x80) {
            x = (x + width - 1) & 0xff;
            x &= 0x7f;
            bx &= 0x7f;
        }
        wr32(img + A_DAT_1A534, (width << 1) & 0xffffu);
        y = (y << 1) & 0xff;
        if (bx < img[A_TEXT_FORM_BOX + 0])
            img[A_TEXT_FORM_BOX + 0] = (uint8_t)bx;
        if (((bx + width) & 0xff) >= img[A_TEXT_FORM_BOX + 1])
            img[A_TEXT_FORM_BOX + 1] = (uint8_t)((bx + width) & 0xff);
        if (y < img[A_TEXT_FORM_BOX + 2])
            img[A_TEXT_FORM_BOX + 2] = (uint8_t)y;
        if (((y + height) & 0xff) >= img[A_TEXT_FORM_BOX + 3])
            img[A_TEXT_FORM_BOX + 3] = (uint8_t)((y + height) & 0xff);
        uint32_t phase = 0;
        uint32_t addr = screen_address(img, x, y, &phase) - 0x8000u;
        wr16(img + list, (uint16_t)width); list += 2;
        wr16(img + list, (uint16_t)height); list += 2;
        wr32(img + list, entry); list += 4;
        wr32(img + list, addr); list += 4;
        wr16(img + list, (uint16_t)phase); list += 2;
        reg += 3;
    }
    {
        uint32_t x0 = img[A_TEXT_FORM_BOX + 0];
        uint32_t y0 = img[A_TEXT_FORM_BOX + 2];
        uint32_t w = (img[A_TEXT_FORM_BOX + 1] - x0) & 0xffu;
        uint32_t h = (img[A_TEXT_FORM_BOX + 3] - y0) & 0xffu;
        uint32_t phase = 0;
        uint32_t addr = screen_address(img, x0, y0, &phase);
        wr16(img + list, (uint16_t)w); list += 2;
        wr16(img + list, (uint16_t)h); list += 2;
        wr32(img + list, addr); list += 4;
        wr16(img + list, (uint16_t)phase); list += 2;
    }
}

/* FUN_000143e2 @ 0x143e2: dibuja la lista preparada por 0x142cc. Modo en $d8f0 (0 =
 * blit directo, != 0 = invertido), cada linea con espera de VBL; la caja final va por
 * el blit enmascarado. */
void textform_draw(uint8_t *img) {
    uint32_t p = A_TEXT_FORM_LIST;
    uint32_t i = 2;
    do {
        uint32_t w = be16(img + p); p += 2;
        uint32_t h = be16(img + p); p += 2;
        uint32_t src = be32(img + p); p += 4;
        uint32_t dest = be32(img + p); p += 4;
        uint32_t phase = be16(img + p); p += 2;
        uint32_t stride = (w << 1) & 0xffffffffu;
        if (be16(img + A_TEXT_FORM_MODE) == 0)
            text_blit_fwd(img, dest, src, stride, w, h, phase);
        else
            text_blit_rev(img, dest, src, stride, w, h, phase);
        wait_vbl(img);
    } while ((i = (i - 1) & 0xffffu) != 0xffffu);
    {
        uint32_t w = be16(img + p); p += 2;
        uint32_t h = be16(img + p); p += 2;
        uint32_t dest = be32(img + p); p += 4;
        uint32_t phase = be16(img + p); p += 2;
        mask_restore_rows(img, dest, w, h, phase);
    }
}

/* FUN_000141fa @ 0x141fa: cuerpo del formulario hacia delante. A6 ya apunta al
 * registro (0x141f4/0x141f6 cargan D0 de A6[1] y avanzan 2). */
void textform_fwd(uint8_t *img, uint32_t script, uint32_t index) {
    (void)script;
    textform_build(img, index);
    sound_play_a(img, 0x1d);
    uint32_t i = ((be16(img + A_TEXT_FORM_1) >> 1) - 1) & 0xffffu;
    do {
        textform_draw(img);
        wr16(img + A_TEXT_FORM_1, (uint16_t)(be16(img + A_TEXT_FORM_1) - 2));
        uint32_t v = (be16(img + A_TEXT_FORM_0) * 4u) & 0xffffffffu;
        wr32(img + A_TEXT_FORM_2, be32(img + A_TEXT_FORM_2) + v);
    } while ((i = (i - 1) & 0xffffu) != 0xffffu);
    supexec_b(img, 0x31, 0x1f);
}

/* FUN_000141f4 @ 0x141f4: entrada con D0 = A6[1], A6 += 2 y cae en 0x141fa. */
void textform_fwd_entry(uint8_t *img, uint32_t script) {
    uint32_t index = img[script + 1];
    script += 2;
    textform_fwd(img, script, index);
}

/* FUN_0001424c @ 0x1424c: cuerpo del formulario hacia atras. A6 ya apunta al
 * registro (0x14246 carga D0 de A6[1] y avanza 2). */
void textform_rev(uint8_t *img, uint32_t script, uint32_t index) {
    (void)script;
    textform_build(img, index);
    sound_play_a(img, 0x1d);
    uint32_t rows = be16(img + A_TEXT_FORM_1);
    uint32_t stride2 = (be16(img + A_TEXT_FORM_0) * 2u) & 0xffffffffu;   /* lsl.l #1 */
    uint32_t dest = be32(img + A_TEXT_FORM_2);
    uint32_t dest_saved = dest;
    uint32_t rows_saved = rows;
    uint32_t delta = stride2;
    dest = (dest + ((stride2 & 0xffffu) * (rows & 0xffffu))) & 0xffffffffu;
    dest = (dest - delta) & 0xffffffffu;
    wr32(img + A_TEXT_FORM_2, dest);
    rows = ((rows >> 1) - 1) & 0xffffu;
    delta = (delta & 0xffff0000u) | ((delta << 1) & 0xffffu);   /* lsl.w #1,delta */
    wr16(img + A_TEXT_FORM_1, 1);
    do {
        textform_draw(img);
        wr16(img + A_TEXT_FORM_1, (uint16_t)(be16(img + A_TEXT_FORM_1) + 2));
        wr32(img + A_TEXT_FORM_2, be32(img + A_TEXT_FORM_2) - delta);
    } while ((rows = (rows - 1) & 0xffffu) != 0xffffu);
    wr32(img + A_TEXT_FORM_2, dest_saved);
    wr16(img + A_TEXT_FORM_1, (uint16_t)rows_saved);
    textform_draw(img);
    sound_play_a(img, 0x1e);
}

/* FUN_00014246 @ 0x14246: entrada con D0 = A6[1], A6 += 2 y cae en 0x1424c. */
void textform_rev_entry(uint8_t *img, uint32_t script) {
    uint32_t index = img[script + 1];
    script += 2;
    textform_rev(img, script, index);
}
