/* loop.c — piezas del bucle principal (seleccion de objeto, lista de objetos).
 *
 * FUN_0001778e @ 0x1778e: busca en la lista de objetos (0x1a7ca..0x1a7ce, 8 B/entrada)
 * la entrada cuyo byte 4 con mascara 0x3f coincide con D0 (byte). Devuelve D2 =
 * indice+1 y D1 = clase encontrada, o D1 = 0xff si no hay. El resultado en el
 * llamante se lee por el Z (encontrado -> Z=1).
 *
 * FUN_000177bc @ 0x177bc: indexa la tabla 0x1aa72 por el byte D0, fija 0x1a772,
 * calcula la clase (byte3 & 7 | 0x10) y busca (0x1778e). Si la encuentra fija
 * 0x1a81a, 0x1acc9 y 0x1a81e y deja D0 = 0; si no, sale por 0x10768 dejando D0.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "machine.h"
#include "addrs.h"
#include "kult_port.h"
#include "os.h"
#include "kult_internal.h"


/* Devuelve 1 si encontrado. d1o = clase (o 0xff), d2o = indice+1. Conserva los bytes
 * altos de d2_in como el original (`move.b`/`addq.b`). */
int obj_find_masked(uint8_t *img, uint32_t target, uint32_t mask, uint32_t idx_in,
                uint32_t *entry_out, uint32_t *class_out, uint32_t *idx_out) {
    uint32_t p = be32(img + A_OBJECT_LIST_BEGIN);
    uint32_t idx = (idx_in & 0xffffff00u) | 1;
    for (;;) {
        uint32_t cls = img[p + 4] & (mask & 0xff);
        if (cls == (target & 0xff)) {
            *entry_out = p;
            *class_out = cls;
            *idx_out = idx;
            return 1;
        }
        p += 8;
        idx = (idx & 0xffffff00u) | (((idx & 0xff) + 1) & 0xff);
        if (p == be32(img + A_OBJECT_LIST_END))
            break;
    }
    *entry_out = p;
    *class_out = 0xff;
    *idx_out = idx;
    return 0;
}

/* FUN_0001778e @ 0x1778e: `move.b #0x3f,d3` y cae en 0x17792; el cuerpo vive alli
 * porque 0x1780c lo llama con su propia mascara D3. */
int obj_find(uint8_t *img, uint32_t target, uint32_t idx_in,
                uint32_t *entry_out, uint32_t *class_out, uint32_t *idx_out) {
    return obj_find_masked(img, target, 0x3f, idx_in, entry_out, class_out, idx_out);
}

/* FUN_000177e4 @ 0x177e4: fija el objeto seleccionado A0 en 0x1a81a, su indice
 * (D2, 1-based) en 0x1acc9 y el puntero de icono 0x1a81e = 0x1a776 + (D2-1)*4.
 * Corazón compartido por 0x177bc (que cae en el) y 0x1780c. */
void obj_find_setup(uint8_t *img, uint32_t obj, uint32_t idx) {
    wr32(img + A_SELECTED_OBJECT, obj);
    img[A_SEL_1ACC9] = (uint8_t)(idx & 0xff);
    uint32_t off = ((idx & 0xff) - 1) & 0xff;
    wr32(img + A_SELECTED_ICON_PTR, A_COMMAND_HINTS_LIST + (off << 2));
}

void class_find(uint8_t *);

/* 0x177bc — obj find class */
uint32_t obj_find_class(uint8_t *img, uint32_t index, uint32_t idx_in, uint32_t *class_out) {
    index &= 0xff;
    uint32_t p = A_OBJ_TBL + index;
    wr32(img + A_ELEM_CLASS, p);
    uint32_t cls = (img[p + 3] & 7) | 0x10;
    uint32_t entry, cls2, idx = idx_in;
    if (!obj_find(img, cls, idx_in, &entry, &cls2, &idx)) {
        *class_out = cls;
        return 0;
    }
    obj_find_setup(img, entry, idx);
    *class_out = 0;
    return 1;
}

/* FUN_000188ce @ 0x188ce: si el byte de script (A6[1]) esta en 0x3b..0x3e y
 * $1aaf9 == $1acd2, lanza las animaciones 0x87 (y 0x96 si $1ab08) con 0x177bc +
 * 0x17856 sobre los datos 0x19d24/0x19d2a. Conserva A6 y restaura $1acc9. */
void obj_anim(uint8_t *, uint32_t);

/* 0x188ce — ---- cluster 0x14602/0x1460a/0x146d0 (modo de escena) ---- 0x1460a: con $1a570 == 0 y ($1ad0f & 0x38) == 8 corre el formulario 0x141fa; guarda $1ad0f & 7 en ... */
uint32_t anim_mode(uint8_t *img, uint32_t script) {
    img[A_DAT_1AD1D] = 0;
    if (img[A_DAT_1AAF9] != img[A_DAT_1ACD2])
        return script;
    uint32_t code = img[script + 1];
    if (code < 0x3b || code >= 0x3f)
        return script;
    uint32_t sel = img[A_SEL_1ACC9];
    img[A_DAT_1AD1D] = 0xff;
    uint32_t class_out;
    obj_find_class(img, 0x87, 0, &class_out);
    obj_anim(img, 0x19d24);
    if (img[A_DAT_1AB08] != 0) {
        obj_find_class(img, 0x96, 0, &class_out);
        obj_anim(img, 0x19d2a);
    }
    img[A_SEL_1ACC9] = (uint8_t)sel;
    return script;
}

/* FUN_00017480 @ 0x17480: elige la accion de la tabla 0x1aa72 avanzando $1ad00
 * de 5 en 5 (salta a 5 al pasar de 0x18). Con $1acd2 == 0x37 fija las etiquetas
 * 0x37/0x32 y sale; si $1acec >= 0x3f o $1acd2 >= 0x2c sale tras limpiar. Busca el
 * objeto de clase 0x11 (0x1778e), indexa por $1ad00 y, con la tabla 0x2741e
 * (0x16210), decide el comando pendiente a018/a019 (o lo limpia). */
uint32_t pal_next_byte(uint8_t *);

/* 0x17480 — rot action */
void rot_action(uint8_t *img) {
    if (img[A_DAT_1ACD2] == 0x37) {
        img[0x1aa77] = 0x37;
        img[0x1aa7c] = 0x37;
        img[0x1aa81] = 0x37;
        img[0x1aa79] = 0x32;
        img[0x1aa7e] = 0x32;
        img[0x1aa83] = 0x32;
        return;
    }
    img[0x1aa77] = 0;
    img[0x1aa7c] = 0;
    img[0x1aa81] = 0;
    img[0x1aa86] = 0;
    if (img[A_DAT_1ACEC] >= 0x3f || img[A_DAT_1ACD2] >= 0x2c)
        return;                                  /* bcc a 0x10768 (rts) */

    uint32_t entry_out, class_out, idx_out;
    if (!obj_find(img, 0x11, 0, &entry_out, &class_out, &idx_out))
        return;
    wr32(img + 0x1a816, entry_out);
    uint32_t rot = ((uint32_t)img[A_DAT_1AD00] + 5) & 0xff;
    if (rot > 0x18)
        rot = 5;
    img[A_DAT_1AD00] = (uint8_t)rot;
    uint32_t entry = 0x1aa72 + rot;
    wr32(img + 0x1a812, entry);
    if (img[entry + 1] & 0x40)
        return;

    uint32_t prev = img[A_DAT_1ACD7];
    pal_next_byte(img);
    uint32_t v = img[A_DAT_1ACD7];
    uint32_t flag = 0;

    if (img[A_DAT_1ACD2] < 6) {
        if (prev < 0x5a && img[entry + 4] != 0 && img[entry + 4] >= 6 && img[entry + 4] < 0x1b &&
            (img[entry + 4] < 0x13 || img[entry + 4] >= 0x17))
            flag = 1;
        if (v < 0x17)
            goto cmd_a018;
        if (v < 0x34)
            goto cmd_a019;
        img[A_DAT_1ACFF] = 0;
        return;
    }
    if (prev < 0x27 && img[entry + 4] != 0)
        flag = 1;
    if (prev >= 0xa6)
        flag = 2;
    if (v < 0x1a)
        goto cmd_a018;
    if (v >= 0x34) {
        img[A_DAT_1ACFF] = 0;
        return;
    }
cmd_a019:
    wr16(img + A_DAT_1ABF8, 0xa019);
    img[A_DAT_1ACF2] = 3;
    img[A_DAT_1ACFF] = (uint8_t)(flag | 4);
    wr32(img + A_ELEM_CLASS, entry);
    return;
cmd_a018:
    img[entry] = img[A_DAT_1ACD2];
    wr16(img + A_DAT_1ABF8, 0xa018);
    img[A_DAT_1ACF2] = 3;
    img[A_DAT_1ACFF] = (uint8_t)flag;
    wr32(img + A_ELEM_CLASS, entry);
}

/* FUN_0001857c @ 0x1857c: busca en la tabla 0x1a8f0 la entrada [0x40][$1acd2] y
 * blitea los dos iconos (0x11020 + 0x11016) en (0x4a,0xe) con d4 = 0xad (0xac si
 * no la encuentra), y cuenta las entradas 0x80 de la tabla 0x1a9d4 en $1acf7. */
void text_blit_ovl_b(uint8_t *, uint32_t, uint32_t, uint32_t);
void text_blit_ovl_a(uint8_t *, uint32_t, uint32_t, uint32_t);

/* 0x1857c — sample find body */
void sample_find_body(uint8_t *img) {
    uint32_t p = 0x1a8f0;
    uint32_t cls = img[A_DAT_1ACD2];
    uint32_t color = 0xad;
    uint32_t i;
    for (i = 0x3e;; i = (i - 1) & 0xffff) {
        if (img[p] == 0x40 && img[p + 1] == cls)
            break;
        p += 6;
        if (i == 0) {
            color = (color - 1) & 0xffff;
            break;
        }
    }
    text_blit_ovl_b(img, 0x4a, 0x0e, color);
    text_blit_ovl_a(img, 0x4a, 0x0e, color);

    p = 0x1a9d4;
    img[A_DAT_1ACF7] = 0;
    for (i = 0x0d;; i = (i - 1) & 0xffff) {
        if (img[p] == 0x80)
            img[A_DAT_1ACF7] = (uint8_t)(img[A_DAT_1ACF7] + 1);
        p += 6;
        if (i == 0)
            break;
    }
}

/* FUN_00018944 @ 0x18944: si $1ad1d esta puesto, repite las animaciones 0x87/0x96
 * de 0x188ce sobre los datos 0x19d21/0x19d27 (guardando $1aaf9/$1ab08), restaura
 * $1acc9 y actualiza la lista de iconos (0x16582). */
void update_icon_list(uint8_t *);

/* 0x18944 — icon repeat */
void icon_repeat(uint8_t *img) {
    if (img[A_DAT_1AD1D] == 0)
        return;
    uint32_t sel = img[A_SEL_1ACC9];
    uint32_t class_out;
    obj_find_class(img, 0x87, 0, &class_out);
    obj_anim(img, 0x19d21);
    img[A_DAT_1AAF9] = img[A_DAT_1ACD2];
    if (img[A_DAT_1AB08] != 0) {
        img[A_DAT_1AB08] = img[A_DAT_1ACD2];
        obj_find_class(img, 0x96, 0, &class_out);
        obj_anim(img, 0x19d27);
    }
    img[A_SEL_1ACC9] = (uint8_t)sel;
    update_icon_list(img);
}

/* FUN_0001857a @ 0x1857a: `addq.l #1,script` y cae en 0x1857c. */
void anim_entry_a(uint8_t *, uint32_t);
void walk_entry(uint8_t *, uint32_t, uint32_t, uint32_t *, uint32_t *);
void icon_state_a(uint8_t *);
void icon_state_b(uint8_t *, uint32_t, uint32_t);
uint32_t anim_entry_b(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t);

/* 0x1857a — sample find wrap */
uint32_t sample_find_wrap(uint8_t *img, uint32_t script) {
    sample_find_body(img);
    return script + 1;
}

/* FUN_00018538 @ 0x18538: camina la entrada 0x7f de 0x27fe2 (walk_entry), guarda
 * D3 en su byte +2 y, si D3 es 0x25/0x3a/0x28, incrementa o decrementa $1ad2c
 * segun D4 == 0x80. */
void sample_walk(uint8_t *img, uint32_t cls, uint32_t flag) {
    { FILE *fl = kult_dbgfile();
      if (fl) { fprintf(fl, "18538 cls=%x flag=%x\n", cls, flag); fflush(fl); } }
    uint32_t end, entry;
    walk_entry(img, 0x27fe2, 0x7f, &end, &entry);
    img[entry + 2] = (uint8_t)cls;
    if (cls == 0x25 || cls == 0x3a || cls == 0x28) {
        if (flag == 0x80)
            img[A_DAT_1AD2C] = (uint8_t)(img[A_DAT_1AD2C] + 1);
        else
            img[A_DAT_1AD2C] = (uint8_t)(img[A_DAT_1AD2C] - 1);
    }
}

/* Cuerpo compartido 0x184de de los comandos 0x184cc/0x184d6/0x1851a: marca la
 * entrada de sample $1a75e (D4 en +0, $1acd2 en +1), repinta (0x164d4), anima
 * (0x159e2 con d0/d1 = 0x2c/0x29 y D5), blitea (0x1857c) y repinta texto
 * (0x164f6). A6 += 1. */
static uint32_t body_184de(uint8_t *img, uint32_t script, uint32_t val, uint32_t arg) {
    script += 1;
    uint32_t entry = be32(img + A_DAT_1A75E);
    { FILE *fl = kult_dbgfile();
      if (fl) {
          fprintf(fl, "184de val=%x arg=%x sample=%u rec=%x byte2=%x c2=%u\n",
                  val, arg, (unsigned)(entry & 0xffff), (unsigned)img[entry], (unsigned)img[entry + 2],
                  (unsigned)img[A_DAT_1ACD2]);
          fflush(fl);
      } }
    sample_walk(img, img[entry + 2], val);
    img[entry] = (uint8_t)val;
    img[entry + 1] = img[A_DAT_1ACD2];
    icon_state_a(img);
    anim_entry_b(img, script, 0x2c, 0x29, arg);
    sample_find_body(img);
    icon_state_b(img, script, 0x2c);
    return script;
}

/* FUN_000184cc @ 0x184cc (cmd idx 116) y FUN_000184d6 @ 0x184d6 (idx 115). */
uint32_t sample_mark_a(uint8_t *img, uint32_t script) {
    return body_184de(img, script, 0x80, 0x55);
}

/* 0x184d6 — sample mark b */
uint32_t sample_mark_b(uint8_t *img, uint32_t script) {
    return body_184de(img, script, 0x40, 0x2b);
}

void draw_hint_icons(uint8_t *);
void icon_table(uint8_t *);
void copy_screen(uint8_t *);

/* FUN_000171b4 @ 0x171b4 y FUN_000173ae @ 0x173ae: fijan el byte de seleccion en
 * la tabla 0x1aa72/0x1aa8b, buscan el objeto (0x177bc/0x17856 con el puntero de
 * 0x1a80a/0x1a80e + 1), programan el temporizador ($1acaa/$1acac o $1acae/$1acb0)
 * y repintan (0x16556/0x16582/copy_screen). */
void obj_anim(uint8_t *, uint32_t);
void sound_stop(uint8_t *);
void hint_blit_2(uint8_t *, uint32_t *);
void hint_blit_3(uint8_t *, uint32_t *);

/* 0x171b4 — sel timer a */
void sel_timer_a(uint8_t *img) {
    uint32_t track = be32(img + 0x1a80a) + 1;
    img[0x1aa72] = img[A_DAT_1ACD2];
    uint32_t class_out;
    obj_find_class(img, 0, 0, &class_out);
    obj_anim(img, track);
    sound_stop(img);
    wr16(img + A_DAT_1ACAA, 0xa015);
    wr16(img + A_DAT_1ACAC, (be16(img + A_DAT_1AB60) + 5) & 0xffff);
    draw_hint_icons(img);
    update_icon_list(img);
    copy_screen(img);
}

/* 0x173ae — sel timer b */
void sel_timer_b(uint8_t *img) {
    uint32_t track = be32(img + 0x1a80e) + 1;
    img[0x1aa8b] = img[A_DAT_1ACD2];
    uint32_t class_out;
    obj_find_class(img, 0x19, 0, &class_out);
    obj_anim(img, track);
    wr16(img + A_DAT_1ACAE, 0xa01f);
    wr16(img + A_DAT_1ACB0, (be16(img + A_DAT_1AB60) + 5) & 0xffff);
    draw_hint_icons(img);
    update_icon_list(img);
    copy_screen(img);
}

/* FUN_00017678 @ 0x17678: restaura los iconos 1 y 3, limpia la entrada $1a812 y
 * $1abf8; si +1 no lleva el bit 6, limpia el bit 7 de $1a816[4], busca con $1ad00
 * y anima 0x19cfa. */
void obj_anim(uint8_t *, uint32_t);

/* 0x17678 — obj clear anim */
void obj_clear_anim(uint8_t *img) {
    uint32_t script2 = 0;
    hint_blit_2(img, &script2);
    hint_blit_3(img, &script2);
    uint32_t entry = be32(img + 0x1a812);
    img[entry] = 0;
    wr16(img + A_DAT_1ABF8, 0);
    if (img[entry + 1] & 0x40)
        return;
    entry = be32(img + 0x1a816);
    img[entry + 4] &= 0x7f;
    img[A_DAT_1ACFF] = 0;
    uint32_t class_out;
    obj_find_class(img, img[A_DAT_1AD00], 0, &class_out);
    obj_anim(img, 0x19cfa);
}

/* FUN_00018128 @ 0x18128: si la clase de $1a772 lleva 0x30, la limpia, elige la
 * entrada ($1ace6-0xa) de 0x1a9d4 en $1a75e y corre 0x184d6. A6 += 1. */
uint32_t class30_dispatch(uint8_t *img, uint32_t script) {
    uint32_t obj = be32(img + A_ELEM_CLASS);
    if ((img[obj + 3] & 0x38) != 0x30)
        return script + 1;
    img[obj + 3] &= 0xe7;
    uint32_t off = ((uint32_t)img[0x1ace6] - 0xa) & 0xff;
    off = (off * 6) & 0xffff;
    wr32(img + A_DAT_1A75E, 0x1a9d4 + off);
    sample_mark_b(img, script);
    return script + 1;
}

/* FUN_000178cc @ 0x178cc: consume la entrada $1a812[4]: elige la entrada de 0x1a8f0
 * en $1a75e, sella 0x1ad33 y corre 0x184cc + despacho 0x90aa; si estaba a 0,
 * despacha 0x9140. A6 += 1. */
void run_command_keep_sp(uint8_t *, uint32_t);

/* 0x178cc — obj consume */
uint32_t obj_consume(uint8_t *img, uint32_t script) {
    script += 1;
    uint32_t entry = be32(img + 0x1a812);
    uint32_t idx = img[entry + 4];
    if (idx == 0) {
        wr16(img + A_THE_COMMAND, 0x9140);
        run_command_keep_sp(img, script);
        return script;
    }
    img[entry + 4] = 0;
    idx = (idx - 1) & 0xff;
    idx = (idx * 6) & 0xffff;
    uint32_t sample = 0x1a8f0 + idx;
    wr32(img + A_DAT_1A75E, sample);
    img[0x1ad01] = (uint8_t)(img[0x1ad01] + 1);
    uint32_t slot = 0x1ad33 + (((uint32_t)img[entry + 3] >> 6) & 0x3);
    img[slot] = img[sample + 3];
    sample_mark_a(img, script);
    wr16(img + A_THE_COMMAND, 0x90aa);
    run_command_keep_sp(img, script);
    return script;
}

/* FUN_000179a2 @ 0x179a2: como 0x18128 pero con 0x9148/0x9147/0x9032, sella 0x1ad33
 * y marca $1ad2e. A6 += 1. */
uint32_t class30_apply(uint8_t *img, uint32_t script) {
    wr16(img + A_THE_COMMAND, 0x9148);
    uint32_t obj = be32(img + A_ELEM_CLASS);
    if ((img[obj + 3] & 0xf8) == 0x30) {
        img[obj + 3] &= 0xe7;
        uint32_t off = ((uint32_t)img[0x1ace6] - 0xa) & 0xff;
        off = (off * 6) & 0xffff;
        uint32_t sample = 0x1a9d4 + off;
        wr32(img + A_DAT_1A75E, sample);
        img[0x1ad01] = (uint8_t)(img[0x1ad01] + 1);
        sample_mark_a(img, script);
        wr16(img + A_THE_COMMAND, 0x9147);
        run_command_keep_sp(img, script);
        img[0x1ad2e] = 1;
        wr16(img + A_THE_COMMAND, 0x9032);
    }
    run_command_keep_sp(img, script);
    return script + 1;
}

void load_eche_bank_wrap(uint8_t *, uint32_t);
void blit_sprite(uint8_t *, uint32_t);
void mask_restore_rows(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t);
void sound_play_a(uint8_t *, uint32_t);
void text_window(uint8_t *, uint32_t, uint32_t);
uint32_t text_index_arg(uint8_t *, uint32_t, uint32_t, uint32_t *);
uint32_t prompt_wait_entry(uint8_t *, uint32_t);
void hint_blit_stroke(uint8_t *, uint32_t *);
uint32_t anim_entry_b(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t);
void sample_list(uint8_t *, uint32_t);

int elem_class_find(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t,
                uint32_t *);
uint32_t elem_list_build(uint8_t *, uint32_t);
void text_block_build(uint8_t *, uint32_t, uint32_t);
void hint_select_on(uint8_t *, uint32_t, uint32_t *);
void blit_rect_vbl(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t,
                 uint32_t);
void hint_color(uint8_t *);
void screen_clean(uint8_t *);
void hint_blit_2(uint8_t *, uint32_t *);
void hint_blit_stroke(uint8_t *, uint32_t *);
uint32_t elem_blit_vbl(uint8_t *, uint32_t);
void wait_frame(uint8_t *);
void sound_play_b(uint8_t *, uint32_t);

/* FUN_00016dec @ 0x16dec: ficha/descripcion del objeto. Resuelve la clase (0x1285a),
 * toma selector D1=A6[2] (0xff/0xfe -> defaults 0x1ab4c/0x1ab4d) y arranca el sonido
 * (0x18e12); recorre la tabla 0x246e6 con 0x12950 y por cada entrada: compone el
 * bitmap (0x129c8), si el siguiente byte es 0xff salta 3 con 0x129d6, elige el icono
 * (0x120dc) y lo blitea (0x124da). Con $1aa6e != 0: si el byte previo es 0x25 avanza
 * la pista (0x18a30/0x16250 segun $1ad03), si no hace la espera de D1>>4. A6 += 4. */
uint32_t object_info_card(uint8_t *img, uint32_t script) {
    uint32_t out[4];
    elem_class_find(img, 0, 0, 0, 0, out);

    uint32_t sel = img[script + 1];
    uint32_t arg = img[script + 3];
    uint32_t idx = img[script + 2];
    if (idx == 0xff) idx = img[0x1ab4c];
    if (idx == 0xfe) idx = img[0x1ab4d];
    sample_list(img, idx);
    idx = (idx - 1) & 0xffff;

    uint32_t end = 0x246e6, p = 0;
    walk_entry(img, 0x246e6, idx, &end, &p);
    img[0x1a2ae] = (uint8_t)sel;
    wr16(img + 0x1aa6e, arg & 0xffff);
    for (;;) {
        img[0x1a2ad] = img[p];
        p = (p + 1) & 0xffffffffu;
        uint32_t n = (uint32_t)img[0x1a2ad];
        n = (n - 1) & 0xffff;
        uint32_t bitmap = elem_list_build(img, n);
        if (img[p] == 0xff) {
            p = (p + 1) & 0xffffffffu;
            text_block_build(img, p, (p + 3) & 0xffffffffu);
        }
        bitmap = (bitmap + 4) & 0xffffffffu;
        uint32_t o[8];
        hint_select_on(img, img[0x1a2ae], o);
        blit_rect_vbl(img, o[0], bitmap, o[5], o[6], o[2]);
        uint32_t wait = be16(img + 0x1aa6e);
        if (wait != 0) {
            if (img[p - 1] == 0x25) {
                if (img[0x1ad03] != 0)
                    hint_color(img);
                else
                    screen_clean(img);
            } else {
                wait = ((wait << 16) | (wait >> 16)) & 0xffffffffu;
                wait = (wait >> 4) & 0xffffffffu;
                do {
                    wait = (wait - 1) & 0xffffffffu;
                } while (wait != 0);
            }
        }
        if (end == p) break;
    }
    return (script + 4) & 0xffffffffu;
}

/* FUN_000189aa @ 0x189aa: ficha de objeto interactiva: retira los iconos 2/1
 * (0x12368/0x1232e), configura la caja de texto 0x19704 (color segun $1ad20), la
 * dibuja (0x134da), y repite {18f8e(0x1c); 16dec(0x19708); 18f98(0x1c);
 * 16dec(0x19708); 13698} hasta que $1a456 tenga el bit 2 (clic); cierra con 0x12368.
 * A6: entry+1/2/3 por los 12368/1232e y el ultimo 12368. */
uint32_t object_info_wait(uint8_t *img, uint32_t script) {
    hint_blit_2(img, &script);
    hint_blit_stroke(img, &script);
    uint32_t script2 = 0x19704;
    uint32_t color = 0x28;
    uint32_t v = img[0x1ad20];
    if (v != 0) {
        v = v >> 4;
        color = (color + v) & 0xff;
        color = (color + 1) & 0xff;
    }
    img[script2 + 6] = (uint8_t)color;
    color = (color + 0x0e) & 0xff;
    img[script2 + 10] = (uint8_t)color;
    elem_blit_vbl(img, script2);
    script = 0x19708;
    do {
        sound_play_a(img, 0x1c);
        uint32_t script3 = object_info_card(img, 0x19708);   /* la 2a llamada hereda A6 = retorno */
        sound_play_b(img, 0x1c);
        script = object_info_card(img, script3);
        wait_frame(img);
    } while ((img[0x1a456] & 0x04) == 0);
    hint_blit_2(img, &script);
    return script;
}

/* FUN_00017a1a @ 0x17a1a: cambio de vista/objeto. Fija $1a57b, resuelve el objeto
 * ($1a772), despacha los comandos pendientes $1acaa (0xa015) / $1abf8 (0xc357) con
 * 177bc y 103ae, y si a0[2] no es 0x2c/0x38/0x33 toma d2/d3 de 0x1a858/0x1a85a.
 * Monta el descriptor 0x19968 y corre 11f98 + 18a30; repinta el icono 1 (120e4/
 * 1215e) y elige la accion d1 de 0x1a69d (con 0x1acf7/0x1ad2c, 0x18f8e y 16210)
 * para publicarla en $1ad02. A6 = entry+1. */
uint32_t overlay_dispatch_2(uint8_t *, uint32_t);
void hint_select_off(uint8_t *, uint32_t, uint32_t *);
uint32_t obj_find_class(uint8_t *, uint32_t, uint32_t, uint32_t *);
uint32_t pal_next_byte(uint8_t *);
void noop_160c0(uint8_t *);

/* 0x17a1a — view change */
uint32_t view_change(uint8_t *img, uint32_t script) {
    uint32_t saved = be32(img + 0x1a4e0);
    uint32_t x = 0x23;
    uint32_t y = 0x14;
    uint32_t obj = be32(img + A_ELEM_CLASS);
    img[0x1a57b] = 1;
    if (img[obj + 2] != 0x2c) {
        uint32_t v = be16(img + A_DAT_1ACAA);
        if (v == 0xa015) {
            wr16(img + A_THE_COMMAND, v & 0xffff);
            uint32_t idx = (obj - 0x1aa72) & 0xffffffffu;
            run_command_keep_sp(img, script);
            uint32_t class_out;
            obj_find_class(img, idx, x, &class_out);
            obj = be32(img + A_ELEM_CLASS);
        }
        if (be16(img + A_DAT_1ABF8) == 0xc357) {
            wr16(img + A_THE_COMMAND, be16(img + A_DAT_1ABF8));
            run_command_keep_sp(img, script);
        }
        if (img[obj + 2] != 0x38 && img[obj + 2] != 0x33) {
            x = (be16(img + 0x1a858) + 0x10) & 0xff;
            y = be16(img + 0x1a85a) & 0xff;
            img[0x1a57b] = 0;
        }
    }
    img[0x19968 + 2] = (uint8_t)x;
    img[0x19968 + 3] = (uint8_t)y;
    overlay_dispatch_2(img, 0x19968);
    screen_clean(img);
    obj = be32(img + A_ELEM_CLASS);
    if (img[obj + 2] != 0x2c && img[obj + 2] != 0x38 && img[obj + 2] != 0x33) {
        uint32_t o[8];
        hint_select_off(img, 1, o);
        mask_restore_rows(img, o[0], o[5], o[6], o[2]);
        obj = be32(img + A_ELEM_CLASS);
    }
    uint32_t action = 0;
    img[0x1ad02] = 0;
    if (img[0x1ad03] == 0) {
        uint32_t idx = (img[obj + 2] - 0x2a) & 0xff;
        action = img[0x1a69d + idx];
        if (action != 1 && (img[obj + 1] & 0x80))
            action = (action - 1) & 0xff;
        if (img[A_DAT_1ACF7] != 0 || img[0x1ad2c] != 0)
            action = (action - 1) & 0xff;
    }
    if (action != 5) {
        uint32_t b = img[obj + 4];
        if ((b >= 0x13 && b < 0x17) || (b >= 0x27 && b < 0x34) || b == 0x38 ||
            b == 0x39 || (img[obj + 3] & 0xf8) == 0x30)
            action = (action + 1) & 0xff;
    }
    x = 1;
    y = img[A_DAT_1ACD7];
    if (action == 2 || action == 4) {
        uint32_t lim = (action == 2) ? 0xcd : 0x64;
        if (y >= lim) {
            x = 0x51;
            if (pal_next_byte(img) >= 0x80)
                x = 0x52;
        }
    } else if (action == 3) {
        if (y >= 0x80) {
            x = 0x31;
            if (pal_next_byte(img) >= 0xcd)
                x = 0x32;
        } else {
            x = 0x91;
            if (pal_next_byte(img) >= 0x33)
                x = 0x92;
        }
    }
    img[0x1ad02] = (uint8_t)x;
    noop_160c0(img);
    (void)action;
    wr32(img + 0x1a4e0, saved);
    return script + 1;
}

/* FUN_00017ca4 @ 0x17ca4: "usar/activar" el objeto de clase 0x1a772: fija $1aa70
 * (indice relativo), marca el objeto (bit6 de +1, +0 = 0) y limpia el bit7 del
 * seleccionado (0x1a81a[4]). Con a0[3] == 0x10/0x08 arma el script 0x55b0x y
 * despacha (17856 con A1=0x19cfd/0x19d00 + $1a50e=$1acaa + 103ae); si no, segun
 * a0[2] busca $1acc7 en 0x19d2d..0x19d55 (d0=0x2f/0x42) o en 0x19d55..0x19dbd,
 * ajusta script[1], suena (0x18f8e 0x96/0x97/0xb1 con $192d0) y corre la animacion
 * 0x159d2; si $192d0 sigue a 0, cuelga el driver (0x18fa4). A6 = entry+1. */
void anim_entry_a(uint8_t *, uint32_t);
void obj_anim(uint8_t *, uint32_t);
void run_command_keep_sp(uint8_t *, uint32_t);

/* 0x17ca4 — use object */
uint32_t use_object(uint8_t *img, uint32_t script) {
    uint32_t ret = script + 1;
    script += 1;
    if (img[A_DAT_1AD09] == 0x12)
        return ret;
    uint32_t obj = be32(img + A_ELEM_CLASS);
    wr16(img + 0x1aa70, (uint16_t)((obj - 0x1aa72) & 0xffff));
    uint32_t sel = be32(img + A_SELECTED_OBJECT);
    img[obj + 1] |= 0x40;
    img[obj] = 0;
    img[sel + 4] &= 0x7f;
    uint32_t v = img[obj + 3];
    uint32_t script2 = 0;
    if (v == 0x10)
        goto L_dea;
    if (v == 0x08)
        goto L_e22;
    if (img[0x1a57a] == 0 && img[0x1ad26] == 0 && img[A_DAT_1ACF7] != 0 &&
        img[0x1a57b] == 0) {
        img[0x1ad02] &= 0xfe;
        goto L_end;
    }
    img[0x1a57a] = 0;
    v = 0x2f;
    {
        uint32_t end;
        if (img[obj + 2] == 0x38)
            goto L_d4c;
        v = 0x42;
        if (img[obj + 2] != 0x33)
            goto L_d5a;
        wr16(img + 0x1acae, 0);
    L_d4c:
        script = 0x19d2d;
        end = 0x19d55;
        goto L_d68;
    L_d5a:
        v = 0;
        end = 0x19dbd;
        script = 0x19d55;
    L_d68:
        {
            uint32_t b = img[A_DAT_1ACC7];
            for (;;) {
                if (img[script] == b)
                    break;
                script += 4;
                if (script == end)
                    goto L_end;
            }
        }
        if (v != 0)
            img[script + 1] = (uint8_t)v;
        if (img[script + 1] == 0x18 || img[script + 1] == 0x1c) {
            img[0x192d0] = 1;
            sound_play_a(img, 0x96);
        } else if (img[script + 1] == 0x37) {
            sound_play_a(img, 0x97);
        } else if (img[script + 1] == 0x43) {
            sound_play_a(img, 0xb1);
        }
        anim_entry_a(img, script);
        if (img[0x192d0] == 0)
            sound_stop(img);
        goto L_end;
    }
L_dea:
    img[0x1ab1c] = img[A_DAT_1ACD2];
    script2 = 0x19cfd;
    img[0x1ab1d] = img[obj + 1];
    goto L_e02;
L_e22:
    script2 = 0x19d00;
    img[0x1ab21] = img[A_DAT_1ACD2];
    img[0x1ab22] = img[obj + 1];
L_e02:
    if (img[A_DAT_1ACF7] == 0)
        obj_anim(img, script2);
    wr16(img + A_THE_COMMAND, be16(img + A_DAT_1ACAA));
    run_command_keep_sp(img, script);
L_end:
    return ret;
}

/* FUN_000176c0 @ 0x176c0: segun $1a57b/$1acc7 avanza la pista 4/3 (0x1066e),
 * limpia $1ad2d y, con $1ad09 != 0x12, repinta la paleta (0x162da) y el icono del
 * objeto seleccionado (0x124f8 + 0x1215e); si $1ad03/$1ad26/$1acf7/$1a57b lo piden,
 * anima el objeto (0x18f8e 0x95 + 0x159e2 d1=0x28). Cierra copiando $1ad03 a $1a57a
 * y limpiandolo. A6 += 1. */
void set_palette_current(uint8_t *);
/* 0x176c0 — dibuja los iconos de pista, busca el objeto 0xbe (0x177bc), fija $1ad26 y corre 0x176c0; luego busca el objeto $1aa70, marca la entrada (0x53), activa el bit... */
uint32_t track_icon_anim(uint8_t *img, uint32_t script) {
    if (img[0x1a57b] != 0) {
        load_eche_bank_wrap(img, 4);
    } else if (img[A_DAT_1ACC7] == 0x3d) {
        load_eche_bank_wrap(img, 3);
    }
    img[0x1ad2d] = 0;
    if (img[A_DAT_1AD09] != 0x12) {
        set_palette_current(img);
        uint32_t slot = be32(img + A_SELECTED_ICON_PTR);
        if (be32(img + slot) != 0) {
            uint32_t desc = be32(img + slot);
            blit_sprite(img, desc);
            uint32_t w = img[desc++];
            uint32_t h = img[desc++];
            uint32_t addr = be32(img + desc);
            desc += 4;
            uint32_t phase = be16(img + desc);
            addr = (addr + 0x8000) & 0xffffffffu;
            mask_restore_rows(img, addr, w, h, phase);
            if (img[0x1ad03] == 0 && img[0x1ad26] == 0 && img[A_DAT_1ACF7] != 0 &&
                img[0x1a57b] == 0) {
                img[0x1ad2d] = 1;
                sound_play_a(img, 0x95);
                uint32_t obj = be32(img + A_SELECTED_OBJECT);
                uint32_t x = img[obj];
                uint32_t y = img[obj + 2];
                anim_entry_b(img, script, x, 0x28, y);
            }
        }
    }
    script += 1;
    img[0x1a57a] = img[0x1ad03];
    img[0x1ad03] = 0;
    return script;
}

/* FUN_00016a20 @ 0x16a20: retrato/intro: avanza la pista (0x1066e), copia el primer
 * par [indice][vol] de 0x1998a a 0xd8f0 y arma la lista con 0x16ab6 (a3=0x1998c,
 * d7=2, $192be=0x64c8e). Repite 0xb veces 14138(0x1997e)/VBL/copy + 14138(0x19981)/
 * VBL/copy, y cierra con 14138(0x19984), 14138(0x19987), copy y 0x18fa4. */
void load_eche_bank_wrap(uint8_t *, uint32_t);
void wait_vbl(uint8_t *);
void sample_entry(uint8_t *, uint32_t, uint32_t);
void run_text_entry(uint8_t *, uint32_t);
void sound_stop(uint8_t *);

/* 0x16a20 — portrait intro */
void portrait_intro(uint8_t *img) {
    img[0x1ad30] = 1;
    load_eche_bank_wrap(img, 0);
    uint32_t p = 0x1998a;
    wr16(img + 0xd8f0, be16(img + p));
    p += 2;
    wr16(img + 0x192e4, 0);
    wr32(img + 0x192be, 0x64c8e);
    sample_entry(img, p, 2);
    for (uint32_t i = 0x0a;; i = (i - 1) & 0xffff) {
        run_text_entry(img, 0x1997e);
        wait_vbl(img);
        copy_screen(img);
        {
            uint32_t spin = 0xffff;
            while (spin != 0xffff)
                spin = (spin - 1) & 0xffff;
        }
        run_text_entry(img, 0x19981);
        wait_vbl(img);
        copy_screen(img);
        {
            uint32_t spin = 0xffff;
            while (spin != 0xffff)
                spin = (spin - 1) & 0xffff;
        }
        if (i == 0) break;
    }
    run_text_entry(img, 0x19984);
    run_text_entry(img, 0x19987);
    copy_screen(img);
    sound_stop(img);
}

/* FUN_000171fa @ 0x171fa: variante A de la seleccion de pista. Ajusta A1 segun
 * $1aa72/$1ab1c (con $1acfc bit7 y eco en $1aa73), busca $1acc6 en la tabla
 * 0x19cd1..0x19cf7 -> $1acaa/$1acac y escribe la entrada en $1aa72, limpia el bit7
 * del objeto indexado por $1aa73&0xf, fija $1a806 y busca/anima (0x177bc/0x17856). */
uint32_t track_select_a(uint8_t *img, uint32_t script) {
    (void)script;
    uint32_t p = be32(img + 0x1a80a) + 4;
    uint32_t tbl = 0x1aa72;
    if (img[tbl] != 0)
        goto l1723c;
    p += 3;
    tbl += 0xaa;
    if (img[tbl] != 0)
        goto l1723c;
    p += 3;
    tbl += 5;
    img[tbl] = 0;
    img[A_DAT_1ACFC] |= 0x80;
    {
        uint8_t b = img[tbl + 1];
        tbl = 0x1aa72;
        img[tbl + 1] = b;
    }
l1723c:
    {
        uint32_t p2 = 0x19cd1;
        uint8_t mode = img[A_MODE_FLAG];
        for (;;) {
            if (img[p2] == mode) {
                wr16(img + A_DAT_1ACAA, 0xa016);
                wr16(img + A_DAT_1ACAC, (be16(img + A_DAT_1AB60) + 5) & 0xffff);
                img[tbl] = img[p2 + 1];
                break;
            }
            p2 += 2;
            if (p2 == 0x19cf7) {
                img[tbl] = 0;
                wr16(img + A_DAT_1ACAA, 0);
                break;
            }
        }
    }
    {
        uint32_t off = ((uint32_t)img[tbl + 1] & 0x0f) - 1;
        off = (off << 3) & 0xffffffffu;
        uint32_t obj = be32(img + A_OBJECT_LIST_BEGIN) + off;
        img[obj + 4] &= 0x7f;
    }
    wr32(img + 0x1a806, tbl);
    uint32_t class_out;
    obj_find_class(img, 0, 0, &class_out);
    /* 0x17856 deja A6 = A1+3 (o 0x55b05 si corrio el bloque del bit 7) */
    uint32_t ret = (img[p] & 0x80) ? 0x55b05 : ((p + 3) & 0xffffffffu);
    obj_anim(img, p);
    sound_stop(img);
    img[A_DAT_1ACFC] &= 0x80;
    return ret;
}

/* FUN_000173f2 @ 0x173f2: variante B (temporizador B): busca $1acc6 en
 * 0x19cd1..0x19cf7 -> $1acae/$1acb0 y escribe la entrada en $1aa8b, limpia el bit7
 * del objeto ($1aa8c&0xf) y busca/anima con 0x177bc(0x19)/0x17856. */
uint32_t track_select_b(uint8_t *img, uint32_t script) {
    (void)script;
    uint32_t p = be32(img + 0x1a80e) + 4;
    uint32_t tbl = 0x1aa72;
    {
        uint32_t p2 = 0x19cd1;
        uint8_t mode = img[A_MODE_FLAG];
        for (;;) {
            if (img[p2] == mode) {
                wr16(img + A_DAT_1ACAE, 0xa020);
                wr16(img + A_DAT_1ACB0, (be16(img + A_DAT_1AB60) + 5) & 0xffff);
                img[tbl + 25] = img[p2 + 1];
                break;
            }
            p2 += 2;
            if (p2 == 0x19cf7) {
                img[tbl + 25] = 0;
                wr16(img + A_DAT_1ACAE, 0);
                break;
            }
        }
    }
    {
        uint32_t off = ((uint32_t)img[tbl + 26] & 0x0f) - 1;
        off = (off << 3) & 0xffffffffu;
        uint32_t obj = be32(img + A_OBJECT_LIST_BEGIN) + off;
        img[obj + 4] &= 0x7f;
    }
    uint32_t class_out;
    obj_find_class(img, 0x19, 0, &class_out);
    uint32_t ret = (img[p] & 0x80) ? 0x55b05 : ((p + 3) & 0xffffffffu);
    obj_anim(img, p);
    return ret;
}

/* FUN_00017c42 @ 0x17c42: dibuja los iconos de pista, busca el objeto 0xbe
 * (0x177bc), fija $1ad26 y corre 0x176c0; luego busca el objeto $1aa70, marca la
 * entrada (0x53), activa el bit7 del seleccionado, limpia el bit6 de su clase y
 * copia $1acd2; repinta la lista de iconos y la pantalla. */
uint32_t track_icon_anim(uint8_t *, uint32_t);
uint32_t obj_find_class(uint8_t *, uint32_t, uint32_t, uint32_t *);

/* 0x17c42 — obj refresh */
uint32_t obj_refresh(uint8_t *img, uint32_t script) {
    draw_hint_icons(img);
    uint32_t class_out;
    obj_find_class(img, 0xbe, 0, &class_out);
    img[0x1ad26] = 1;
    script = track_icon_anim(img, script);
    img[0x1ad26] = 0;
    img[0x1ab30] = 0;
    obj_find_class(img, be16(img + 0x1aa70), 0, &class_out);
    {
        uint32_t p = be32(img + A_OBJECT_LIST_BEGIN);
        img[p + 44] = 0x53;
    }
    {
        uint32_t p = be32(img + A_SELECTED_OBJECT);
        img[p + 4] |= 0x80;
    }
    {
        uint32_t p = be32(img + A_ELEM_CLASS);
        img[p + 1] &= 0xbf;
        img[p] = img[A_DAT_1ACD2];
    }
    update_icon_list(img);
    copy_screen(img);
    return script;
}

/* FUN_000185e6 @ 0x185e6: sonido 0xe1, anima el objeto seleccionado (0x159e2 d1=0x2a
 * con +1/+6), elige el indice de pista d4 = 0x141 + ($1acd7&3) (+4 si $1ab60 >= 0xe10),
 * imprime el nombre (0x10fb4/0x10b6c sobre 0x19970 con color 0xb4/0x94), avanza la
 * tabla 0x2741e (0x18e12 con d1=0x22), anima d1=0x2b, espera tecla (0x1228a/0x1232e),
 * sonido 0xe1 y anima d1=0x2d. D0/D2 se preservan alrededor de los helpers. */
uint32_t track_name_print(uint8_t *img, uint32_t script) {
    sound_play_a(img, 0xe1);
    uint32_t obj = be32(img + A_SELECTED_OBJECT);
    uint32_t x = img[obj];
    uint32_t y = img[obj + 2];
    anim_entry_b(img, script, (x + 1) & 0xffff, 0x2a, (y + 6) & 0xffff);
    uint32_t idx = (uint32_t)(img[A_DAT_1ACD7] & 3);
    if (be16(img + A_DAT_1AB60) >= 0xe10)
        idx |= 4;
    idx = (idx + 0x141) & 0xffff;
    {
        uint32_t str = text_index_arg(img, 0x3c534, idx, &script);
        wr16(img + 0x1a484, be16(img + A_TEXT_INDEX));
        uint32_t ptr = 0x19970;
        uint32_t v;
        if ((x & 0xff) < 0x23) {
            v = (x + 8) & 0xff;
            img[ptr + 3] = 0xb4;
        } else {
            v = (x - 0x17) & 0xff;
            img[ptr + 3] = 0x94;
        }
        img[ptr + 2] = (uint8_t)((y - 0x28) & 0xff);
        img[ptr + 1] = (uint8_t)v;
        text_window(img, str, ptr);
        sample_list(img, 0x22);
    }
    anim_entry_b(img, 0x19970, x, 0x2b, y);
    (void)prompt_wait_entry(img, 0x19970);
    {
        uint32_t ptr2 = 0x19971;
        hint_blit_stroke(img, &ptr2);
    }
    sound_play_a(img, 0xe1);
    anim_entry_b(img, 0x19970, x, 0x2d, y);
    return script;
}

/* FUN_000175f6 @ 0x175f6: restaura el icono 1, sella la entrada $1a812 con $1acd2 y
 * los comandos a018/3, limpia $1ace7, busca/anima (0x177bc($1ad00)/0x17856(0x19cf7)),
 * repinta listas y copia pantalla; con $1acff == 5/6 despacha 0xc029/0xc165. */
uint32_t obj_seal(uint8_t *img, uint32_t script) {
    uint32_t script2 = 0;
    hint_blit_2(img, &script2);
    uint32_t entry = be32(img + 0x1a812);
    img[entry] = img[A_DAT_1ACD2];
    wr16(img + A_DAT_1ABF8, 0xa018);
    img[A_DAT_1ACF2] = 3;
    img[A_DAT_1ACE7] = 0;
    uint32_t class_out;
    obj_find_class(img, img[A_DAT_1AD00], 0, &class_out);
    uint32_t ret = (img[0x19cf7] & 0x80) ? 0x55b05 : 0x19cfa;
    obj_anim(img, 0x19cf7);
    draw_hint_icons(img);
    update_icon_list(img);
    copy_screen(img);
    if (img[A_DAT_1ACFF] == 5) {
        wr16(img + A_THE_COMMAND, 0xc029);
        img[A_DAT_1ACFF] = 0;
        run_command_keep_sp(img, script);
        return ret;
    }
    if (img[A_DAT_1ACFF] == 6) {
        wr16(img + A_THE_COMMAND, 0xc165);
        img[A_DAT_1ACFF] = 0;
        run_command_keep_sp(img, script);
        return ret;
    }
    return ret;
}

/* FUN_0001817a @ 0x1817a: marca la entrada del objeto (byte 44 = 0x53), limpia
 * $1ad0b/$1abf8 y cae en 0x18192. */
void track5_body(uint8_t *);

/* 0x1817a — obj mark */
void obj_mark(uint8_t *img) {
    uint32_t obj = be32(img + A_OBJECT_LIST_BEGIN);
    img[A_DAT_1AD0B] = 0;
    img[obj + 44] = 0x53;
    wr16(img + A_DAT_1ABF8, 0);
    track5_body(img);
}

/* FUN_0001834a @ 0x1834a y 0x18342 (A5 = 0/0x8000): recorre 256 descriptores de
 * 0xa bytes en 0xd8f0. Borra dos longs de pantalla con la mascara 0x7fff7fff
 * rotada por la fase (word +2), recalcula la posicion desde el campo +8 (0x10fee
 * con la escala 0xcffff/(v-0x138)) y OR-ea la mascara 0x8000 rotada en (0,2,4,6)
 * segun el tramo; si +8 < 0x148 y D5 >= 0x1e reinicia la entrada (0x18316) con el
 * bit 0x1800. Cierra con una VBL. */
void icon_list_build(uint8_t *, uint32_t);
void wait_vbl(uint8_t *);
uint32_t screen_address_raw(uint8_t *, uint32_t, uint32_t, uint32_t *);

/* 0x1834a — icon scale loop */
void icon_scale_loop(uint8_t *img, uint32_t base, uint32_t pass) {
    uint32_t desc = 0xd8f0;
    for (uint32_t i = 0xff;; i = (i - 1) & 0xffff) {
        uint32_t addr = be32(img + A_SCREEN_PTR_A) + be16(img + desc);
        uint32_t mask = rotate_right32(0x7fff7fffu, be16(img + desc + 2));
        wr32(img + addr, be32(img + addr) & mask);
        wr32(img + addr + 4, be32(img + addr + 4) & mask);

        uint32_t v = be16(img + desc + 8);
        if (v >= 0x148) {
            uint32_t scale = (v - 0x148) & 0xffff;
            wr16(img + desc + 8, scale);
            scale = (scale + 0x10) & 0xffff;
            scale = (0xcffffu / scale) & 0xffff;                   /* divu.w */
            int32_t p1 = (int32_t)(int16_t)be16(img + desc + 4) * (int32_t)scale;
            uint32_t x = ((uint32_t)p1 >> 16) & 0xffff;     /* swap + move.w */
            int32_t p2 = (int32_t)(int16_t)be16(img + desc + 6) * (int32_t)scale;
            uint32_t y = ((uint32_t)p2 >> 16) & 0xffff;
            x = (x + 0xa0) & 0xffff;
            if (!((int16_t)x < 0 || x >= 0x140)) {
                y = (y + 0x64) & 0xffff;
                if (!((int16_t)y < 0 || y >= 0xc8)) {
                    uint32_t phase;
                    uint32_t addr2 = screen_address_raw(img, x, y, &phase);
                    uint32_t off = (addr2 - be32(img + A_SCREEN_PTR_A)) & 0xffff;
                    addr2 = (addr2 - base) & 0xffffffffu;
                    wr16(img + desc, off);
                    wr16(img + desc + 2, phase & 0xffff);
                    uint32_t m = ror16(0x8000, phase);
                    uint32_t vb = be16(img + desc + 8);
                    if (vb < 0x600) {
                        wr16(img + addr2, be16(img + addr2) | m);
                        wr16(img + addr2 + 2, be16(img + addr2 + 2) | m);
                        wr16(img + addr2 + 6, be16(img + addr2 + 6) | m);
                    } else if (vb < 0xc00) {
                        wr16(img + addr2 + 4, be16(img + addr2 + 4) | m);
                    } else {
                        wr16(img + addr2 + 2, be16(img + addr2 + 2) | m);
                        wr16(img + addr2 + 6, be16(img + addr2 + 6) | m);
                    }
                    if (vb >= 0x600 && vb < 0xc00)
                        goto next;
                    if (vb >= 0xc00)
                        goto next;
                    wr16(img + addr2 + 4, be16(img + addr2 + 4) | m);
                } else {
                    wr16(img + desc + 8, 0);
                }
            } else {
                wr16(img + desc + 8, 0);
            }
        } else {
            if (pass >= 0x1e) {
                icon_list_build(img, desc);
                wr16(img + desc + 8, be16(img + desc + 8) | 0x1800);
            }
        }
next:
        desc += 0xa;
        if (i == 0)
            break;
    }
    wait_vbl(img);
}

/* FUN_00018334 @ 0x18334: 0x65 pasadas de 0x1834a (A5 = 0) con D5 = 0x64..0. */
void icon_scale(uint8_t *img) {
    uint32_t pass = 0x64;
    for (;;) {
        icon_scale_loop(img, 0, pass);
        if (pass == 0)
            break;
        pass = (pass - 1) & 0xffff;
    }
}

/* FUN_000181f0 @ 0x181f0: acumula las 16 entradas de 0x198c0 en la tabla 0x3b1ac
 * (16 longs inicializados a 0x014000c8). Cada pasada: avanza $1a4a0, refresca
 * $1a4a2 con 0x16230 cada 8 y, si el bit 0 de D3 esta puesto, suma (x,y) con topes
 * 0x258/0x190 (marcando en D2). Si algo topo, sale; si no, dibuja los 16 segmentos
 * con 0x13d0e (leyendo x/y del long y del siguiente, con vuelta al primero) y
 * repite. */
void reveal_line(uint8_t *);
uint32_t pal_next_word(uint8_t *);

/* 0x181f0 — room reset */
void room_reset(uint8_t *img) {
    uint32_t p = 0x3b1ac;
    for (int i = 0; i < 0x10; i++) {
        wr32(img + p, 0x14000c8);
        p += 4;
    }
    wr32(img + p, 0);
    wr16(img + A_DAT_1A49E, 0);
    wr16(img + A_ANIM_1A4A0, 0);
    wr16(img + A_SAVE_1A4A2, 0);
    for (;;) {
        uint32_t i = 0x0f;
        uint32_t entry = 0x3b1ac;
        uint32_t tbl = 0x198c0;
        uint32_t acc = be16(img + A_DAT_1A49E);
        uint32_t t = (be16(img + A_ANIM_1A4A0) + 1) & 0xffff;
        wr16(img + A_ANIM_1A4A0, t);
        uint32_t bits = be16(img + A_SAVE_1A4A2);
        if ((t & 7) == 0) {
            bits = pal_next_word(img);
            wr16(img + A_SAVE_1A4A2, bits);
        }
        for (;;) {
            uint32_t c = bits & 1;
            bits = (bits >> 1) & 0xffff;
            if (c) {
                uint32_t v = (be16(img + entry) + be16(img + tbl)) & 0xffff;
                if (v >= 0x258) {
                    acc |= 1u << (i & 0x1f);
                    v = be16(img + entry);
                }
                wr16(img + entry, v);
                v = (be16(img + entry + 2) + be16(img + tbl + 2)) & 0xffff;
                if (v >= 0x190) {
                    acc |= 1u << (i & 0x1f);
                    v = be16(img + entry + 2);
                }
                wr16(img + entry + 2, v);
            }
            tbl += 4;
            entry += 4;
            if (i == 0)
                break;
            i--;
        }
        if (acc != 0)
            return;
        wr16(img + A_DAT_1A49E, (uint16_t)acc);
        entry = 0x3b1ac;
        for (uint32_t i2 = 0x0f;; i2 = (i2 - 1) & 0xffff) {
            uint32_t v = be32(img + entry);
            entry += 4;
            wr16(img + 0x1a51a, ((v & 0xffff) >> 1) & 0xffff);
            wr16(img + 0x1a516, (((v >> 16) & 0xffff) >> 1) & 0xffff);
            uint32_t next = be32(img + entry);
            if (next == 0)
                next = be32(img + 0x3b1ac);
            wr16(img + 0x1a51c, ((next & 0xffff) >> 1) & 0xffff);
            wr16(img + 0x1a518, (((next >> 16) & 0xffff) >> 1) & 0xffff);
            reveal_line(img);
            if (i2 == 0)
                break;
        }
    }
}

/* FUN_0001842c @ 0x1842c: prepara la sala: llena screen_ptr_b con 0xffffffff/0xffff0000,
 * corre 0x181f0, limpia screen_ptr_b, copia a la frontal, reinicia la tabla de iconos
 * (0x182f0) y las 0x65 pasadas de 0x18334, anima (0x159e2), fuerza $1acc6 = 0x87,
 * carga FOND.PI1 (0x13568 reintentando con 0x918e), fija la paleta, repite 0x181f0,
 * copia y encadena con 0x1682a. */
void icon_list_reset(uint8_t *);
void load_fond(uint8_t *);
void set_palette(uint8_t *);
void reset_clear(uint8_t *);
void sound_play_a(uint8_t *, uint32_t);
uint32_t anim_entry_b(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t);
void run_command_keep_sp(uint8_t *, uint32_t);

/* 0x1842c — prepara sala: fondos, reveal, FOND.PI1, paleta, CLEAR */
uint32_t prepare_room(uint8_t *img, uint32_t script) {
    img[A_FRAME_1AB4F] = 0;
    sound_play_a(img, 0xa0);
    img[A_DAT_1AD0D] = 1;
    uint32_t p = be32(img + A_SCREEN_PTR_B);
    for (uint32_t i = 0xf9f;; i = (i - 1) & 0xffff) {
        wr32(img + p, 0xffffffffu); p += 4;
        wr32(img + p, 0xffff0000u); p += 4;
        if (i == 0) break;
    }
    room_reset(img);
    p = be32(img + A_SCREEN_PTR_B);
    for (uint32_t i = 0xf9f;; i = (i - 1) & 0xffff) {
        wr32(img + p, 0); p += 4;
        wr32(img + p, 0); p += 4;
        if (i == 0) break;
    }
    copy_screen(img);
    icon_list_reset(img);
    icon_scale(img);
    sound_play_a(img, 0xa1);
    anim_entry_b(img, script, 0x27, 0x2c, 0x5f);
    img[A_MODE_FLAG] = 0x87;
    for (;;) {
        load_fond(img);
        if ((int16_t)be16(img + A_FILE_HANDLE) >= 0)
            break;
        wr16(img + A_THE_COMMAND, 0x918e);
        run_command_keep_sp(img, script);
    }
    set_palette(img);
    room_reset(img);
    copy_screen(img);
    reset_clear(img);
    return script;
}

/* FUN_0001881e @ 0x1881e: suena 0xf2, repinta el texto 0x13f56 y copia 0x3c
 * filas de 0x28 longs desde screen_ptr_b+0x5500 a screen_ptr_a+0x5500 (VBL por
 * fila, retrocediendo 0x140), guarda el icono del objeto +0x18 en 0x1a782
 * (0x164a4), para el sonido y avanza A6. */
void run_text_table(uint8_t *, uint32_t, uint32_t, uint32_t);
uint32_t icon_table_entry(uint8_t *, uint32_t, uint32_t, uint32_t);
void wait_vbl(uint8_t *);
void sound_play_a(uint8_t *, uint32_t);
void sound_stop(uint8_t *);

/* 0x1881e — copy rows vbl */
uint32_t copy_rows_vbl(uint8_t *img, uint32_t script) {
    sound_play_a(img, 0xf2);
    run_text_table(img, script, 0, 0x62);
    uint32_t dst = be32(img + A_SCREEN_PTR_A) + 0x5500;
    uint32_t src = be32(img + A_SCREEN_PTR_B) + 0x5500;
    for (uint32_t row = 0x3b;; row = (row - 1) & 0xffff) {
        for (uint32_t col = 0x27;; col = (col - 1) & 0xffff) {
            wr32(img + dst, be32(img + src));
            dst += 4;
            src += 4;
            if (col == 0)
                break;
        }
        wait_vbl(img);
        src = (src - 0x140) & 0xffffffffu;
        dst = (dst - 0x140) & 0xffffffffu;
        if (row == 0)
            break;
    }
    uint32_t p = 0x1a782;
    uint32_t desc = be32(img + p);
    icon_table_entry(img, desc, p, (be32(img + A_OBJECT_LIST_BEGIN) + 0x18) & 0xffffffffu);
    sound_stop(img);
    return script + 1;
}

/* FUN_00018cfc @ 0x18cfc: dibuja los iconos de pista, baja 5 a los bytes 50/51 de
 * la lista de objetos, reconstruye la tabla de iconos (0x16460), la lista
 * (0x16582), copia pantalla y avanza A6. */
uint32_t obj_dec(uint8_t *img, uint32_t script) {
    draw_hint_icons(img);
    uint32_t obj = be32(img + A_OBJECT_LIST_BEGIN);
    img[obj + 50] = (uint8_t)(img[obj + 50] - 5);
    img[obj + 51] = (uint8_t)(img[obj + 51] - 5);
    icon_table(img);
    update_icon_list(img);
    copy_screen(img);
    return script + 1;
}

/* FUN_000186b6 @ 0x186b6: busca en 0x1a8f0 (desde la entrada A6[1], A6[2] intentos)
 * la primera cuyo byte 0 sea A6[3]; guarda la entrada en $1a75e y A6 += 4. */
uint32_t sample_find(uint8_t *img, uint32_t script) {
    uint32_t off = ((uint32_t)img[script + 1] - 1) & 0xff;
    off = (off * 6) & 0xffff;
    uint32_t cls = img[script + 3];
    uint32_t count = img[script + 2];
    uint32_t p = 0x1a8f0 + off;
    uint32_t n = (count - 1) & 0xffff;
    for (;;) {
        if (img[p] == cls)
            break;
        p += 6;
        if (n == 0) {
            p = (p - 6) & 0xffffffffu;
            break;
        }
        n = (n - 1) & 0xffff;
    }
    wr32(img + A_DAT_1A75E, p);
    return script + 4;
}

/* FUN_00018e12 @ 0x18e12: si $1a484 esta puesto, mapea D1 por la tabla 0x19994 a
 * un trio (d2,d3,d4), compone 10 pares [pitch][vol] en 0xd8f0 desde 0x1d402+$1a484
 * (dedup contra el par anterior, +4/+2 al cerrar) y arma la lista de samples en
 * 0x192d2.. desde el banco 0x55b0a (0x10f8e) apuntando $192be a 0x55b0a; termina
 * arrancando el driver (0x18f7a) con el primer par. */
uint32_t advance_text_entry(uint8_t *, uint32_t, uint32_t);
void supexec_b(uint8_t *, uint32_t, uint32_t);

static void tail_18e12(uint8_t *img, uint32_t p, uint32_t n);

/* 0x18e12 — sample list */
void sample_list(uint8_t *img, uint32_t sel) {
    if (be16(img + A_DAT_1A484) == 0)
        return;
    sel = img[0x19994 + (((sel & 0xff) - 1) & 0xff)];
    if (sel == 0)
        return;

    uint32_t mask = 7, base = 0, vol = 0x29;
    sel = (sel - 1) & 0xff;
    if (sel != 0) {
        base = 5;
        sel = (sel - 1) & 0xff;
        if (sel != 0) {
            base = 0xa; vol = 0x23; mask = 3;
            sel = (sel - 1) & 0xff;
            if (sel != 0) {
                base = 0xe; vol = 0x1d;
                sel = (sel - 1) & 0xff;
                if (sel != 0) {
                    base = 5; mask = 7;
                    sel = (sel - 1) & 0xff;
                    if (sel != 0) {
                        mask = 3; base = 0xa; vol = 0x29;
                        sel = (sel - 1) & 0xff;
                        if (sel != 0) {
                            mask = 7; base = 0; vol = 0x3d;
                            sel = (sel - 1) & 0xff;
                            if (sel != 0) {
                                mask = 3; base = 0xe; vol = 0x18;
                            }
                        }
                    }
                }
            }
        }
    }

    uint32_t p = 0x1d402 + be16(img + A_DAT_1A484);
    uint32_t out = 0xd8f0;
    uint32_t n = 9;
    for (;;) {
        uint32_t v = img[p++];
        v &= 7;
        v &= mask;
        if (v & 4)
            v &= 4;
        v = (v + base) & 0xff;
        if (v == img[out - 2])
            continue;                        /* dedup: no avanza ni cuenta */
        img[out++] = (uint8_t)v;
        img[out++] = (uint8_t)vol;
        if (n == 0)
            break;
        n = (n - 1) & 0xffff;
    }
    img[out - 1] = (uint8_t)(img[out - 1] + 4);
    img[out - 3] = (uint8_t)(img[out - 3] + 2);

    wr32(img + 0x192be, 0x55b0a);
    tail_18e12(img, 0xd8f2, 8);
}

/* Cola de 0x18e12 desde 0x18f0c: arma la lista de samples en 0x192d2 leyendo D7 pares
 * [indice][vol] en A3 (banco 0x55b0a via 0x10f8e; los offsets son relativos a $192be) y
 * arranca el driver con el primer par de 0xd8f0. Entrada propia 0x18f0c (desde 0x16ab6). */
static void tail_18e12(uint8_t *img, uint32_t p, uint32_t n) {
    img[0x192cf] = 1;
    uint32_t out = 0x192d2;
    wr32(img + 0x1930e, out);
    for (;;) {
        uint32_t idx = img[p];
        img[out + 1] = img[p + 1];
        uint32_t bank = advance_text_entry(img, 0x55b0a, idx);
        uint32_t end = be32(img + A_CUR_STR_END);
        uint32_t base = be32(img + 0x192be);
        wr16(img + out + 2, (uint16_t)((bank - base) + 2));
        wr16(img + out + 4, (uint16_t)(end - base));
        p += 2;
        out += 6;
        if (n == 0)
            break;
        n = (n - 1) & 0xffff;
    }
    supexec_b(img, img[0xd8f1], img[0xd8f0]);
}

/* 0x18f0c — sample queue */
void sample_queue(uint8_t *img, uint32_t p, uint32_t n) {
    tail_18e12(img, p, n);
}

/* FUN_00013f2a @ 0x13f2a: imprime la tabla de texto 0x24982 con el indice de
 * $1acf1 (0x13f56, d1=0x5c), suena 0x1d/0x1f y copia pantalla. A6 += 1. */
uint32_t text_table_paint(uint8_t *img, uint32_t script) {
    uint32_t idx = img[A_DAT_1ACF1];
    script += 1;
    run_text_table(img, script, idx, 0x5c);
    supexec_b(img, 0x1d, 0x1f);
    copy_screen(img);
    return script;
}

/* FUN_00016250 @ 0x16250: avanza la pista (0x18a10), pinta el color 7 a 0x777,
 * espera una VBL (0x13594) y lo devuelve a 0. */
void track_advance(uint8_t *);
uint32_t obj_wait_a0(uint8_t *);

/* 0x16250 — hint color */
void hint_color(uint8_t *img) {
    track_advance(img);
    os_setcolor(0, 0x777);
    wait_vbl(img);
    os_setcolor(0, 0);
}

/* FUN_00013686 @ 0x13686: copia 5 bytes desde A0 al paquete IKBD $1a44f. */


/* FUN_000169b4 @ 0x169b4 + envoltorios 0x1699c/0x169a4/0x169ac: llaman a 0x1680c
 * (espera + clase), despachan el comando 0x9183 y, si hay seleccion y clase, mapean
 * el byte 2 de la clase (0x5d-0x20, 0x85-0x49, -0x2a; <<1) a la tabla de A1; el
 * wrapper 0x169ac ademas suena 0x16. */
static void body_169b4(uint8_t *img, uint32_t table) {
    obj_wait_a0(img);
    uint32_t obj = be32(img + A_ELEM_CLASS);
    wr16(img + A_THE_COMMAND, 0x9183);
    if (img[A_SEL_1ACC9] == 0 || img[0x1ace6] == 0) {
        run_command_keep_sp(img, 0);
        return;
    }
    uint32_t v = img[obj + 2];
    if (v == 0x5d)
        v = (v - 0x20) & 0xff;
    if (v == 0x85)
        v = (v - 0x49) & 0xff;
    v = (v - 0x2a) & 0xff;
    v = (v << 1) & 0xffff;
    wr16(img + A_THE_COMMAND, be16(img + table + v));
    if (table == 0x1ac32)
        sound_play_a(img, 0x16);
    run_command_keep_sp(img, 0);
}

/* 0x1699c — obj wait cmd a */
void obj_wait_cmd_a(uint8_t *img) { body_169b4(img, 0x1ac82); }
/* 0x169a4 — obj wait cmd b */
void obj_wait_cmd_b(uint8_t *img) { body_169b4(img, 0x1ac5a); }
/* 0x169ac — obj wait cmd snd */
void obj_wait_cmd_snd(uint8_t *img) { body_169b4(img, 0x1ac32); }
/* 0x169b4 — obj wait cmd */
void obj_wait_cmd(uint8_t *img) { body_169b4(img, 0x1ac0a); }

/* FUN_00016ab6 @ 0x16ab6: guarda todos los registros y salta a la cola 0x18f0c con el
 * A3/D7 del llamante (0x16a20 fija $192be = 0x64c8e antes). */
void sample_entry(uint8_t *img, uint32_t p, uint32_t n) {
    tail_18e12(img, p, n);
}

/* FUN_00018e06 @ 0x18e06: D1 = A6[1], cuerpo 0x18e12 y A6 += 2. */
uint32_t sample_pair_a(uint8_t *img, uint32_t script) {
    sample_list(img, img[script + 1]);
    return script + 2;
}

/* FUN_00016902 @ 0x16902: imprime el nombre del objeto seleccionado con la pista
 * segun $1acd7 (6/7/0x23), publica $1a484 = $1a482, limpia el bit 6 del color,
 * arma el banco de samples (0x18e12) y espera (0x1228a -> 0x1232e). Devuelve A6. */
void obj_name_table(uint8_t *);
void text_window(uint8_t *, uint32_t, uint32_t);
uint32_t text_index_arg(uint8_t *, uint32_t, uint32_t, uint32_t *);
uint32_t prompt_wait_entry(uint8_t *, uint32_t);
void hint_blit_stroke(uint8_t *, uint32_t *);

/* 0x16902 — obj name track */
uint32_t obj_name_track(uint8_t *img, uint32_t script) {
    uint32_t idx = 6;
    if (img[A_DAT_1ACD7] < 0x55) {
        idx = 7;
        if (img[A_DAT_1ACD7] < 0xaa)
            idx = 0x23;
    }
    uint32_t str = text_index_arg(img, 0x3c534, idx, &script);
    wr16(img + A_DAT_1A484, be16(img + A_TEXT_INDEX));
    obj_name_table(img);
    /* 0x1695a deja A6 = 0x19970; 0x10b6c lo deja en 0x19974 (+4) */
    img[0x19973] &= 0xbf;
    text_window(img, str, 0x19970);
    sample_list(img, 0x23);
    (void)prompt_wait_entry(img, 0x19974);
    uint32_t ptr = 0x19975;
    hint_blit_stroke(img, &ptr);
    return 0x19976;
}

/* FUN_0001870a @ 0x1870a: avanza la tabla 0x2741e (0x16210) hasta que (byte&7)
 * != 7, imprime la entrada del banco 0x3c534 correspondiente (0x10fb4/0x10b6c con
 * A6 = 0x19974), espera tecla (0x1228a) y blitea los iconos 0x1232e. */
uint32_t text_index_arg(uint8_t *, uint32_t, uint32_t, uint32_t *);
void text_window(uint8_t *, uint32_t, uint32_t);
void hint_blit_stroke(uint8_t *, uint32_t *);
uint32_t prompt_wait_entry(uint8_t *, uint32_t);

/* 0x1870a — track walk */
uint32_t track_walk(uint8_t *img, uint32_t script) {
    uint32_t idx;
    for (;;) {
        pal_next_byte(img);
        idx = ((uint32_t)img[A_DAT_1ACD7] & 7) + 0x149;
        if (idx < 0x14f)
            break;
    }
    uint32_t str = text_index_arg(img, 0x3c534, idx, &script);
    text_window(img, str, 0x19974);
    /* 0x10b6c deja A6 en 0x19978 (avanza +4); 0x1228a/0x1232e suman 1 cada uno */
    (void)prompt_wait_entry(img, 0x19978);
    uint32_t ptr = 0x19979;
    hint_blit_stroke(img, &ptr);
    return 0x1997a;
}

/* FUN_0001851a @ 0x1851a (cmd idx 117): fija $1a75e a la entrada (A6[1]-1) de
 * 0x1a8f0, A6 += 1 y cae en 0x184cc (A6 total += 2). */
uint32_t sample_mark_c(uint8_t *img, uint32_t script) {
    uint32_t off = ((uint32_t)img[script + 1] - 1) & 0xff;
    off = (off * 6) & 0xffff;
    wr32(img + A_DAT_1A75E, 0x1a8f0 + off);
    return body_184de(img, script + 1, 0x80, 0x55);
}

/* FUN_00018888 @ 0x18888 y FUN_00018890 @ 0x18890: fijan A4 = 0x8000/0 y caen en
 * el cuerpo 0x18896: carga el banco de animacion A6[1] (0x15ac8), intercambia los
 * bytes del stride en 0xd8f0 y blitea con 0x11030. A6 += 4. */
void anim_bank(uint8_t *, uint32_t);
void text_blit_body_a(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t);

static void body_18896(uint8_t *img, uint32_t script, uint32_t base) {
    wr32(img + 0x1a4c6, 0xd8f0);
    anim_bank(img, img[script + 1]);
    uint32_t x = img[script + 2];
    uint32_t y = img[script + 3];
    uint32_t p = 0xd8f0;
    uint32_t stride = be16(img + p);
    stride = ((stride >> 8) | (stride << 8)) & 0xffff;
    wr16(img + p, stride);
    text_blit_body_a(img, p, x, y, base);
}

/* 0x18888 — bank blit a */
uint32_t bank_blit_a(uint8_t *img, uint32_t script) {
    body_18896(img, script, 0x8000);
    return script + 4;
}

/* 0x18890 — bank blit b */
uint32_t bank_blit_b(uint8_t *img, uint32_t script) {
    body_18896(img, script, 0);
    return script + 4;
}

/* FUN_00018a10 @ 0x18a10: avanza la tabla 0x2741e (0x16210) y llama al driver de
 * sonido (0x18f7a) con D4 = ((byte>>3)&3)+0x90 y D1 = (byte&7)+0x23. */
void supexec_b(uint8_t *, uint32_t, uint32_t);

/* 0x18a10 — avanza la pista (0x18a10), pinta el color 7 a 0x777, espera una VBL (0x13594) y lo devuelve a 0. */
void track_advance(uint8_t *img) {
    pal_next_byte(img);
    uint32_t v = img[A_DAT_1ACD7];
    uint32_t bank = (((v >> 3) & 3) + 0x90) & 0xff;
    uint32_t idx = ((v & 7) + 0x23) & 0xff;
    supexec_b(img, idx, bank);
}

/* FUN_00018192 @ 0x18192: si $1acc7 == 8 avanza la pista 5 (0x1066e); busca el
 * objeto 0xbe (0x177bc) y, si lo encuentra, le limpia el bit 7 de +4 y blitea su
 * icono (0x124f8); luego busca en la tabla 0x19dbd..0x19e19 el byte $1acc7 y anima
 * esa entrada (0x159d2). FUN_0001816e @ 0x1816e es el envoltorio: conserva A6, llama
 * a 0x18192 y avanza A6. */
void load_eche_bank_wrap(uint8_t *, uint32_t);
void blit_sprite(uint8_t *, uint32_t);

/* 0x18192 — marca la entrada del objeto (byte 44 = 0x53), limpia $1ad0b/$1abf8 y cae en 0x18192. */
void track5_body(uint8_t *img) {
    if (img[A_DAT_1ACC7] == 8)
        load_eche_bank_wrap(img, 5);
    uint32_t class_out;
    if (obj_find_class(img, 0xbe, 0, &class_out)) {
        uint32_t obj = be32(img + A_SELECTED_OBJECT);
        img[obj + 4] &= 0x7f;
        uint32_t slot = be32(img + A_SELECTED_ICON_PTR);
        blit_sprite(img, be32(img + slot));
    }
    for (uint32_t p = 0x19dbd; p != 0x19e19; p += 4) {
        if (img[p] == img[A_DAT_1ACC7]) {
            anim_entry_a(img, p);
            return;
        }
    }
}

/* 0x1816e — track5 apply */
uint32_t track5_apply(uint8_t *img, uint32_t script) {
    track5_body(img);
    return script + 1;
}

/* FUN_0001780c @ 0x1780c: A6[1] es la mascara D3 y A6[2] la clase D0. Con D3 != 0
 * busca el objeto (0x17792) y con D3 == 0 lo indexa directamente; luego 0x177e4 +
 * 0x14926 y, si no hay clase (0x1ace6 == 0), deja 0x1a772 = 0x1aad6. A6 += 3.
 * El camino "no encontrado" del original salta a 0x1018e (Pterm) sin escribir. */
uint32_t obj_find_or_exit(uint8_t *img, uint32_t script) {
    uint32_t target = img[script + 2];
    uint32_t mask = img[script + 1];
    uint32_t idx = target & 0xff;
    uint32_t entry;
    if (mask != 0) {
        uint32_t cls;
        if (!obj_find_masked(img, target, mask, 0, &entry, &cls, &idx))
            return script + 3;
    } else {
        target = (target - 1) & 0xff;
        entry = be32(img + A_OBJECT_LIST_BEGIN) + (target << 3);
    }
    obj_find_setup(img, entry, idx);
    class_find(img);
    if (img[A_CLASS_IDX] == 0)
        wr32(img + A_ELEM_CLASS, 0x1aad6);
    return script + 3;
}

/* ---- banco de animacion (0x15ac8) y blit de texto (0x1103e) ---- */

uint32_t advance_text_entry(uint8_t *, uint32_t, uint32_t);
void elem_blit(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t,
                 uint32_t);
void elem_blit_rev(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t,
                 uint32_t);
void text_blit_fwd(uint8_t *, uint32_t, uint32_t, uint32_t,
                     uint32_t, uint32_t, uint32_t);

/* FUN_0001103e @ 0x1103e: fija 0x1a534 = D2*2 y cae en el blit de texto (0x1104a). */
void text_blit_setup(uint8_t *img, uint32_t dest, uint32_t src, uint32_t cols, uint32_t rows,
                 uint32_t phase) {
    uint32_t stride = (cols << 1) & 0xffff;
    wr32(img + A_DAT_1A534, stride);
    text_blit_fwd(img, dest, src, be32(img + A_DAT_1A534), cols, rows, phase);
}

/* FUN_00015ac8 @ 0x15ac8: carga el banco de animacion en *(0x1a4c6) (0x190 longs a
 * cero), recorre la entrada D0 de la tabla 0x27fe2 (walk), copia sus 2 primeros bytes
 * como cabecera, toma el bitmap de 0x28ad2 (0x10f8e) y blitea cada bloque de 3 bytes
 * [n][b1][b2] con 0x12ad6/0x12b04 (bit7), stride = cabecera[0]*2. */
void anim_bank(uint8_t *img, uint32_t index) {
    frame_gate_delay(img);
    uint32_t p = be32(img + 0x1a4c6);
    for (int i = 0; i < 0x190; i++)
        wr32(img + p + (uint32_t)i * 4, 0);

    uint32_t end = 0x27fe2;
    if (index != 0) {
        uint32_t n = (index - 1) & 0xffff;
        for (;;) {
            end += img[end];
            if (n == 0)
                break;
            n = (n - 1) & 0xffff;
        }
    }
    uint32_t s = end;
    end += img[end];
    s += 1;
    p = be32(img + 0x1a4c6);
    img[p++] = img[s++];
    img[p++] = img[s++];

    /* Cada bloque recalcula su bitmap (0x10f8e), tamano y offset: el original
     * hace TODO esto dentro del bucle (0x15b02). Calcularlo una sola vez fuera
     * solo pintaba el primer bloque repetido -> sprites animados truncados. */
    for (;;) {
        uint32_t n2 = img[s];
        uint32_t bank = advance_text_entry(img, 0x28ad2, n2);
        uint32_t h = img[bank++];
        uint32_t w = img[bank++];
        uint32_t off = (((uint32_t)(img[s + 2] & 0x7f) << 8) | img[s + 1]) & 0xffff;
        uint32_t addr = be32(img + 0x1a4c6);
        uint32_t stride = ((uint32_t)img[addr] << 1) & 0xffffffffu;
        addr += 2;
        addr += (off << 1);
        if (img[s + 2] & 0x80)
            elem_blit_rev(img, addr, bank, w, h, stride);
        else
            elem_blit(img, addr, bank, w, h, stride);
        s += 3;
        if (s == end) break;
    }
}

void anim_bank(uint8_t *, uint32_t);
void text_blit_setup(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t,
                 uint32_t);
uint32_t screen_address(uint8_t *, uint32_t, uint32_t, uint32_t *);

/* FUN_00016636 @ 0x16636: el objeto A2 (byte0=x, byte2=y) pide la clase D3. Busca la
 * clase en la tabla 0x1aa72 (0x29 entradas de 5 B) por byte1&0xf; con el indice
 * avanza el contador de 0x1a57d, carga el banco de animacion en 0xd8f0 (0x15ac8) y
 * lo blitea en el back buffer ((x,y)-0x8000) con 0x1103e. Devuelve D0=0xff si la
 * clase esta, 0 si no. */
uint32_t obj_list_iter(uint8_t *img, uint32_t cls, uint32_t obj, uint32_t *out) {
    uint32_t p = A_OBJ_TBL;
    int found = 0;
    for (uint32_t n = 0x28;; n = (n - 1) & 0xffff) {
        if ((img[p + 1] & 0xf) == (cls & 0xff)) {
            found = 1;
            break;
        }
        p += 5;
        if (n == 0)
            break;
    }
    if (!found)
        return 0;

    uint32_t v = img[p + 3] & 0xf8;
    uint32_t v2 = v >> 3;
    p = 0x1a57d + v + v2;
    uint32_t idx = img[p];
    img[p] = (uint8_t)((img[p] + 1) & 7);
    p += idx + 1;
    idx = img[p];

    wr32(img + 0x1a4c6, A_SAVE_DESC);
    anim_bank(img, idx);

    uint32_t phase;
    uint32_t addr = screen_address(img, img[obj], img[obj + 2], &phase) - 0x8000;
    uint32_t desc = A_SAVE_DESC;
    uint32_t w = img[desc++];
    uint32_t h = img[desc++];
    text_blit_setup(img, addr, desc, w, h, phase);
    /* 0x16636 preserva D2/D3/D7/A0 alrededor de 0x1103e (movem #$3180): los deja
     * tal cual entraron (destino back buffer, ancho/alto del banco y fase). */
    out[0] = addr;
    out[1] = w;
    out[2] = h;
    out[3] = phase;
    return 0xff;
}

/* FUN_000166ec @ 0x166ec: escanea la lista de objetos desde *(0x1a7d2) buscando
 * entradas con byte4 bit6 y clase 0xd0. Para la primera que ademas 0x16636 resuelva,
 * espera VBL, restaura el cursor, blitea el sprite del banco y el icono del objeto.
 * FUN_000166da @ 0x166da: solo si (0x1acd4 & 0x1f) == 0x1e. */
void mask_restore_rows(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t);
void cursor_draw(uint8_t *);
void cursor_restore(uint8_t *);
void blit_sprite(uint8_t *, uint32_t);
void wait_vbl(uint8_t *);

/* 0x166ec — obj list b */
uint32_t obj_list_b(uint8_t *img) {
    uint32_t idx = img[A_SEL_1AB4A];
    uint32_t p = be32(img + A_SEL_1A7D2);
    if (p == be32(img + A_OBJECT_LIST_END)) {
        p = be32(img + A_OBJECT_LIST_BEGIN);
        idx = 0;
    }
    for (;;) {
        idx = (idx + 1) & 0xff;
        uint32_t b = img[p + 4];
        if ((b & 0x40) && ((b & 0xf8) == 0xd0)) {
            wr32(img + A_SEL_1A7D2, p + 8);
            img[A_SEL_1AB4A] = (uint8_t)idx;
            uint32_t cls = (((p - be32(img + A_OBJECT_LIST_BEGIN)) >> 3) + 1) & 0xff;
            uint32_t out[4];
            if (obj_list_iter(img, cls, p, out) != 0) {
                wait_vbl(img);
                cursor_restore(img);
                mask_restore_rows(img, out[0] + 0x8000, out[1], out[2], out[3]);
                cursor_draw(img);
                uint32_t idx = (img[A_SEL_1AB4A] - 1) & 0xff;
                blit_sprite(img, be32(img + A_COMMAND_HINTS_LIST + idx * 4));
                return 0;
            }
        }
        p += 8;
        if (p == be32(img + A_OBJECT_LIST_END))
            break;
    }
    wr32(img + A_SEL_1A7D2, p);
    img[A_SEL_1AB4A] = (uint8_t)idx;
    return 0;
}

/* 0x166da — obj list a */
uint32_t obj_list_a(uint8_t *img) {
    if ((img[A_DAT_1ACD4] & 0x1f) != 0x1e)
        return 0;
    return obj_list_b(img);
}

/* ---- hit-test de objetos y cursor ---- */

int rect_hit_test(uint8_t *, uint32_t, uint32_t, uint32_t);
void wait_frame(uint8_t *);
void cursor_draw(uint8_t *);

/* FUN_00010522 @ 0x10522: espera un frame y cae en el dibujo del cursor (0x10526). */
void wait_frame_cursor(uint8_t *img) {
    wait_frame(img);
    cursor_draw(img);
}

/* FUN_00014886 @ 0x14886: hit-test de la lista de objetos contra el cursor. Si el
 * cursor cae en un objeto con bit7 de su byte4 y (byte4 & D2) == D3, selecciona el
 * icono/objeto y busca su clase en 0x1aa72. Si no, marca 0x1a48a=1 y el selector por
 * defecto. */
void hit_test_objects(uint8_t *img, uint32_t mask, uint32_t value) {
    uint32_t x = (be16(img + A_CURSOR_X) >> 2) & 0xffff;
    uint32_t y = be16(img + A_CURSOR_Y);
    uint32_t p = be32(img + A_OBJECT_LIST_BEGIN);
    uint32_t idx = 1;
    int found = 0;
    for (;;) {
        if (rect_hit_test(img, p, x, y) && (img[p + 4] & 0x80) &&
            ((img[p + 4] & (mask & 0xff)) == (value & 0xff))) {
            found = 1;
            break;
        }
        p += 8;
        idx = (idx + 1) & 0xff;
        if (p == be32(img + A_OBJECT_LIST_END))
            break;
    }
    if (!found) {
        img[0x1a48a] = 1;
        img[A_TEXT_SEL_A] = img[A_1AB48];
        img[A_SEL_1ACC9] = 0;
        return;
    }

    img[A_SEL_1ACC9] = (uint8_t)idx;
    uint32_t off = (((idx & 0xff) - 1) << 2);
    wr32(img + A_SELECTED_ICON_PTR, A_COMMAND_HINTS_LIST + off);
    wr32(img + A_SELECTED_OBJECT, p);
    img[A_CUR_CLASS] = img[p + 4];
    img[A_TEXT_SEL_A] = img[p + 5];
    img[0x1a48a] = 0;
    wr16(img + A_THE_COMMAND, be16(img + p + 6));

    uint32_t cls_tbl = A_OBJ_TBL;
    uint32_t n = 1;
    uint32_t cls = img[A_SEL_1ACC9];
    for (uint32_t c = 0x28;; c = (c - 1) & 0xffff) {
        if ((img[cls_tbl + 1] & 0xf) == cls) {
            wr32(img + A_ELEM_CLASS, cls_tbl);
            img[A_CLASS_IDX] = (uint8_t)n;
            return;
        }
        cls_tbl += 5;
        n = (n + 1) & 0xff;
        if (c == 0)
            break;
    }
    img[A_CLASS_IDX] = 0;
}

/* FUN_00014832 @ 0x14832: hit-test y eleccion del sprite del cursor segun 0x1a48a y
 * los bits de 0x1ad0f. */
void hit_test(uint8_t *img) {
    hit_test_objects(img, img[A_HIT_MASK], img[A_HIT_VALUE]);
    uint32_t sprite = 0x194dc;
    if (img[0x1a48a] == 0) {
        uint32_t v = img[A_CUR_CLASS] & 0x38;
        sprite = 0x1961c;
        if (v != 0x10) {
            sprite = 0x1965c;
            if (v & 0x20)
                sprite = 0x1969c;
        }
    }
    wr32(img + A_CURSOR_SPRITE_PTR, sprite);
}

/* FUN_00016582 @ 0x16582 (update_icon_list): limpia el bit7 de los objetos de clase
 * 0x10; recorre la tabla 0x1aa72 y, para las entradas activas de la clase actual
 * (0x1acd2), marca el objeto correspondiente (bit7 + selector) y lo anima (0x16636). */
void update_icon_list(uint8_t *img) {
    for (uint32_t p = be32(img + A_OBJECT_LIST_BEGIN); p != be32(img + A_OBJECT_LIST_END); p += 8) {
        if ((img[p + 4] & 0x38) == 0x10)
            img[p + 4] &= 0x7f;
    }

    uint32_t cls_tbl = A_OBJ_TBL;
    for (uint32_t n = 0x28;; n = (n - 1) & 0xffff) {
        img[cls_tbl + 1] &= 0xf0;
        if (img[0x1acd2] == img[cls_tbl] && !(img[cls_tbl + 1] & 0x40)) {
            uint32_t p = be32(img + A_OBJECT_LIST_BEGIN);
            uint32_t cls = (img[cls_tbl + 3] & 7) | 0x10;
            uint32_t idx = 1;
            int found = 0;
            for (;;) {
                uint32_t b = img[p + 4] & 0xbf;
                if (b == cls) {
                    found = 1;
                    break;
                }
                p += 8;
                idx = (idx + 1) & 0xff;
                if (p == be32(img + A_OBJECT_LIST_END))
                    break;
            }
            if (found) {
                img[p + 4] |= 0x80;
                img[p + 5] = img[cls_tbl + 2];
                img[cls_tbl + 1] |= (uint8_t)idx;
                if (img[p + 4] & 0x40) {
                    uint32_t out[4];
                    obj_list_iter(img, idx, p, out);
                }
            }
        }
        cls_tbl += 5;
        if (n == 0)
            break;
    }
}

/* ---- animacion del objeto seleccionado (0x17856) ---- */

void blit_sprite(uint8_t *, uint32_t);
uint32_t draw_text_box(uint8_t *, uint32_t);
void anim_entry_a(uint8_t *, uint32_t);

/* FUN_00017856 @ 0x17856: blitea el icono seleccionado (*(0x1a81e)), y si el byte0
 * del script (A1, que el original salva y recupera como A6) tiene el bit7, fija el
 * cursor con la posicion del objeto (*(0x1a81a)), dibuja la caja de texto (0x1042c)
 * y prepara el "script" de 3 bytes en 0x55b02 [indice|x|y]. Cierra con el
 * interprete de animacion (0x159d2) sobre A1-1. */
void obj_anim(uint8_t *img, uint32_t entry) {
    uint32_t slot = be32(img + A_SELECTED_ICON_PTR);
    blit_sprite(img, be32(img + slot));
    uint32_t obj = be32(img + A_SELECTED_OBJECT);
    uint32_t script = entry;
    if (img[script] & 0x80) {
        uint32_t v = ((uint32_t)img[obj] << 2) & 0xffff;
        wr16(img + A_CURSOR_X, v);
        wr16(img + A_CURSOR_Y, img[obj + 2]);
        if (img[script + 1] != 0 || img[script + 2] != 0)
            draw_text_box(img, script);
        v = img[script] & 0x7f;
        script = 0x55b02;
        img[script] = (uint8_t)v;
        img[script + 1] = img[obj];
        img[script + 2] = img[obj + 2];
    }
    anim_entry_a(img, script - 1);
}

/* FUN_00018740 @ 0x18740: logica de fase por el reloj 0x1ab60. Segun el tramo fija
 * 0x1acfd/0x1ad14/0x1acec, reinicia partes de la tabla 0x1aa72 y encola comandos
 * (0x1ac06). Sin llamadas ni efectos fuera de imagen. */
void anim_step(uint8_t *img) {
    uint32_t t = be16(img + A_DAT_1AB60);
    if (t < 0xe10)
        return;
    if (img[A_DAT_1ACEC] >= 0x3f) {
        if (t < 0x189c)
            return;
        if (t >= 0x1c20) {
            wr16(img + A_DAT_1AC06, 0xc318);
            return;
        }
        img[A_DAT_1ACFD] = 2;
        img[A_DAT_1AD14] = 1;
        if (img[A_DAT_1ACD2] == 0x3d)
            return;
        if (img[A_DAT_1ACD2] < 0x3b)
            return;
        if (img[A_DAT_1ACD2] < 0x4b) {
            wr16(img + A_DAT_1AC06, 0xc012);
            return;
        }
        return;
    }

    img[A_DAT_1ACFD] = 3;
    img[A_DAT_1ACEC] = (img[A_DAT_1ACD2] < 0x2c) ? 0xff : 0x3f;
    uint32_t p = A_OBJ_TBL;
    img[p + 65] = 0;
    img[p + 70] = 0;
    img[p + 75] = 0;
    img[p + 80] = 0;
    img[p + 85] = 0;
    img[p + 90] = 0;
    img[p + 95] = 0x38;
    img[p + 100] = 0x38;
    img[p + 105] = 0x38;
    img[p + 110] = 0x38;
    if (img[A_DAT_1ACD2] >= 0x16)
        return;
    wr16(img + A_DAT_1AC06, 0xc012);
}

/* ---- actualizacion de pista / sonido (0x18dc0) ---- */

uint32_t pal_next_byte(uint8_t *);

/* FUN_00018dc0 @ 0x18dc0: si la pista es 8 o 0x32 y 0x1acd7 < 0x32, avanza la tabla
 * 0x2741e (0x16210) y llama al driver de musica (0x18f7a) con D4/D1 derivados de la
 * posicion. */
void supexec_b(uint8_t *, uint32_t, uint32_t);
/* 0x18dc0 — ---- bucle principal (game_loop @ 0x101be) ---- */
void sound_path(uint8_t *img) {
    uint8_t track = img[A_DAT_1ACD2];
    if (track != 8 && track != 0x32)
        return;
    if (img[A_DAT_1ACD7] >= 0x32)
        return;
    pal_next_byte(img);
    uint32_t v = img[A_DAT_1ACD7];
    uint32_t bank = 0xd0 + ((v & 0x80) ? 1u : 0u);
    uint32_t idx = 0x34 + (v & 0xf);
    supexec_b(img, idx, bank);
}

/* ---- bucle principal (game_loop @ 0x101be) ---- */

void sound_path(uint8_t *);
void script_step(uint8_t *);
void anim_step(uint8_t *);
void pal_threshold(uint8_t *);
void wait_frame(uint8_t *);
int  rect_hit_test_a(uint8_t *, uint32_t);
void set_cursor_clip(uint8_t *, uint32_t, uint32_t);
void hit_test(uint8_t *);
uint32_t cursor_select_b(uint8_t *, uint32_t);
void overlay_a_draw(uint8_t *, uint32_t);
void overlay_b_draw(uint8_t *, uint32_t);
void handle_frame_input(uint8_t *);
void run_command_keep_sp(uint8_t *, uint32_t);
void run_command(uint8_t *, uint32_t);
void clear_object_flags(uint8_t *);
void draw_hint_icons(uint8_t *);
void hint_icons(uint8_t *);
void wait_frame_cursor(uint8_t *);

/* Hook opcional al inicio de cada iteracion del bucle. Lo usa el motor nativo
 * para aplicar guardado/carga de sala en un punto de control conocido (entre
 * iteraciones); NULL en el build diferencial, asi que no altera la verificacion. */
void (*hook_loop_top)(uint8_t *img) = 0;

/* 0x101be — do{ ... } while: update subsistemas + comando pendiente + repintado */
void game_loop(uint8_t *img, uint32_t stack) {
    for (;;) {
        if (hook_loop_top) hook_loop_top(img);
        obj_list_a(img);
        sound_path(img);
        script_step(img);
        anim_step(img);
        pal_threshold(img);
        wait_frame(img);
        wr16(img + A_THE_COMMAND, 0);

        if (rect_hit_test_a(img, 0x1ab40)) {
            set_cursor_clip(img, 0, 1);
            img[A_TEXT_SEL_B] = 0x64;
            hit_test(img);
        } else {
            set_cursor_clip(img, 0, 0);
            img[A_TEXT_SEL_A] = 0x75;
            cursor_select_b(img, 0x19b06);
        }
        if (img[A_TEXT_SEL_A] != img[A_1AB44])
            overlay_a_draw(img, 0);
        if (img[A_TEXT_SEL_B] != img[A_1AB47])
            overlay_b_draw(img, 0);
        handle_frame_input(img);

        if ((img[A_INPUT_BUTTONS] & 0x04) && be16(img + A_THE_COMMAND) != 0) {
            cursor_restore(img);
            hint_icons(img);
            run_command(img, stack);
            img[A_DAT_1ACE7]++;
            if (img[A_DAT_1ACD3] != 0) {
                img[A_DAT_1ACE8]--;
                if (img[A_DAT_1ACE8] == 0)
                    clear_object_flags(img);
            }
            goto lab_304;
        }

        if (img[A_DAT_1ACF2] < img[A_DAT_1ACE7]) {
            wr16(img + A_THE_COMMAND, be16(img + A_DAT_1ABF8));
            if (be16(img + A_THE_COMMAND) != 0)
                goto lab_2cc;
        }
        if (img[A_DAT_1AD0B] != 0)
            goto lab_310;
        wr16(img + A_THE_COMMAND, be16(img + A_DAT_1AC06));
        if (be16(img + A_THE_COMMAND) != 0)
            goto lab_2cc;
        if (be16(img + A_DAT_1ACAC) < be16(img + A_DAT_1AB60)) {
            wr16(img + A_THE_COMMAND, be16(img + A_DAT_1ACAA));
            if (be16(img + A_THE_COMMAND) != 0)
                goto lab_2cc;
        }
        if (be16(img + A_DAT_1ACB0) < be16(img + A_DAT_1AB60))
            goto lab_310;
        wr16(img + A_THE_COMMAND, be16(img + A_DAT_1ACAE));
        if (be16(img + A_THE_COMMAND) == 0)
            goto lab_310;
    lab_2cc:
        cursor_restore(img);
        hint_icons(img);
        run_command_keep_sp(img, 0);
    lab_304:
        draw_hint_icons(img);
        wait_frame_cursor(img);
        obj_list_b(img);
    lab_310:
        if (be16(img + A_DAT_1A4D2) != 0)
            return;
    }
}

/* ---- intérprete de guion del bucle (script_step @ 0x17e3c) ---- */

void update_icon_list(uint8_t *);
void copy_screen(uint8_t *);
uint32_t obj_find_class(uint8_t *, uint32_t, uint32_t, uint32_t *);
void obj_anim(uint8_t *, uint32_t);

static void clear_selected_object(uint8_t *img) {
    uint32_t p = be32(img + A_SELECTED_OBJECT);
    img[p + 4] &= 0x7f;
}

/* 0x17e3c — ctx: avanza PTR_DAT_0001a822 y compara codigos de script 0x38/0x0c/0x0a */
void script_step(uint8_t *img) {
    if (img[A_DAT_1AD0B] != 0)
        return;
    if (img[A_DAT_1ACEC] >= 0x3f)
        return;
    wr16(img + A_DAT_1AC06, 0);
    if (img[A_1AAB4] & 0x40) {
        img[A_1AAB3] = 0x38;
        img[A_1AAB4] &= 0x0f;
        img[A_1AAB6] = 0x34;
    }
    if (img[A_1AAE1] & 0x20)
        img[A_DAT_1ACF6] |= 4;
    if (be16(img + A_DAT_1ACB2) >= be16(img + A_DAT_1AB60))
        return;
    wr16(img + A_DAT_1ACB2, (be16(img + A_DAT_1AB60) + 0x1e) & 0xffff);

    uint32_t p = be32(img + A_SCRIPT_PTR);
    uint32_t entry = A_OVL_ENTRY;
    img[A_DAT_1AD05] = img[p++];
    if (p == 0x19d0f)
        p = 0x19d03;
    img[A_DAT_1AD06] = img[p];
    wr32(img + A_SCRIPT_PTR, p);
    if (img[A_DAT_1AD05] == 0x38) {
        img[A_DAT_1ACF6] &= 0xfb;
        img[entry + 1] &= 0x1f;
        img[entry + 3] = 0x37;
        if (img[A_1AAB3] == 0x38) {
            img[A_1AAB4] &= 0x1f;
            img[A_DAT_1AD07] = 1;
        }
    }
    if (img[A_DAT_1AD05] == 0x0c && img[A_DAT_1AD06] == 0x0a) {
        img[A_DAT_1AD07] = 0;
        if (img[A_1AAB3] == 0x38) {
            img[A_DAT_1AD06] = 0x2c;
            wr32(img + A_SCRIPT_PTR, 0x19d07);
        }
    }
    img[entry] = img[A_DAT_1AD06];

    uint32_t class_out;
    uint32_t script;
    if (img[A_DAT_1AD07] != 0) {
        uint32_t v = img[A_DAT_1AD06];
        img[A_1AAB3] = (uint8_t)v;
        img[A_1AAD6] = (uint8_t)v;
        img[A_1AADB] = (uint8_t)v;
        if (img[A_DAT_1ACD2] == img[A_DAT_1AD05])
            goto lab_180aa;
        if (img[A_DAT_1ACD2] != img[A_DAT_1AD06])
            return;
        cursor_restore(img);
        hint_icons(img);
        script = 0x19d12;
        obj_find_class(img, 0x6e, 0, &class_out);
        obj_anim(img, script);
        script = 0x19d15;
        if (obj_find_class(img, 0x64, 0, &class_out)) {
            obj_anim(img, script);
            script = 0x19d15;
            obj_find_class(img, 0x41, 0, &class_out);
            obj_anim(img, script);
            script = 0x19d15;
            obj_find_class(img, 0x69, 0, &class_out);
            obj_anim(img, script);
        }
        goto lab_17fec;
    }

    if (img[entry + 1] & 0x40)
        return;
    if (img[A_DAT_1ACD2] == img[A_DAT_1AD05])
        goto lab_1807a;
    if (img[A_DAT_1ACD2] != img[A_DAT_1AD06])
        return;
    cursor_restore(img);
    hint_icons(img);
    obj_find_class(img, 0x6e, 0, &class_out);
    obj_anim(img, 0x19d0f);
    goto lab_17fec;

lab_1807a:
    cursor_restore(img);
    hint_icons(img);
    obj_find_class(img, 0x6e, 0, &class_out);
    clear_selected_object(img);
    obj_anim(img, 0x19d18);
    draw_hint_icons(img);
    cursor_draw(img);
    return;

lab_180aa:
    cursor_restore(img);
    hint_icons(img);
    obj_find_class(img, 0x6e, 0, &class_out);
    clear_selected_object(img);
    obj_find_class(img, 0x41, 0, &class_out);
    clear_selected_object(img);
    obj_find_class(img, 0x64, 0, &class_out);
    clear_selected_object(img);
    obj_find_class(img, 0x69, 0, &class_out);
    clear_selected_object(img);
    obj_anim(img, 0x19d1e);
    draw_hint_icons(img);
    cursor_draw(img);
    return;

lab_17fec:
    update_icon_list(img);
    copy_screen(img);
    draw_hint_icons(img);
    cursor_draw(img);
    if (img[A_DAT_1ACF7] != 0) {
        wr16(img + A_DAT_1AC06, 0xc1fd);
        return;
    }
    if (img[A_1AAE1] & 0x20) {
        wr16(img + A_DAT_1AC06, 0xc1e5);
        return;
    }
    if (img[A_DAT_1ACD2] != 0x2c)
        return;
    wr16(img + A_DAT_1AC06, 0xc060);
}

/* ---- estado de objetos/hints 0x17094/0x172ca y tabla 0x16f90/0x17930 ---- */

/* FUN_00016f90 @ 0x16f90: suma 0x101a8 a cada long no nulo de la tabla
 * 0x1a752..0x1a82e (0x37 entradas). */
void hint_table_relocate(uint8_t *img) {
    for (uint32_t p = 0x1a752; p != 0x1a82e; p += 4) {
        if (be32(img + p) != 0)
            wr32(img + p, be32(img + p) + 0x101a8);
    }
}

/* FUN_00017094 @ 0x17094: mantiene la tabla 0x1aa72 (bytes 0, 0xaa, 0xaf, 0xb0)
 * contra el objeto de clase 0x50 y la entrada 0x19b46 (13 B) elegida por 0x1acc7.
 * Varias ramas fijan el comando 0x1acaa (0xa014/0xa015) y el temporizador 0x1acac. */
static void hint_clear_17094(uint8_t *img, uint32_t base) {
    img[base + 170] = 0;
    img[base] = 0;
    img[base + 175] = 0;
    img[0x1acfc] &= 0x80;
}

/* 0x17094 — hint state a */
void hint_state_a(uint8_t *img, uint32_t base) {
    if (img[0x1acd2] == 8 && (img[0x1acfc] & 0x80)) {
        hint_clear_17094(img, base);
        return;
    }
    base = 0x1aa72;
    uint32_t p = be32(img + A_OBJECT_LIST_BEGIN);
    img[base + 1] &= 0xbf;
    img[base + 171] &= 0xbf;
    img[base + 176] &= 0xbf;
    int found = 0;
    for (;;) {
        if ((img[p + 4] & 0x7f) == 0x50) { found = 1; break; }
        p += 8;
        if (p == be32(img + A_OBJECT_LIST_END)) break;
    }
    if (!found) { hint_clear_17094(img, base); return; }
    p = 0x19b46;
    found = 0;
    for (;;) {
        if (img[p] == img[A_DAT_1ACC7]) { found = 1; break; }
        p += 0xd;
        if (p == 0x19c8b) break;
    }
    if (!found) { hint_clear_17094(img, base); return; }
    wr32(img + 0x1a80a, p);
    uint32_t v = img[0x1acd2];
    if (img[base] == v || img[base + 170] == v || img[base + 175] == v) {
        wr16(img + 0x1acaa, 0xa015);
        wr16(img + 0x1acac, (be16(img + 0x1ab60) + 5) & 0xffff);
        img[0x1acfc] |= 1;
        return;
    }
    hint_clear_17094(img, base);
    uint8_t track = img[A_DAT_1ACD7];
    if (track < 0x27) {
        img[base] = img[0x1acd2];
        wr16(img + 0x1acaa, 0xa015);
        wr16(img + 0x1acac, (be16(img + 0x1ab60) + 5) & 0xffff);
        img[0x1acfc] |= 1;
        return;
    }
    if (track >= 0x4e) return;
    wr16(img + 0x1acaa, 0xa014);
    wr16(img + 0x1acac, (be16(img + 0x1ab60) + 5) & 0xffff);
    img[0x1acfc] |= 1;
}

/* FUN_000172ca @ 0x172ca: como 0x17094 pero para la clase 0x51 y la tabla 0x19c8b
 * (7 B); fija 0x1acae (0xa01e/0xa01f) y 0x1acb0. */
void hint_state_b(uint8_t *img) {
    uint32_t base = 0x1aa72;
    if (img[0x1acd2] == 0x3b && img[0x1ad14] == 0) {
        img[0x1aa8b] = 0x3b;
        return;
    }
    uint32_t p = be32(img + A_OBJECT_LIST_BEGIN);
    img[base + 26] &= 0xbf;
    int found = 0;
    for (;;) {
        if ((img[p + 4] & 0x7f) == 0x51) { found = 1; break; }
        p += 8;
        if (p == be32(img + A_OBJECT_LIST_END)) break;
    }
    if (!found) { img[base + 25] = 0; return; }
    p = 0x19c8b;
    found = 0;
    for (;;) {
        if (img[p] == img[A_DAT_1ACC7]) { found = 1; break; }
        p += 7;
        if (p == 0x19cd1) break;
    }
    if (!found) { img[base + 25] = 0; return; }
    wr32(img + 0x1a80e, p);
    uint32_t v = img[0x1acd2];
    if (img[base + 25] == v)
        goto emit_a01f;
    img[base + 25] = 0;
    if (img[A_DAT_1ACD7] >= 0xd9) {
        img[base + 25] = img[0x1acd2];
        goto emit_a01f;
    }
    if (img[A_DAT_1ACD7] < 0xb2)
        return;
    wr16(img + 0x1acae, 0xa01e);
    wr16(img + 0x1acb0, (be16(img + 0x1ab60) + 5) & 0xffff);
    return;
emit_a01f:
    wr16(img + 0x1acae, 0xa01f);
    wr16(img + 0x1acb0, (be16(img + 0x1ab60) + 5) & 0xffff);
}

uint32_t text_box_icon(uint8_t *, uint32_t);
void text_blit_ovl_b(uint8_t *, uint32_t, uint32_t, uint32_t);

/* FUN_00017930 @ 0x17930: monta la entrada 0x19978 (x=0x1a858, y=0x1a85a+0x46,
 * ancho=a1[3]+0x12), la dibuja (0x10dfa) y termina rellenando con 0x11020;
 * a1 = 0x1a75e o 0x1a8f0+(obj[4]-1)*6 segun A6[1]. A6 += 2. */
void obj_text_box(uint8_t *img, uint32_t script) {
    script += 2;
    uint32_t entry;
    if (img[script - 1] != 0) {
        entry = be32(img + 0x1a75e);
    } else {
        uint32_t obj = be32(img + 0x1a812);
        uint32_t off = (uint32_t)(((img[obj + 4] - 1) & 0xff) * 6);
        entry = 0x1a8f0 + off;
    }
    uint32_t ptr = 0x19978;
    uint32_t x = be16(img + 0x1a858);
    uint32_t y = be16(img + 0x1a85a);
    y = (y & 0xff00) | ((y + 0x46) & 0xff);
    img[ptr + 3] = (uint8_t)x;
    img[ptr + 4] = (uint8_t)y;
    img[ptr + 2] = (uint8_t)((img[entry + 3] + 0x12) & 0xff);
    text_box_icon(img, ptr);
    uint32_t color = img[entry + 2];
    y = (y + 1) & 0xffff;
    text_blit_ovl_b(img, x, y, color);
}
