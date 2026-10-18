/* dispatch.c — el despacho de comandos y la VM de bytes del script.
 *
 * run_command @ 0x103a8 / run_command_keep_sp @ 0x103ae: leen `the_command`
 * ($1a50e) y despachan por su nibble alto:
 *   0x0   -> a6 = TEMPL(0x1d402) + comando, y FUN_00010630 (VM de bytes)
 *   0xa/b -> handler de la tabla cmd_jump_table (0x19eca, 144 entradas)
 *   0x9   -> caja de texto (0x10440)  [pendiente]
 *   0xf/otro -> walk_entry(TEMPL, (cmd&0x3ff)-1), $1a4e0 = fin, FUN_00010630
 *
 * FUN_00010630 @ 0x10630: VM que lee bytes de a6 y ejecuta el handler de la tabla
 * 0x19f62 mientras el byte esté en 1..0x6a y a6 no haya llegado a $1a4e0.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "machine.h"
#include "addrs.h"

void  walk_entry(uint8_t *img, uint32_t a4, uint32_t d1, uint32_t *a4o, uint32_t *a5o);
void  copy_screen_a6(uint8_t *img);      /* copy_screen (byte handler 68 / cmd 105) */
void  noop_11014(uint8_t *img);      /* rts (byte/cmd handler) */
void  func_141dc(uint8_t *img, uint32_t a6);
void  run_text_entry_1(uint8_t *img, uint32_t a6);
void  text_table_entry(uint8_t *img, uint32_t a6);
void  func_159d2(uint8_t *img, uint32_t a6);
void  set_palette_from_entry(uint8_t *img, uint32_t a6);
void  draw_text_box_variant(uint8_t *img, uint32_t the_command);
void  clear_flag(uint8_t *img);
void  func_15588(uint8_t *img);
void  func_155b8(uint8_t *img);
uint32_t func_104f4(uint8_t *img, uint32_t a6);
uint32_t load_eche_bank_cmd(uint8_t *img, uint32_t a6);
void  func_186ee(uint8_t *img);
void  func_17472(uint8_t *img);
uint32_t func_11f64(uint8_t *img, uint32_t a6);
uint32_t func_11f98(uint8_t *img, uint32_t a6);
uint32_t func_11f02(uint8_t *img, uint32_t a6);
uint32_t func_11f2e(uint8_t *img, uint32_t a6);
uint32_t func_134da(uint8_t *img, uint32_t a6);
uint32_t draw_text_box(uint8_t *img, uint32_t a6);
uint32_t func_12076(uint8_t *img, uint32_t a6);
uint32_t func_12044(uint8_t *img, uint32_t a6);
uint32_t func_11fd8(uint8_t *img, uint32_t a6);
uint32_t func_1200a(uint8_t *img, uint32_t a6);
uint32_t func_13474(uint8_t *img, uint32_t a6);
uint32_t func_12e08(uint8_t *img, uint32_t a6);
uint32_t func_12fea(uint8_t *img, uint32_t a6);
void     hint_blit_entry(uint8_t *img, uint32_t a6, uint32_t *a6_out);
void     hint_blit_2(uint8_t *img, uint32_t *a6);
uint32_t func_121fa(uint8_t *img, uint32_t a6);
void     hint_blit_stroke(uint8_t *img, uint32_t *a6);
uint32_t func_1228a(uint8_t *img, uint32_t a6);
void     func_1350c(uint8_t *img, uint32_t *a6);
void     func_13522(uint8_t *img, uint32_t *a6);
void     func_13552(uint8_t *img, uint32_t *a6);
void     script_store(uint8_t *img, uint32_t *a6);
void     script_jump_zero(uint8_t *img, uint32_t *a6);
void     hint_blit_3(uint8_t *img, uint32_t *a6);
uint32_t func_12ea2(uint8_t *img, uint32_t a6);
void     set_palette_advance(uint8_t *img);
void     func_18c8e(uint8_t *img, uint32_t *a6);
void     func_18cb2(uint8_t *img, uint32_t *a6);

/* Un handler de byte de la VM (tabla 0x19f62): recibe a6 y devuelve el a6 nuevo.
 * El avance de A6 es el neto del handler original (los `addq`/push-pop), dato que
 * no modela la firma C, así que se codifica aquí. Solo los reconstruidos. */
uint32_t func_14ac6(uint8_t *img, uint32_t a6);
uint32_t func_14bfa(uint8_t *img, uint32_t a6);
void func_17930(uint8_t *img, uint32_t a6);
uint32_t func_179a2(uint8_t *img, uint32_t a6);
uint32_t func_132aa(uint8_t *img, uint32_t a6);
uint32_t func_132ec(uint8_t *img, uint32_t a6);
uint32_t func_1332e(uint8_t *img, uint32_t a6);
uint32_t reveal_pixel(uint8_t *img, uint32_t a6);
uint32_t func_13740(uint8_t *img, uint32_t a6);
uint32_t func_13702(uint8_t *img, uint32_t a6);
uint32_t func_1780c(uint8_t *img, uint32_t a6);
uint32_t func_1816e(uint8_t *img, uint32_t a6);
void func_168a0(uint8_t *img, uint32_t a6);
uint32_t object_info_card(uint8_t *img, uint32_t a6);
uint32_t func_13490(uint8_t *img, uint32_t a6);
uint32_t func_134b2(uint8_t *img, uint32_t a6);
uint32_t func_138a6(uint8_t *img, uint32_t a6);
uint32_t func_1442c(uint8_t *img, uint32_t a6);
uint32_t prepare_room(uint8_t *img, uint32_t a6);
void func_1687e(uint8_t *img, uint32_t a6);
uint32_t func_167aa(uint8_t *img, uint32_t a6);
uint32_t func_10b52(uint8_t *img, uint32_t a6);
uint32_t func_1224e(uint8_t *img, uint32_t a6);
uint32_t func_12240(uint8_t *img, uint32_t a6);
uint32_t func_178cc(uint8_t *img, uint32_t a6);
uint32_t func_17a1a(uint8_t *img, uint32_t a6);
uint32_t func_17ca4(uint8_t *img, uint32_t a6);
uint32_t func_176c0(uint8_t *img, uint32_t a6);
void func_14602(uint8_t *img, uint32_t a6);
uint32_t func_10dfa(uint8_t *img, uint32_t a6);
void textform_fwd_entry(uint8_t *img, uint32_t a6);
void textform_rev_entry(uint8_t *img, uint32_t a6);
uint32_t func_15888(uint8_t *img, uint32_t a6);
uint32_t view_mode_cycle(uint8_t *img, uint32_t a6);
uint32_t func_13f2a(uint8_t *img, uint32_t a6);
uint32_t scene_mode_apply(uint8_t *img, uint32_t a6);
uint32_t scene_mode_draw(uint8_t *img, uint32_t a6);
uint32_t func_16b9c(uint8_t *img, uint32_t a6);
uint32_t func_16d78(uint8_t *img, uint32_t a6);
uint32_t func_16ce2(uint8_t *img, uint32_t a6);
uint32_t func_16ca6(uint8_t *img, uint32_t a6);
uint32_t func_16d3c(uint8_t *img, uint32_t a6);
void noop_16dba(uint8_t *img);
uint32_t func_18128(uint8_t *img, uint32_t a6);
uint32_t func_16576(uint8_t *img, uint32_t a6);
void func_10390(uint8_t *img, uint32_t a6);
uint32_t func_184d6(uint8_t *img, uint32_t a6);
uint32_t func_184cc(uint8_t *img, uint32_t a6);
uint32_t func_1851a(uint8_t *img, uint32_t a6);
uint32_t func_149da(uint8_t *img, uint32_t a6);
uint32_t func_16512(uint8_t *img, uint32_t a6);
uint32_t func_186b6(uint8_t *img, uint32_t a6);
uint32_t func_1857a(uint8_t *img, uint32_t a6);
uint32_t func_1881e(uint8_t *img, uint32_t a6);
uint32_t func_18890(uint8_t *img, uint32_t a6);
uint32_t func_18888(uint8_t *img, uint32_t a6);
uint32_t func_16554(uint8_t *img, uint32_t a6);
uint32_t func_18a98(uint8_t *img, uint32_t a6);
uint32_t func_17c42(uint8_t *img, uint32_t a6);
void func_168d4(uint8_t *img, uint32_t a6);
uint32_t func_14ee8(uint8_t *img, uint32_t a6);
uint32_t func_18cfc(uint8_t *img, uint32_t a6);
uint32_t func_18d26(uint8_t *img, uint32_t a6);
uint32_t func_16c48(uint8_t *img, uint32_t a6);
uint32_t func_16c00(uint8_t *img, uint32_t a6);
uint32_t func_18e06(uint8_t *img, uint32_t a6);
uint32_t func_18f6e(uint8_t *img, uint32_t a6);
void func_18fa2(uint8_t *img, uint32_t *a6);

/* Prototipos de los comandos de la tabla 0x19eca que aun no estaban declarados
 * aqui (las firmas salen de sus definiciones). */
void func_14d90(uint8_t *img, uint32_t a6);
uint32_t func_14e66(uint8_t *img, uint32_t a6);
void func_14d76(uint8_t *img, uint32_t a6);
uint32_t object_info_wait(uint8_t *img, uint32_t a6);
uint32_t func_15530(uint8_t *img, uint32_t a6);
uint32_t func_155dc(uint8_t *img);
void noop_19312(uint8_t *img);
uint32_t func_14f5e(uint8_t *img, uint32_t a6);
uint32_t func_14f74(uint8_t *img, uint32_t a6);
uint32_t func_15052(uint8_t *img, uint32_t a6);
uint32_t func_150ae(uint8_t *img, uint32_t a6);
uint32_t func_151b0(uint8_t *img, uint32_t a6);
uint32_t func_152ee(uint8_t *img, uint32_t a6);
uint32_t func_15382(uint8_t *img, uint32_t a6);
uint32_t func_15428(uint8_t *img, uint32_t a6);
void func_169b4(uint8_t *img);
void func_16a20(uint8_t *img);
void func_171b4(uint8_t *img);
uint32_t func_171fa(uint8_t *img, uint32_t a6);
void func_17678(uint8_t *img);
uint32_t func_175f6(uint8_t *img, uint32_t a6);
void func_1817a(uint8_t *img);
uint32_t track_name_print(uint8_t *img, uint32_t a6);
uint32_t func_1870a(uint8_t *img, uint32_t a6);
void func_173ae(uint8_t *img);
uint32_t func_173f2(uint8_t *img, uint32_t a6);
uint32_t func_16902(uint8_t *img, uint32_t a6);
void func_169ac(uint8_t *img);
void func_1699c(uint8_t *img);
void func_169a4(uint8_t *img);
uint32_t func_15594(uint8_t *img);
uint32_t func_155c4(uint8_t *img);

static uint32_t byte_handler(uint32_t b, uint8_t *img, uint32_t a6) {
    uint32_t n;
    switch (b) {
    case 5:  return func_11f64(img, a6);
    case 6:  return func_11f98(img, a6);
    case 7:  return func_11f02(img, a6);
    case 8:  return func_11f2e(img, a6);
    case 9:  return func_134da(img, a6);
    case 15: noop_11014(img); return a6;                    /* 0x11014: rts (no avanza) */
    case 17: func_141dc(img, a6); return addr_add(a6, 4);   /* 0x141dc: +1, blit, +3 */
    case 18: return func_104f4(img, a6);
    case 19: text_table_entry(img, a6); return addr_add(a6, 2);   /* 0x13f4c: d1=a6[1], a6+=2 */
    case 20: return draw_text_box(img, a6);
    case 21: return func_1780c(img, a6);
    case 25: return func_12076(img, a6);
    case 26: return func_12044(img, a6);
    case 27: return func_11fd8(img, a6);
    case 28: return func_1200a(img, a6);
    case 29: return func_13474(img, a6);
    case 32: return func_12e08(img, a6);
    case 33: return func_12fea(img, a6);
    case 35:
    case 63: hint_blit_entry(img, a6, &n); return n;
    case 36: n = a6; hint_blit_2(img, &n); return n;
    case 42: return func_121fa(img, a6);
    case 43: n = a6; hint_blit_stroke(img, &n); return n;
    case 46: return func_1228a(img, a6);
    case 51: n = a6; func_1350c(img, &n); return n;
    case 52: n = a6; func_13522(img, &n); return n;
    case 53: n = a6; func_13552(img, &n); return n;
    case 56: func_159d2(img, a6); return addr_add(a6, 4);   /* 0x159d2: push a6+4, pop al final */
    case 59: n = a6; script_store(img, &n); return n;
    case 60: n = a6; script_jump_zero(img, &n); return n;
    case 64: n = a6; hint_blit_3(img, &n); return n;
    case 68: copy_screen_a6(img);     return addr_add(a6, 1);   /* 0x121c4: addq.l #1,a6 + copy_screen */
    case 84: return func_12ea2(img, a6);
    case 90: set_palette_advance(img); return addr_add(a6, 1);       /* 0x162d8: addq.l #1,a6 */
    case 92: n = a6; func_18c8e(img, &n); return n;
    case 93: n = a6; func_18cb2(img, &n); return n;
    case 94: set_palette_from_entry(img, a6); return addr_add(a6, 2);   /* 0x162ce: d0=a6[1], a6+=2 */
    case 103: return load_eche_bank_cmd(img, a6);
    case 95: run_text_entry_1(img, a6); return addr_add(a6, 4);   /* 0x141ea: +1, blit, +3 */
    /* Resto de la tabla 0x19f62 (byte b -> base + long[b-1]). Cableado completo
     * para que el arranque nativo recorra los guiones; los 48 handlers con
     * firma uint32_t devuelven su A6, y el avance de los void sale del
     * desensamblado. Los wipes devuelven el A6 real (16dbc hace addq.l #1,a6):
     * 69 -> +1 (16dbc x2 con subq -1 en medio), 70-73/102 -> +1, 101 -> +2
     * (16dbc + el addq de 16c6c). */
    case 1: return func_14ac6(img, a6);
    case 2: return func_14bfa(img, a6);
    case 3: func_17930(img, a6); return addr_add(a6, 2);
    case 4: return func_179a2(img, a6);
    case 10: return func_132aa(img, a6);
    case 11: return func_132ec(img, a6);
    case 12: return func_1332e(img, a6);
    case 13: return reveal_pixel(img, a6);
    case 14: return func_13740(img, a6);
    case 16: return func_13702(img, a6);
    case 22: return func_1816e(img, a6);
    case 23: func_168a0(img, a6); return addr_add(a6, 2);
    case 24: return object_info_card(img, a6);
    case 30: return func_13490(img, a6);
    case 31: return func_134b2(img, a6);
    case 34: return func_138a6(img, a6);
    case 37: return func_1442c(img, a6);
    case 38: return prepare_room(img, a6);
    case 39: func_1687e(img, a6); return addr_add(a6, 2);
    case 40: return func_167aa(img, a6);
    case 41: return func_10b52(img, a6);
    case 44: return func_1224e(img, a6);
    case 45: return func_12240(img, a6);
    case 47: return func_178cc(img, a6);
    case 48: return func_17a1a(img, a6);
    case 49: return func_17ca4(img, a6);
    case 50: return func_176c0(img, a6);
    case 54: func_14602(img, a6); return addr_add(a6, 2);
    case 55: return func_10dfa(img, a6);
    case 57: textform_fwd_entry(img, a6); return addr_add(a6, 2);
    case 58: textform_rev_entry(img, a6); return addr_add(a6, 2);
    case 61: return func_15888(img, a6);
    case 62: return view_mode_cycle(img, a6);
    case 65: return func_13f2a(img, a6);
    case 66: return scene_mode_apply(img, a6);
    case 67: return scene_mode_draw(img, a6);
    case 69: return func_16b9c(img, a6);
    case 70: return func_16d78(img, a6);
    case 71: return func_16ce2(img, a6);
    case 72: return func_16ca6(img, a6);
    case 73: return func_16d3c(img, a6);
    case 74: noop_16dba(img); return a6;
    case 75: return func_18128(img, a6);
    case 76: return func_16576(img, a6);
    case 77: func_10390(img, a6); return addr_add(a6, 1);
    case 78: return func_184d6(img, a6);
    case 79: return func_184cc(img, a6);
    case 80: return func_1851a(img, a6);
    case 81: return func_149da(img, a6);
    case 82: return func_16512(img, a6);
    case 83: return func_186b6(img, a6);
    case 85: return func_1857a(img, a6);
    case 86: return func_1881e(img, a6);
    case 87: return func_18890(img, a6);
    case 88: return func_18888(img, a6);
    case 89: return func_16554(img, a6);
    case 91: return func_18a98(img, a6);
    case 96: return func_17c42(img, a6);
    case 97: func_168d4(img, a6); return addr_add(a6, 2);
    case 98: return func_14ee8(img, a6);
    case 99: return func_18cfc(img, a6);
    case 100: return func_18d26(img, a6);
    case 101: return func_16c48(img, a6);
    case 102: return func_16c00(img, a6);
    case 104: return func_18e06(img, a6);
    case 105: return func_18f6e(img, a6);
    case 106: n = a6; func_18fa2(img, &n); return n;
    default: return a6;
    }
}

uint32_t func_10630(uint8_t *img, uint32_t a6) {
    for (;;) {
        uint32_t b = img[a6];
        if (a6 == be32(img + A_TEMPL_PTR)) return a6;
        if (b >= 0x6b || b == 0) return a6;
        a6 = byte_handler(b, img, a6);
    }
}

/* FUN_000104f4 @ 0x104f4 (byte 18): the_command = word inline (a6[1..2]), walk_entry
 * con (templ, cmd-1) y devuelve A5 (el nuevo stream). */
uint32_t func_104f4(uint8_t *img, uint32_t a6) {
    uint32_t d1 = ((uint32_t)img[a6 + 2] << 8) | img[a6 + 1];
    wr16(img + A_THE_COMMAND, d1 & 0xffff);
    uint32_t a4o, a5o;
    walk_entry(img, A_TEMPL_DATA, (d1 - 1) & 0xffff, &a4o, &a5o);
    wr32(img + A_TEMPL_PTR, a4o);
    return a5o;
}

/* Handler de comando de la tabla 0x19eca. Solo los reconstruidos y sin A6
 * implicito (los de la cola compartida con 0x19f62 se invocan por el VM de bytes). */
static void kult_dbg_cmd(uint32_t cmd, int implemented);

static int cmd_implemented(uint32_t idx) { return idx <= 143; }  /* tabla completa */

/* Comandos 0xa000/0xb000 de la tabla 0x19eca. */
static void cmd_handler(uint8_t *img, uint32_t idx, uint32_t a6) {
    kult_dbg_cmd((idx + 1) | 0xa000, cmd_implemented(idx));
    switch (idx) {
    case 21: clear_flag(img); break;   /* 0x172ba */
    case 0: (void)func_14d90(img, a6); break;
    case 1: (void)func_14e66(img, a6); break;
    case 2: (void)func_14d76(img, a6); break;
    case 3: (void)object_info_wait(img, a6); break;
    case 4: (void)func_15530(img, a6); break;
    case 5: func_15588(img); break;
    case 6: func_155b8(img); break;
    case 7: func_155dc(img); break;
    case 8: noop_19312(img); break;
    case 9: (void)func_14f5e(img, a6); break;
    case 10: (void)func_14f74(img, a6); break;
    case 11: (void)func_15052(img, a6); break;
    case 12: (void)func_150ae(img, a6); break;
    case 13: (void)func_151b0(img, a6); break;
    case 14: (void)func_152ee(img, a6); break;
    case 15: (void)func_15382(img, a6); break;
    case 16: (void)func_15428(img, a6); break;
    case 17: func_169b4(img); break;
    case 18: func_16a20(img); break;
    case 19: func_171b4(img); break;
    case 20: (void)func_171fa(img, a6); break;
    case 22: (void)func_178cc(img, a6); break;
    case 23: func_17678(img); break;
    case 24: (void)func_175f6(img, a6); break;
    case 25: func_1817a(img); break;
    case 26: (void)track_name_print(img, a6); break;
    case 27: func_186ee(img); break;
    case 28: (void)func_1870a(img, a6); break;
    case 29: func_173ae(img); break;
    case 30: (void)func_173f2(img, a6); break;
    case 31: func_17472(img); break;
    case 32: (void)func_16902(img, a6); break;
    case 33: func_169ac(img); break;
    case 34: func_1699c(img); break;
    case 35: func_169a4(img); break;
    case 36: func_15594(img); break;
    case 37: func_155c4(img); break;
    case 38: (void)func_14ac6(img, a6); break;
    case 39: (void)func_14bfa(img, a6); break;
    case 40: (void)func_17930(img, a6); break;
    case 41: (void)func_179a2(img, a6); break;
    case 42: (void)func_11f64(img, a6); break;
    case 43: (void)func_11f98(img, a6); break;
    case 44: (void)func_11f02(img, a6); break;
    case 45: (void)func_11f2e(img, a6); break;
    case 46: (void)func_134da(img, a6); break;
    case 47: (void)func_132aa(img, a6); break;
    case 48: (void)func_132ec(img, a6); break;
    case 49: (void)func_1332e(img, a6); break;
    case 50: (void)reveal_pixel(img, a6); break;
    case 51: (void)func_13740(img, a6); break;
    case 52: noop_11014(img); break;
    case 53: (void)func_13702(img, a6); break;
    case 54: (void)func_141dc(img, a6); break;
    case 55: (void)func_104f4(img, a6); break;
    case 56: (void)text_table_entry(img, a6); break;
    case 57: (void)draw_text_box(img, a6); break;
    case 58: (void)func_1780c(img, a6); break;
    case 59: (void)func_1816e(img, a6); break;
    case 60: (void)func_168a0(img, a6); break;
    case 61: (void)object_info_card(img, a6); break;
    case 62: (void)func_12076(img, a6); break;
    case 63: (void)func_12044(img, a6); break;
    case 64: (void)func_11fd8(img, a6); break;
    case 65: (void)func_1200a(img, a6); break;
    case 66: (void)func_13474(img, a6); break;
    case 67: (void)func_13490(img, a6); break;
    case 68: (void)func_134b2(img, a6); break;
    case 69: (void)func_12e08(img, a6); break;
    case 70: (void)func_12fea(img, a6); break;
    case 71: (void)func_138a6(img, a6); break;
    case 72: { uint32_t o; hint_blit_entry(img, a6, &o); } break;
    case 73: { uint32_t n = a6; hint_blit_2(img, &n); } break;
    case 74: (void)func_1442c(img, a6); break;
    case 75: (void)prepare_room(img, a6); break;
    case 76: (void)func_1687e(img, a6); break;
    case 77: (void)func_167aa(img, a6); break;
    case 78: (void)func_10b52(img, a6); break;
    case 79: (void)func_121fa(img, a6); break;
    case 80: { uint32_t n = a6; hint_blit_stroke(img, &n); } break;
    case 81: (void)func_1224e(img, a6); break;
    case 82: (void)func_12240(img, a6); break;
    case 83: (void)func_1228a(img, a6); break;
    case 84: (void)func_178cc(img, a6); break;
    case 85: (void)func_17a1a(img, a6); break;
    case 86: (void)func_17ca4(img, a6); break;
    case 87: (void)func_176c0(img, a6); break;
    case 88: { uint32_t n = a6; func_1350c(img, &n); } break;
    case 89: { uint32_t n = a6; func_13522(img, &n); } break;
    case 90: { uint32_t n = a6; func_13552(img, &n); } break;
    case 91: (void)func_14602(img, a6); break;
    case 92: (void)func_10dfa(img, a6); break;
    case 93: (void)func_159d2(img, a6); break;
    case 94: (void)textform_fwd_entry(img, a6); break;
    case 95: (void)textform_rev_entry(img, a6); break;
    case 96: { uint32_t n = a6; script_store(img, &n); } break;
    case 97: { uint32_t n = a6; script_jump_zero(img, &n); } break;
    case 98: (void)func_15888(img, a6); break;
    case 99: (void)view_mode_cycle(img, a6); break;
    case 100: { uint32_t o; hint_blit_entry(img, a6, &o); } break;
    case 101: { uint32_t n = a6; hint_blit_3(img, &n); } break;
    case 102: (void)func_13f2a(img, a6); break;
    case 103: (void)scene_mode_apply(img, a6); break;
    case 104: (void)scene_mode_draw(img, a6); break;
    case 105: copy_screen_a6(img); break;
    case 106: (void)func_16b9c(img, a6); break;
    case 107: (void)func_16d78(img, a6); break;
    case 108: (void)func_16ce2(img, a6); break;
    case 109: (void)func_16ca6(img, a6); break;
    case 110: (void)func_16d3c(img, a6); break;
    case 111: noop_16dba(img); break;
    case 112: (void)func_18128(img, a6); break;
    case 113: (void)func_16576(img, a6); break;
    case 114: (void)func_10390(img, a6); break;
    case 115: (void)func_184d6(img, a6); break;
    case 116: (void)func_184cc(img, a6); break;
    case 117: (void)func_1851a(img, a6); break;
    case 118: (void)func_149da(img, a6); break;
    case 119: (void)func_16512(img, a6); break;
    case 120: (void)func_186b6(img, a6); break;
    case 121: (void)func_12ea2(img, a6); break;
    case 122: (void)func_1857a(img, a6); break;
    case 123: (void)func_1881e(img, a6); break;
    case 124: (void)func_18890(img, a6); break;
    case 125: (void)func_18888(img, a6); break;
    case 126: (void)func_16554(img, a6); break;
    case 127: set_palette_advance(img); break;
    case 128: (void)func_18a98(img, a6); break;
    case 129: { uint32_t n = a6; func_18c8e(img, &n); } break;
    case 130: { uint32_t n = a6; func_18cb2(img, &n); } break;
    case 131: (void)set_palette_from_entry(img, a6); break;
    case 132: (void)run_text_entry_1(img, a6); break;
    case 133: (void)func_17c42(img, a6); break;
    case 134: (void)func_168d4(img, a6); break;
    case 135: (void)func_14ee8(img, a6); break;
    case 136: (void)func_18cfc(img, a6); break;
    case 137: (void)func_18d26(img, a6); break;
    case 138: (void)func_16c48(img, a6); break;
    case 139: (void)func_16c00(img, a6); break;
    case 140: (void)load_eche_bank_cmd(img, a6); break;
    case 141: (void)func_18e06(img, a6); break;
    case 142: (void)func_18f6e(img, a6); break;
    case 143: { uint32_t n = a6; func_18fa2(img, &n); } break;
    default: break;
    }
}

/* Traza de diagnostico (KULT_DBG_CMDS=1): que comandos 0xa/b pide el guion y
 * si el port tiene handler para ellos. No altera la imagen. */
static void kult_dbg_cmd(uint32_t cmd, int implemented) {
    static int on = -1;
    if (on < 0) on = getenv("KULT_DBG_CMDS") != 0;
    if (on)
        fprintf(stderr, "[cmd] %04x idx=%u %s\n", (unsigned)(cmd & 0xffff),
                (unsigned)(cmd & 0x3ff) - 1, implemented ? "ok" : "SIN HANDLER");
}

void run_command_keep_sp(uint8_t *img, uint32_t a6) {
    uint32_t d1 = be16(img + A_THE_COMMAND);
    if (getenv("KULT_DBG_RUN"))
        fprintf(stderr, "[run] cmd=%04x a6=%05x\n", (unsigned)d1, (unsigned)a6);
    if (d1 == 0) return;
    uint32_t d2 = d1 & 0xf000;
    if (d2 == 0x0000) {
        func_10630(img, A_TEMPL_DATA + d1);
    } else if (d2 == 0xa000 || d2 == 0xb000) {
        cmd_handler(img, (d1 & 0x3ff) - 1, a6);
    } else if (d2 == 0x9000) {
        draw_text_box_variant(img, d1);                        /* caja de texto */
    } else {
        uint32_t a4o, a5o;
        walk_entry(img, A_TEMPL_DATA, (d1 & 0x3ff) - 1, &a4o, &a5o);
        wr32(img + A_TEMPL_PTR, a4o);
        func_10630(img, a5o);
    }
}

/* FUN_00010390 @ 0x10390: handler de byte que lee un word de a6[1..2], lo marca con el
 * nibble 0xf000 (comando de plantilla) y salta al despacho. */
void func_10390(uint8_t *img, uint32_t a6) {
    uint32_t d1 = ((img[a6 + 2] << 8) | img[a6 + 1]) | 0xf000;
    wr16(img + A_THE_COMMAND, d1 & 0xffff);
    run_command_keep_sp(img, a6);
}

/* FUN_000186ee @ 0x186ee: reproduce la paleta 0x10, lanza el comando 0x916b (caja de
 * texto) por el despacho y restaura la paleta actual. */
void set_palette_entry(uint8_t *img, uint32_t d0);
void set_palette_current(uint8_t *img);
void func_186ee(uint8_t *img) {
    set_palette_entry(img, 0x10);
    wr16(img + A_THE_COMMAND, 0x916b);
    run_command_keep_sp(img, 0);
    set_palette_current(img);
}
