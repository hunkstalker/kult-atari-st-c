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
void  text_entry_blit(uint8_t *img, uint32_t a6);
void  run_text_entry_1(uint8_t *img, uint32_t a6);
void  text_table_entry(uint8_t *img, uint32_t a6);
void  anim_entry_a(uint8_t *img, uint32_t a6);
void  set_palette_from_entry(uint8_t *img, uint32_t a6);
void  draw_text_box_variant(uint8_t *img, uint32_t the_command);
void  clear_flag(uint8_t *img);
void  mode_flag_wrap(uint8_t *img);
void  mode_queue_wrap(uint8_t *img);
uint32_t inline_cmd_walk(uint8_t *img, uint32_t a6);
uint32_t load_eche_bank_cmd(uint8_t *img, uint32_t a6);
void  dispatch_cmd(uint8_t *img);
void  clear_globals(uint8_t *img);
uint32_t overlay_dispatch_1(uint8_t *img, uint32_t a6);
uint32_t overlay_dispatch_2(uint8_t *img, uint32_t a6);
uint32_t blit_dispatch_a(uint8_t *img, uint32_t a6);
uint32_t blit_dispatch_b(uint8_t *img, uint32_t a6);
uint32_t elem_blit_vbl(uint8_t *img, uint32_t a6);
uint32_t draw_text_box(uint8_t *img, uint32_t a6);
uint32_t overlay_dispatch_6(uint8_t *img, uint32_t a6);
uint32_t overlay_dispatch_5(uint8_t *img, uint32_t a6);
uint32_t overlay_dispatch_3(uint8_t *img, uint32_t a6);
uint32_t overlay_dispatch_4(uint8_t *img, uint32_t a6);
uint32_t icon_reveal(uint8_t *img, uint32_t a6);
uint32_t reveal_box_a(uint8_t *img, uint32_t a6);
uint32_t hint_blit_entry_b(uint8_t *img, uint32_t a6);
void     hint_blit_entry(uint8_t *img, uint32_t a6, uint32_t *a6_out);
void     hint_blit_2(uint8_t *img, uint32_t *a6);
uint32_t icon_blit_handler(uint8_t *img, uint32_t a6);
void     hint_blit_stroke(uint8_t *img, uint32_t *a6);
uint32_t prompt_wait_entry(uint8_t *img, uint32_t a6);
void     func_1350c(uint8_t *img, uint32_t *a6);
void     script_pop(uint8_t *img, uint32_t *a6);
void     script_call(uint8_t *img, uint32_t *a6);
void     script_store(uint8_t *img, uint32_t *a6);
void     script_jump_zero(uint8_t *img, uint32_t *a6);
void     hint_blit_3(uint8_t *img, uint32_t *a6);
uint32_t reveal_box_b(uint8_t *img, uint32_t a6);
void     set_palette_advance(uint8_t *img);
void     icon_anim_a(uint8_t *img, uint32_t *a6);
void     icon_anim_b(uint8_t *img, uint32_t *a6);

/* Un handler de byte de la VM (tabla 0x19f62): recibe a6 y devuelve el a6 nuevo.
 * El avance de A6 es el neto del handler original (los `addq`/push-pop), dato que
 * no modela la firma C, así que se codifica aquí. Solo los reconstruidos. */
uint32_t sample_menu_a(uint8_t *img, uint32_t a6);
uint32_t sample_menu_b(uint8_t *img, uint32_t a6);
void obj_text_box(uint8_t *img, uint32_t a6);
uint32_t class30_apply(uint8_t *img, uint32_t a6);
uint32_t reveal_box_build(uint8_t *img, uint32_t a6);
uint32_t reveal_box_wait_a(uint8_t *img, uint32_t a6);
uint32_t reveal_box_wait_b(uint8_t *img, uint32_t a6);
uint32_t reveal_pixel(uint8_t *img, uint32_t a6);
uint32_t scale_loop(uint8_t *img, uint32_t a6);
uint32_t scale_prep(uint8_t *img, uint32_t a6);
uint32_t obj_find_or_exit(uint8_t *img, uint32_t a6);
uint32_t track5_apply(uint8_t *img, uint32_t a6);
void obj_name_snd(uint8_t *img, uint32_t a6);
uint32_t object_info_card(uint8_t *img, uint32_t a6);
uint32_t icon_reveal_h(uint8_t *img, uint32_t a6);
uint32_t icon_reveal_v(uint8_t *img, uint32_t a6);
uint32_t scale_anim(uint8_t *img, uint32_t a6);
uint32_t cmd74_wrap(uint8_t *img, uint32_t a6);
uint32_t prepare_room(uint8_t *img, uint32_t a6);
void obj_name(uint8_t *img, uint32_t a6);
uint32_t obj_wait(uint8_t *img, uint32_t a6);
uint32_t text_window_entry(uint8_t *img, uint32_t a6);
uint32_t wait_button_b(uint8_t *img, uint32_t a6);
uint32_t wait_button_a(uint8_t *img, uint32_t a6);
uint32_t obj_consume(uint8_t *img, uint32_t a6);
uint32_t view_change(uint8_t *img, uint32_t a6);
uint32_t use_object(uint8_t *img, uint32_t a6);
uint32_t track_icon_anim(uint8_t *img, uint32_t a6);
void scene_mode_draw_wrap(uint8_t *img, uint32_t a6);
uint32_t text_box_icon(uint8_t *img, uint32_t a6);
void textform_fwd_entry(uint8_t *img, uint32_t a6);
void textform_rev_entry(uint8_t *img, uint32_t a6);
uint32_t icon_ring_loop(uint8_t *img, uint32_t a6);
uint32_t view_mode_cycle(uint8_t *img, uint32_t a6);
uint32_t text_table_paint(uint8_t *img, uint32_t a6);
uint32_t scene_mode_apply(uint8_t *img, uint32_t a6);
uint32_t scene_mode_draw(uint8_t *img, uint32_t a6);
uint32_t wipe_72(uint8_t *img, uint32_t a6);
uint32_t wipe_f(uint8_t *img, uint32_t a6);
uint32_t wipe_d(uint8_t *img, uint32_t a6);
uint32_t wipe_c(uint8_t *img, uint32_t a6);
uint32_t wipe_e(uint8_t *img, uint32_t a6);
void noop_16dba(uint8_t *img);
uint32_t class30_dispatch(uint8_t *img, uint32_t a6);
uint32_t update_icon_list_wrap(uint8_t *img, uint32_t a6);
void dispatch_a6(uint8_t *img, uint32_t a6);
uint32_t sample_mark_b(uint8_t *img, uint32_t a6);
uint32_t sample_mark_a(uint8_t *img, uint32_t a6);
uint32_t sample_mark_c(uint8_t *img, uint32_t a6);
uint32_t action_menu(uint8_t *img, uint32_t a6);
uint32_t hint_icons_wrap(uint8_t *img, uint32_t a6);
uint32_t sample_find(uint8_t *img, uint32_t a6);
uint32_t sample_find_wrap(uint8_t *img, uint32_t a6);
uint32_t copy_rows_vbl(uint8_t *img, uint32_t a6);
uint32_t bank_blit_b(uint8_t *img, uint32_t a6);
uint32_t bank_blit_a(uint8_t *img, uint32_t a6);
uint32_t draw_hint_icons_wrap(uint8_t *img, uint32_t a6);
uint32_t attract(uint8_t *img, uint32_t a6);
uint32_t obj_refresh(uint8_t *img, uint32_t a6);
void obj_name_clean(uint8_t *img, uint32_t a6);
uint32_t cmd135(uint8_t *img, uint32_t a6);
uint32_t obj_dec(uint8_t *img, uint32_t a6);
uint32_t draw_selection_frame_wrap(uint8_t *img, uint32_t a6);
uint32_t wipe_b(uint8_t *img, uint32_t a6);
uint32_t wipe_a(uint8_t *img, uint32_t a6);
uint32_t sample_pair_a(uint8_t *img, uint32_t a6);
uint32_t supexec_wrap(uint8_t *img, uint32_t a6);
void supexec_a(uint8_t *img, uint32_t *a6);

/* Prototipos de los comandos de la tabla 0x19eca que aun no estaban declarados
 * aqui (las firmas salen de sus definiciones). */
void sample_menu_entry_b(uint8_t *img, uint32_t a6);
uint32_t overlay_menu(uint8_t *img, uint32_t a6);
void sample_menu_entry_a(uint8_t *img, uint32_t a6);
uint32_t object_info_wait(uint8_t *img, uint32_t a6);
uint32_t mode_queue(uint8_t *img, uint32_t a6);
uint32_t clock(uint8_t *img);
void noop_19312(uint8_t *img);
uint32_t anim_threshold2(uint8_t *img, uint32_t a6);
uint32_t grid_wait(uint8_t *img, uint32_t a6);
uint32_t obj_action_select(uint8_t *img, uint32_t a6);
uint32_t obj_action(uint8_t *img, uint32_t a6);
uint32_t row_transition(uint8_t *img, uint32_t a6);
uint32_t cursor_anim_wait(uint8_t *img, uint32_t a6);
uint32_t palette_cmd(uint8_t *img, uint32_t a6);
uint32_t name_prompt(uint8_t *img, uint32_t a6);
void obj_wait_cmd(uint8_t *img);
void portrait_intro(uint8_t *img);
void sel_timer_a(uint8_t *img);
uint32_t track_select_a(uint8_t *img, uint32_t a6);
void obj_clear_anim(uint8_t *img);
uint32_t obj_seal(uint8_t *img, uint32_t a6);
void obj_mark(uint8_t *img);
uint32_t track_name_print(uint8_t *img, uint32_t a6);
uint32_t track_walk(uint8_t *img, uint32_t a6);
void sel_timer_b(uint8_t *img);
uint32_t track_select_b(uint8_t *img, uint32_t a6);
uint32_t obj_name_track(uint8_t *img, uint32_t a6);
void obj_wait_cmd_snd(uint8_t *img);
void obj_wait_cmd_a(uint8_t *img);
void obj_wait_cmd_b(uint8_t *img);
uint32_t load_room(uint8_t *img);
uint32_t save_room(uint8_t *img);

static uint32_t byte_handler(uint32_t b, uint8_t *img, uint32_t a6) {
    uint32_t n;
    switch (b) {
    case 5:  return overlay_dispatch_1(img, a6);
    case 6:  return overlay_dispatch_2(img, a6);
    case 7:  return blit_dispatch_a(img, a6);
    case 8:  return blit_dispatch_b(img, a6);
    case 9:  return elem_blit_vbl(img, a6);
    case 15: noop_11014(img); return a6;                    /* 0x11014: rts (no avanza) */
    case 17: text_entry_blit(img, a6); return addr_add(a6, 4);   /* 0x141dc: +1, blit, +3 */
    case 18: return inline_cmd_walk(img, a6);
    case 19: text_table_entry(img, a6); return addr_add(a6, 2);   /* 0x13f4c: d1=a6[1], a6+=2 */
    case 20: return draw_text_box(img, a6);
    case 21: return obj_find_or_exit(img, a6);
    case 25: return overlay_dispatch_6(img, a6);
    case 26: return overlay_dispatch_5(img, a6);
    case 27: return overlay_dispatch_3(img, a6);
    case 28: return overlay_dispatch_4(img, a6);
    case 29: return icon_reveal(img, a6);
    case 32: return reveal_box_a(img, a6);
    case 33: return hint_blit_entry_b(img, a6);
    case 35:
    case 63: hint_blit_entry(img, a6, &n); return n;
    case 36: n = a6; hint_blit_2(img, &n); return n;
    case 42: return icon_blit_handler(img, a6);
    case 43: n = a6; hint_blit_stroke(img, &n); return n;
    case 46: return prompt_wait_entry(img, a6);
    case 51: n = a6; func_1350c(img, &n); return n;
    case 52: n = a6; script_pop(img, &n); return n;
    case 53: n = a6; script_call(img, &n); return n;
    case 56: anim_entry_a(img, a6); return addr_add(a6, 4);   /* 0x159d2: push a6+4, pop al final */
    case 59: n = a6; script_store(img, &n); return n;
    case 60: n = a6; script_jump_zero(img, &n); return n;
    case 64: n = a6; hint_blit_3(img, &n); return n;
    case 68: copy_screen_a6(img);     return addr_add(a6, 1);   /* 0x121c4: addq.l #1,a6 + copy_screen */
    case 84: return reveal_box_b(img, a6);
    case 90: set_palette_advance(img); return addr_add(a6, 1);       /* 0x162d8: addq.l #1,a6 */
    case 92: n = a6; icon_anim_a(img, &n); return n;
    case 93: n = a6; icon_anim_b(img, &n); return n;
    case 94: set_palette_from_entry(img, a6); return addr_add(a6, 2);   /* 0x162ce: d0=a6[1], a6+=2 */
    case 103: return load_eche_bank_cmd(img, a6);
    case 95: run_text_entry_1(img, a6); return addr_add(a6, 4);   /* 0x141ea: +1, blit, +3 */
    /* Resto de la tabla 0x19f62 (byte b -> base + long[b-1]). Cableado completo
     * para que el arranque nativo recorra los guiones; los 48 handlers con
     * firma uint32_t devuelven su A6, y el avance de los void sale del
     * desensamblado. Los wipes devuelven el A6 real (16dbc hace addq.l #1,a6):
     * 69 -> +1 (16dbc x2 con subq -1 en medio), 70-73/102 -> +1, 101 -> +2
     * (16dbc + el addq de 16c6c). */
    case 1: return sample_menu_a(img, a6);
    case 2: return sample_menu_b(img, a6);
    case 3: obj_text_box(img, a6); return addr_add(a6, 2);
    case 4: return class30_apply(img, a6);
    case 10: return reveal_box_build(img, a6);
    case 11: return reveal_box_wait_a(img, a6);
    case 12: return reveal_box_wait_b(img, a6);
    case 13: return reveal_pixel(img, a6);
    case 14: return scale_loop(img, a6);
    case 16: return scale_prep(img, a6);
    case 22: return track5_apply(img, a6);
    case 23: obj_name_snd(img, a6); return addr_add(a6, 2);
    case 24: return object_info_card(img, a6);
    case 30: return icon_reveal_h(img, a6);
    case 31: return icon_reveal_v(img, a6);
    case 34: return scale_anim(img, a6);
    case 37: return cmd74_wrap(img, a6);
    case 38: return prepare_room(img, a6);
    case 39: obj_name(img, a6); return addr_add(a6, 2);
    case 40: return obj_wait(img, a6);
    case 41: return text_window_entry(img, a6);
    case 44: return wait_button_b(img, a6);
    case 45: return wait_button_a(img, a6);
    case 47: return obj_consume(img, a6);
    case 48: return view_change(img, a6);
    case 49: return use_object(img, a6);
    case 50: return track_icon_anim(img, a6);
    case 54: scene_mode_draw_wrap(img, a6); return addr_add(a6, 2);
    case 55: return text_box_icon(img, a6);
    case 57: textform_fwd_entry(img, a6); return addr_add(a6, 2);
    case 58: textform_rev_entry(img, a6); return addr_add(a6, 2);
    case 61: return icon_ring_loop(img, a6);
    case 62: return view_mode_cycle(img, a6);
    case 65: return text_table_paint(img, a6);
    case 66: return scene_mode_apply(img, a6);
    case 67: return scene_mode_draw(img, a6);
    case 69: return wipe_72(img, a6);
    case 70: return wipe_f(img, a6);
    case 71: return wipe_d(img, a6);
    case 72: return wipe_c(img, a6);
    case 73: return wipe_e(img, a6);
    case 74: noop_16dba(img); return a6;
    case 75: return class30_dispatch(img, a6);
    case 76: return update_icon_list_wrap(img, a6);
    case 77: dispatch_a6(img, a6); return addr_add(a6, 1);
    case 78: return sample_mark_b(img, a6);
    case 79: return sample_mark_a(img, a6);
    case 80: return sample_mark_c(img, a6);
    case 81: return action_menu(img, a6);
    case 82: return hint_icons_wrap(img, a6);
    case 83: return sample_find(img, a6);
    case 85: return sample_find_wrap(img, a6);
    case 86: return copy_rows_vbl(img, a6);
    case 87: return bank_blit_b(img, a6);
    case 88: return bank_blit_a(img, a6);
    case 89: return draw_hint_icons_wrap(img, a6);
    case 91: return attract(img, a6);
    case 96: return obj_refresh(img, a6);
    case 97: obj_name_clean(img, a6); return addr_add(a6, 2);
    case 98: return cmd135(img, a6);
    case 99: return obj_dec(img, a6);
    case 100: return draw_selection_frame_wrap(img, a6);
    case 101: return wipe_b(img, a6);
    case 102: return wipe_a(img, a6);
    case 104: return sample_pair_a(img, a6);
    case 105: return supexec_wrap(img, a6);
    case 106: n = a6; supexec_a(img, &n); return n;
    default: return a6;
    }
}

uint32_t vm_run(uint8_t *img, uint32_t a6) {
    for (;;) {
        uint32_t b = img[a6];
        if (a6 == be32(img + A_TEMPL_PTR)) return a6;
        if (b >= 0x6b || b == 0) return a6;
        a6 = byte_handler(b, img, a6);
    }
}

/* FUN_000104f4 @ 0x104f4 (byte 18): the_command = word inline (a6[1..2]), walk_entry
 * con (templ, cmd-1) y devuelve A5 (el nuevo stream). */
uint32_t inline_cmd_walk(uint8_t *img, uint32_t a6) {
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
    case 0: (void)sample_menu_entry_b(img, a6); break;
    case 1: (void)overlay_menu(img, a6); break;
    case 2: (void)sample_menu_entry_a(img, a6); break;
    case 3: (void)object_info_wait(img, a6); break;
    case 4: (void)mode_queue(img, a6); break;
    case 5: mode_flag_wrap(img); break;
    case 6: mode_queue_wrap(img); break;
    case 7: clock(img); break;
    case 8: noop_19312(img); break;
    case 9: (void)anim_threshold2(img, a6); break;
    case 10: (void)grid_wait(img, a6); break;
    case 11: (void)obj_action_select(img, a6); break;
    case 12: (void)obj_action(img, a6); break;
    case 13: (void)row_transition(img, a6); break;
    case 14: (void)cursor_anim_wait(img, a6); break;
    case 15: (void)palette_cmd(img, a6); break;
    case 16: (void)name_prompt(img, a6); break;
    case 17: obj_wait_cmd(img); break;
    case 18: portrait_intro(img); break;
    case 19: sel_timer_a(img); break;
    case 20: (void)track_select_a(img, a6); break;
    case 22: (void)obj_consume(img, a6); break;
    case 23: obj_clear_anim(img); break;
    case 24: (void)obj_seal(img, a6); break;
    case 25: obj_mark(img); break;
    case 26: (void)track_name_print(img, a6); break;
    case 27: dispatch_cmd(img); break;
    case 28: (void)track_walk(img, a6); break;
    case 29: sel_timer_b(img); break;
    case 30: (void)track_select_b(img, a6); break;
    case 31: clear_globals(img); break;
    case 32: (void)obj_name_track(img, a6); break;
    case 33: obj_wait_cmd_snd(img); break;
    case 34: obj_wait_cmd_a(img); break;
    case 35: obj_wait_cmd_b(img); break;
    case 36: load_room(img); break;
    case 37: save_room(img); break;
    case 38: (void)sample_menu_a(img, a6); break;
    case 39: (void)sample_menu_b(img, a6); break;
    case 40: (void)obj_text_box(img, a6); break;
    case 41: (void)class30_apply(img, a6); break;
    case 42: (void)overlay_dispatch_1(img, a6); break;
    case 43: (void)overlay_dispatch_2(img, a6); break;
    case 44: (void)blit_dispatch_a(img, a6); break;
    case 45: (void)blit_dispatch_b(img, a6); break;
    case 46: (void)elem_blit_vbl(img, a6); break;
    case 47: (void)reveal_box_build(img, a6); break;
    case 48: (void)reveal_box_wait_a(img, a6); break;
    case 49: (void)reveal_box_wait_b(img, a6); break;
    case 50: (void)reveal_pixel(img, a6); break;
    case 51: (void)scale_loop(img, a6); break;
    case 52: noop_11014(img); break;
    case 53: (void)scale_prep(img, a6); break;
    case 54: (void)text_entry_blit(img, a6); break;
    case 55: (void)inline_cmd_walk(img, a6); break;
    case 56: (void)text_table_entry(img, a6); break;
    case 57: (void)draw_text_box(img, a6); break;
    case 58: (void)obj_find_or_exit(img, a6); break;
    case 59: (void)track5_apply(img, a6); break;
    case 60: (void)obj_name_snd(img, a6); break;
    case 61: (void)object_info_card(img, a6); break;
    case 62: (void)overlay_dispatch_6(img, a6); break;
    case 63: (void)overlay_dispatch_5(img, a6); break;
    case 64: (void)overlay_dispatch_3(img, a6); break;
    case 65: (void)overlay_dispatch_4(img, a6); break;
    case 66: (void)icon_reveal(img, a6); break;
    case 67: (void)icon_reveal_h(img, a6); break;
    case 68: (void)icon_reveal_v(img, a6); break;
    case 69: (void)reveal_box_a(img, a6); break;
    case 70: (void)hint_blit_entry_b(img, a6); break;
    case 71: (void)scale_anim(img, a6); break;
    case 72: { uint32_t o; hint_blit_entry(img, a6, &o); } break;
    case 73: { uint32_t n = a6; hint_blit_2(img, &n); } break;
    case 74: (void)cmd74_wrap(img, a6); break;
    case 75: (void)prepare_room(img, a6); break;
    case 76: (void)obj_name(img, a6); break;
    case 77: (void)obj_wait(img, a6); break;
    case 78: (void)text_window_entry(img, a6); break;
    case 79: (void)icon_blit_handler(img, a6); break;
    case 80: { uint32_t n = a6; hint_blit_stroke(img, &n); } break;
    case 81: (void)wait_button_b(img, a6); break;
    case 82: (void)wait_button_a(img, a6); break;
    case 83: (void)prompt_wait_entry(img, a6); break;
    case 84: (void)obj_consume(img, a6); break;
    case 85: (void)view_change(img, a6); break;
    case 86: (void)use_object(img, a6); break;
    case 87: (void)track_icon_anim(img, a6); break;
    case 88: { uint32_t n = a6; func_1350c(img, &n); } break;
    case 89: { uint32_t n = a6; script_pop(img, &n); } break;
    case 90: { uint32_t n = a6; script_call(img, &n); } break;
    case 91: (void)scene_mode_draw_wrap(img, a6); break;
    case 92: (void)text_box_icon(img, a6); break;
    case 93: (void)anim_entry_a(img, a6); break;
    case 94: (void)textform_fwd_entry(img, a6); break;
    case 95: (void)textform_rev_entry(img, a6); break;
    case 96: { uint32_t n = a6; script_store(img, &n); } break;
    case 97: { uint32_t n = a6; script_jump_zero(img, &n); } break;
    case 98: (void)icon_ring_loop(img, a6); break;
    case 99: (void)view_mode_cycle(img, a6); break;
    case 100: { uint32_t o; hint_blit_entry(img, a6, &o); } break;
    case 101: { uint32_t n = a6; hint_blit_3(img, &n); } break;
    case 102: (void)text_table_paint(img, a6); break;
    case 103: (void)scene_mode_apply(img, a6); break;
    case 104: (void)scene_mode_draw(img, a6); break;
    case 105: copy_screen_a6(img); break;
    case 106: (void)wipe_72(img, a6); break;
    case 107: (void)wipe_f(img, a6); break;
    case 108: (void)wipe_d(img, a6); break;
    case 109: (void)wipe_c(img, a6); break;
    case 110: (void)wipe_e(img, a6); break;
    case 111: noop_16dba(img); break;
    case 112: (void)class30_dispatch(img, a6); break;
    case 113: (void)update_icon_list_wrap(img, a6); break;
    case 114: (void)dispatch_a6(img, a6); break;
    case 115: (void)sample_mark_b(img, a6); break;
    case 116: (void)sample_mark_a(img, a6); break;
    case 117: (void)sample_mark_c(img, a6); break;
    case 118: (void)action_menu(img, a6); break;
    case 119: (void)hint_icons_wrap(img, a6); break;
    case 120: (void)sample_find(img, a6); break;
    case 121: (void)reveal_box_b(img, a6); break;
    case 122: (void)sample_find_wrap(img, a6); break;
    case 123: (void)copy_rows_vbl(img, a6); break;
    case 124: (void)bank_blit_b(img, a6); break;
    case 125: (void)bank_blit_a(img, a6); break;
    case 126: (void)draw_hint_icons_wrap(img, a6); break;
    case 127: set_palette_advance(img); break;
    case 128: (void)attract(img, a6); break;
    case 129: { uint32_t n = a6; icon_anim_a(img, &n); } break;
    case 130: { uint32_t n = a6; icon_anim_b(img, &n); } break;
    case 131: (void)set_palette_from_entry(img, a6); break;
    case 132: (void)run_text_entry_1(img, a6); break;
    case 133: (void)obj_refresh(img, a6); break;
    case 134: (void)obj_name_clean(img, a6); break;
    case 135: (void)cmd135(img, a6); break;
    case 136: (void)obj_dec(img, a6); break;
    case 137: (void)draw_selection_frame_wrap(img, a6); break;
    case 138: (void)wipe_b(img, a6); break;
    case 139: (void)wipe_a(img, a6); break;
    case 140: (void)load_eche_bank_cmd(img, a6); break;
    case 141: (void)sample_pair_a(img, a6); break;
    case 142: (void)supexec_wrap(img, a6); break;
    case 143: { uint32_t n = a6; supexec_a(img, &n); } break;
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
        vm_run(img, A_TEMPL_DATA + d1);
    } else if (d2 == 0xa000 || d2 == 0xb000) {
        cmd_handler(img, (d1 & 0x3ff) - 1, a6);
    } else if (d2 == 0x9000) {
        draw_text_box_variant(img, d1);                        /* caja de texto */
    } else {
        uint32_t a4o, a5o;
        walk_entry(img, A_TEMPL_DATA, (d1 & 0x3ff) - 1, &a4o, &a5o);
        wr32(img + A_TEMPL_PTR, a4o);
        vm_run(img, a5o);
    }
}

/* FUN_00010390 @ 0x10390: handler de byte que lee un word de a6[1..2], lo marca con el
 * nibble 0xf000 (comando de plantilla) y salta al despacho. */
void dispatch_a6(uint8_t *img, uint32_t a6) {
    uint32_t d1 = ((img[a6 + 2] << 8) | img[a6 + 1]) | 0xf000;
    wr16(img + A_THE_COMMAND, d1 & 0xffff);
    run_command_keep_sp(img, a6);
}

/* FUN_000186ee @ 0x186ee: reproduce la paleta 0x10, lanza el comando 0x916b (caja de
 * texto) por el despacho y restaura la paleta actual. */
void set_palette_entry(uint8_t *img, uint32_t d0);
void set_palette_current(uint8_t *img);
void dispatch_cmd(uint8_t *img) {
    set_palette_entry(img, 0x10);
    wr16(img + A_THE_COMMAND, 0x916b);
    run_command_keep_sp(img, 0);
    set_palette_current(img);
}
