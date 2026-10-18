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

uint32_t advance_text_entry(uint8_t *img, uint32_t a1, uint32_t d4);
uint32_t screen_address(uint8_t *img, uint32_t d0, uint32_t d1, uint32_t *d7out);
void func_18f7a(uint8_t *img, uint32_t d1, uint32_t d4);
void func_18f8e(uint8_t *img, uint32_t d4);
void text_blit_fwd(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t a3,
                     uint32_t d2w, uint32_t d3w, uint32_t d7w);
void text_blit_rev(uint8_t *img, uint32_t a0, uint32_t a1, uint32_t a3,
                     uint32_t d2w, uint32_t d3w, uint32_t d7w);
void wait_vbl(uint8_t *img);
void mask_restore_rows(uint8_t *img, uint32_t a0, uint32_t d2, uint32_t d3, uint32_t d7);

/* FUN_000142cc @ 0x142cc: prepara la lista de blits del formulario. D0 = indice
 * (1..16) del registro en la tabla 0x1a7b6; A6 = *(tabla + ((D0-1)&0xf)*4). */
void textform_build(uint8_t *img, uint32_t d0) {
    wr32(img + A_TEXT_FORM_BOX, 0xff00ff00u);
    uint32_t a6 = be32(img + A_TEXT_FORM_TABLE + (((d0 - 1) & 0xfu) * 4));
    uint32_t a5 = A_TEXT_FORM_MODE;
    uint32_t d4 = 0;
    if (img[a6 + 1] & 0x80)
        d4 = 0xffff;
    wr16(img + a5, (uint16_t)d4);
    a5 += 2;
    for (int line = 0; line < 3; line++) {
        uint32_t a1 = advance_text_entry(img, A_TEXT_ENTRIES, img[a6]);
        uint32_t dd0 = img[a6 + 1];
        uint32_t d1 = img[a6 + 2];
        uint32_t d3 = img[a1];
        a1 += 1;
        uint32_t d2 = img[a1];
        a1 += 1;
        uint32_t d6 = dd0;
        if (dd0 & 0x80) {
            dd0 = (dd0 + d2 - 1) & 0xff;
            dd0 &= 0x7f;
            d6 &= 0x7f;
        }
        wr32(img + A_DAT_1A534, (d2 << 1) & 0xffffu);
        d1 = (d1 << 1) & 0xff;
        if (d6 < img[A_TEXT_FORM_BOX + 0])
            img[A_TEXT_FORM_BOX + 0] = (uint8_t)d6;
        if (((d6 + d2) & 0xff) >= img[A_TEXT_FORM_BOX + 1])
            img[A_TEXT_FORM_BOX + 1] = (uint8_t)((d6 + d2) & 0xff);
        if (d1 < img[A_TEXT_FORM_BOX + 2])
            img[A_TEXT_FORM_BOX + 2] = (uint8_t)d1;
        if (((d1 + d3) & 0xff) >= img[A_TEXT_FORM_BOX + 3])
            img[A_TEXT_FORM_BOX + 3] = (uint8_t)((d1 + d3) & 0xff);
        uint32_t d7 = 0;
        uint32_t a0 = screen_address(img, dd0, d1, &d7) - 0x8000u;
        wr16(img + a5, (uint16_t)d2); a5 += 2;
        wr16(img + a5, (uint16_t)d3); a5 += 2;
        wr32(img + a5, a1); a5 += 4;
        wr32(img + a5, a0); a5 += 4;
        wr16(img + a5, (uint16_t)d7); a5 += 2;
        a6 += 3;
    }
    {
        uint32_t x0 = img[A_TEXT_FORM_BOX + 0];
        uint32_t y0 = img[A_TEXT_FORM_BOX + 2];
        uint32_t w = (img[A_TEXT_FORM_BOX + 1] - x0) & 0xffu;
        uint32_t h = (img[A_TEXT_FORM_BOX + 3] - y0) & 0xffu;
        uint32_t d7 = 0;
        uint32_t a0 = screen_address(img, x0, y0, &d7);
        wr16(img + a5, (uint16_t)w); a5 += 2;
        wr16(img + a5, (uint16_t)h); a5 += 2;
        wr32(img + a5, a0); a5 += 4;
        wr16(img + a5, (uint16_t)d7); a5 += 2;
    }
}

/* FUN_000143e2 @ 0x143e2: dibuja la lista preparada por 0x142cc. Modo en $d8f0 (0 =
 * blit directo, != 0 = invertido), cada linea con espera de VBL; la caja final va por
 * el blit enmascarado. */
void textform_draw(uint8_t *img) {
    uint32_t a6 = A_TEXT_FORM_LIST;
    uint32_t d6 = 2;
    do {
        uint32_t d2 = be16(img + a6); a6 += 2;
        uint32_t d3 = be16(img + a6); a6 += 2;
        uint32_t a1 = be32(img + a6); a6 += 4;
        uint32_t a0 = be32(img + a6); a6 += 4;
        uint32_t d7 = be16(img + a6); a6 += 2;
        uint32_t a3 = (d2 << 1) & 0xffffffffu;
        if (be16(img + A_TEXT_FORM_MODE) == 0)
            text_blit_fwd(img, a0, a1, a3, d2, d3, d7);
        else
            text_blit_rev(img, a0, a1, a3, d2, d3, d7);
        wait_vbl(img);
    } while ((d6 = (d6 - 1) & 0xffffu) != 0xffffu);
    {
        uint32_t d2 = be16(img + a6); a6 += 2;
        uint32_t d3 = be16(img + a6); a6 += 2;
        uint32_t a0 = be32(img + a6); a6 += 4;
        uint32_t d7 = be16(img + a6); a6 += 2;
        mask_restore_rows(img, a0, d2, d3, d7);
    }
}

/* FUN_000141fa @ 0x141fa: cuerpo del formulario hacia delante. A6 ya apunta al
 * registro (0x141f4/0x141f6 cargan D0 de A6[1] y avanzan 2). */
void textform_fwd(uint8_t *img, uint32_t a6, uint32_t d0) {
    (void)a6;
    textform_build(img, d0);
    func_18f8e(img, 0x1d);
    uint32_t d6 = ((be16(img + A_TEXT_FORM_1) >> 1) - 1) & 0xffffu;
    do {
        textform_draw(img);
        wr16(img + A_TEXT_FORM_1, (uint16_t)(be16(img + A_TEXT_FORM_1) - 2));
        uint32_t v = (be16(img + A_TEXT_FORM_0) * 4u) & 0xffffffffu;
        wr32(img + A_TEXT_FORM_2, be32(img + A_TEXT_FORM_2) + v);
    } while ((d6 = (d6 - 1) & 0xffffu) != 0xffffu);
    func_18f7a(img, 0x31, 0x1f);
}

/* FUN_000141f4 @ 0x141f4: entrada con D0 = A6[1], A6 += 2 y cae en 0x141fa. */
void textform_fwd_entry(uint8_t *img, uint32_t a6) {
    uint32_t d0 = img[a6 + 1];
    a6 += 2;
    textform_fwd(img, a6, d0);
}

/* FUN_0001424c @ 0x1424c: cuerpo del formulario hacia atras. A6 ya apunta al
 * registro (0x14246 carga D0 de A6[1] y avanza 2). */
void textform_rev(uint8_t *img, uint32_t a6, uint32_t d0) {
    (void)a6;
    textform_build(img, d0);
    func_18f8e(img, 0x1d);
    uint32_t d6 = be16(img + A_TEXT_FORM_1);
    uint32_t d0w = (be16(img + A_TEXT_FORM_0) * 2u) & 0xffffffffu;   /* lsl.l #1 */
    uint32_t a0 = be32(img + A_TEXT_FORM_2);
    uint32_t a0_saved = a0;
    uint32_t d6_saved = d6;
    uint32_t d5 = d0w;
    a0 = (a0 + ((d0w & 0xffffu) * (d6 & 0xffffu))) & 0xffffffffu;
    a0 = (a0 - d5) & 0xffffffffu;
    wr32(img + A_TEXT_FORM_2, a0);
    d6 = ((d6 >> 1) - 1) & 0xffffu;
    d5 = (d5 & 0xffff0000u) | ((d5 << 1) & 0xffffu);   /* lsl.w #1,d5 */
    wr16(img + A_TEXT_FORM_1, 1);
    do {
        textform_draw(img);
        wr16(img + A_TEXT_FORM_1, (uint16_t)(be16(img + A_TEXT_FORM_1) + 2));
        wr32(img + A_TEXT_FORM_2, be32(img + A_TEXT_FORM_2) - d5);
    } while ((d6 = (d6 - 1) & 0xffffu) != 0xffffu);
    wr32(img + A_TEXT_FORM_2, a0_saved);
    wr16(img + A_TEXT_FORM_1, (uint16_t)d6_saved);
    textform_draw(img);
    func_18f8e(img, 0x1e);
}

/* FUN_00014246 @ 0x14246: entrada con D0 = A6[1], A6 += 2 y cae en 0x1424c. */
void textform_rev_entry(uint8_t *img, uint32_t a6) {
    uint32_t d0 = img[a6 + 1];
    a6 += 2;
    textform_rev(img, a6, d0);
}
