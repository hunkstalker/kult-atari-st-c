/* kult_api.h — API pública del core reconstruido (frontera con el motor).
 *
 * El core expone un conjunto reducido de símbolos (los que el motor nativo
 * necesita); el resto queda local vía el version-script `kult_core.map` al
 * construir `build/libkultcore.so` (`make core`). Así, al enlazar con SDL y
 * otras librerías no hay colisiones con los ~400 símbolos internos.
 *
 * El build diferencial (harness) NO usa el version-script: necesita ver todos
 * los símbolos por dlsym. La lista de exportados vive en `kult_core.map`; el
 * test `test/test_visibility.py` comprueba que solo se exporta esa lista.
 */
#ifndef KULT_API_H
#define KULT_API_H

#include <stdint.h>

#if defined(_WIN32)
#  define KULT_API __declspec(dllexport)
#elif defined(__GNUC__)
#  define KULT_API __attribute__((visibility("default")))
#else
#  define KULT_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

KULT_API void copy_screen(uint8_t *img);
KULT_API void crt0_init(uint8_t *img);
KULT_API void cursor_draw(uint8_t *img);
KULT_API void cursor_restore(uint8_t *img);
KULT_API void draw_hint_icons(uint8_t *img);
KULT_API void game_loop(uint8_t *img, uint32_t a7);
KULT_API uint32_t gemdos_cnecin(uint8_t *img);
KULT_API uint32_t init_hardware(uint8_t *img, uint32_t d2);
KULT_API void load_eche_bank(uint8_t *img, uint32_t d0, uint32_t d1);
KULT_API void load_fond(uint8_t *img);
KULT_API void load_named_screen(uint8_t *img);
KULT_API int32_t load_resource_list_a(uint8_t *img);
KULT_API int32_t load_resource_list_b(uint8_t *img);
KULT_API int32_t load_scena(uint8_t *img);
KULT_API void music_stop(uint8_t *img, uint32_t d2);
KULT_API void music_tick(uint8_t *img, uint32_t a3, uint32_t a6, uint32_t d7);
KULT_API uint32_t pal_next_byte(uint8_t *img);
KULT_API void prompt_wait(uint8_t *img);
KULT_API void psg_trap3(uint8_t *img, uint32_t d0, uint32_t d2, uint32_t d7, uint32_t *d0_out);
KULT_API void run_command_keep_sp(uint8_t *img, uint32_t a6);
KULT_API void save_clear_bin(uint8_t *img);
KULT_API int32_t save_scena(uint8_t *img);
KULT_API void set_palette(uint8_t *img);
KULT_API void sound_isr_back(uint8_t *img);
KULT_API void sound_isr_fwd(uint8_t *img);
KULT_API void sound_isr_load(uint8_t *img);
KULT_API void sound_stop(uint8_t *img);
KULT_API void sound_stop_supexec(uint8_t *img);
KULT_API void swap_screens(uint8_t *img);
KULT_API void wait_frame(uint8_t *img);
KULT_API void wait_frame_cursor(uint8_t *img);

#ifdef __cplusplus
}
#endif

#endif /* KULT_API_H */
