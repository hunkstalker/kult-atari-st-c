# textblit.c

textblit.c — the register-driven proportional text blitters.  FUN_00011196 @ 0x11196: map D0's low word through a nibble/byte permutation and a 16-step rotate-through-X chain; the result lands in D0 (move.w d1,d0) and D1. FUN_00011050 @ 0x11050: forward blit — A0 dest (planes via movep, stride 0xa0), A1 source, A3 source stride, D2 outer/columns, D3 inner/rows, D7 pixel phase. FUN_00011102 @ 0x11102: reverse variant — each source word goes through FUN_00011196 first and the phase walks downward (D7 -= 4, A0 -= 8 on wrap).  Both blitters expand each source word to four plane bytes through the 0x55b02 scratch (the same `movep` nibble expansion blit_sprite uses) and merge them into the destination long with a mask computed from the expanded word itself. 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x11196` | `bit_reverse_word` | permutacion de palabra (resultado en D0/D1) | test_textblit |
| `0x11050` | `text_blit_fwd` | blit de texto directo (A0/A1/A3/D2/D3/D7) | test_textblit |
| `0x11102` | `text_blit_rev` | idem con palabra invertida por FUN_00011196 | test_textblit |
| `0x14138` | `run_text_entry_ex` | FUN_00014138 @ 0x14138: process one script/text entry at A6.  A6[0] selects a D4-entry skip in the text table at 0x41fb0; the entry's first two bytes are read (D3/D2), A6[1] is the character/position and A6[2] its flags. The function then computes the screen address (0x10fec) and dispatches to the forward blitter (FUN_00011050) or the bit-reversed one (FUN_00011102) depending on A6[1] bit 7.  The `_ex` form also reports the registers the original leaves for its caller: A0 (the 0x10fec screen address, before the -0x8000 back-buffer shift), D2/D3 (the entry's columns/rows) and D7 (the pixel phase).  The original preserves them across the blit with the `movem` at 0x141b0/0x141d6, and FUN_000141dc (0x141dc) feeds them straight into FUN_0001215e, so the reconstruction has to hand them back. | — |
| `0x14138` | `run_text_entry` | lee entrada de script A6, calcula pantalla y blitea texto | test_conventions, test_textblit |
| `0x16dbc` | `text_transition` | FUN_00016dbc @ 0x16dbc: `a6 += 1`; calcula la direccion de pantalla de (D0,D1) (0x10fec), avanza la entrada del banco A_TEXT_TABLE por D4 (0x10f8e), lee [rows][cols], fija el stride 0x1a534 = cols*2 y dibuja el elemento (0x126e0). Devuelve A0 (direccion de pantalla) y deja A5 = 0 (lo que los llamantes esperan: 0x16abe/0x16d3c lo usan como offset del descriptor y lo incrementan). | test_textblit |
| `0x16c6c` | `wipe_rows` | FUN_00016c6c @ 0x16c6c: cuerpo compartido del wipe de texto horizontal. A0 = base de pantalla (de 0x16dbc), D2/D3 grupos/filas, A5 = offset del descriptor. Rota D3 filas y reblitea (0x16b76/0x16b2e) esperando una VBL por iteracion (D7+1 = 4, o 3 si A6[0] != 0); al final reblitea la ultima fila si A6[0] != 0 y A6 += 1. | test_textblit |
| `0x16c48` | `wipe_b` | FUN_00016c48 @ 0x16c48: wipe de texto (d4 = 3, x = 0x23, y = 0xad o 0x31 segun A6[1]); cae en el cuerpo 0x16c6c con D2 = 2, D3 = 8. Devuelve A6 = entrada+2 (0x16dbc + el addq final de 0x16c6c): el VM original continua 2 bytes mas alla. | test_textblit |
| `0x16c00` | `wipe_a` | FUN_00016c00 @ 0x16c00: transicion de texto (d4 = 0x77); blitea un bloque y luego prepara el revelado (0x12e2e) con x=0x1c,y=0x8b. Devuelve A6 = entrada+1 (0x16dbc). | test_textblit |
| `0x16ca6` | `wipe_c` | FUN_00016ca6 @ 0x16ca6: wipe hacia abajo (34 VBL), D2 = 4, D3 0x43->... Devuelve A6 = entrada+1 (0x16dbc). | test_textblit |
| `0x16ce2` | `wipe_d` | FUN_00016ce2 @ 0x16ce2: wipe hacia arriba (68 VBL), D2 = 4, D3 0->... Devuelve A6 = entrada+1 (0x16dbc). | test_textblit |
| `0x16d3c` | `wipe_e` | FUN_00016d3c @ 0x16d3c: wipe con A5 +0x40 por pasada (16 VBL), A3 fijo. Devuelve A6 = entrada+1 (0x16dbc). | test_textblit |
| `0x16d78` | `wipe_f` | FUN_00016d78 @ 0x16d78: wipe con A5 -0x40 por pasada (15 VBL) desde 0x3c0. Devuelve A6 = entrada+1 (0x16dbc). | test_textblit |
| `0x141dc` | `text_entry_blit` | dispatch.c — el despacho de comandos y la VM de bytes del script. run_command @ 0x103a8 / run_command_keep_sp @ 0x103ae: leen `the_command` ($1a50e) y despac... | test_textblit |
| `0x141ea` | `run_text_entry_1` | FUN_000141ea @ 0x141ea: run FUN_00014138 on the entry at A6+1 (then A6 += 3, unused). | test_textblit |
| `0x11014` | `noop_11014` | FUN_00011014 @ 0x11014: a bare `rts` (no-op stub in the original). | test_textblit |
| `0x11026` | `text_blit_entry` | FUN_00011016 @ 0x11016 (A4 = 0x8000) y FUN_00011020 @ 0x11020 (A4 = 0), cuerpo comun FUN_00011026: avanza la entrada de texto de 0x28ad2 D4 posiciones, lee [rows][cols] y dibuja con el blit de texto hacia delante (stride cols*2) restando A4 a la dirección. | — |
| `0x11016` | `text_blit_ovl_a` | text blit ovl a | test_textblit |
| `0x11020` | `text_blit_ovl_b` | busca en la tabla 0x1a8f0 la entrada [0x40][$1acd2] y blitea los dos iconos (0x11020 + 0x11016) en (0x4a,0xe) con d4 = 0xad (0xac si no la encuentra), y cuen... | test_textblit |
| `0x11030` | `text_blit_body_a` | FUN_00011030 @ 0x11030: cuerpo de 0x11026 sin el avance de entrada (A1 ya apunta a [rows][cols]); direccion de pantalla menos A4 y blit hacia delante. | test_textblit |
| `0x110da` | `text_blit_rev_entry` | FUN_000110da @ 0x110da: como 0x11026 pero con el blit inverso (0x11102) y SIN restar A4; D4 = indice de la entrada de texto en 0x28ad2. | — |
| `0x10768` | `text_end` | FUN_00010768 @ 0x10768: `rts` pelado (destino del modo sin overlay, $1acc6 == 0x87). | test_textblit |
| `0x122ee` | `prompt` | prompt | test_dispatch, test_textblit |
| `0x12292` | `prompt_wait` | wipe grande de texto. Primer 0x16dbc (d4=3, x=0x23, y=0xad) + 0x16b2e(2,7); segundo 0x16dbc (d4=0x77, x=0x20, y=0x5e) + 0x16b2e(3,0x47); prompt (0x12292) y 7... | test_family151, test_handlers, test_textblit, test_textbox |
| `0x16abe` | `grid_bands` | (cmd idx 10): rejilla de texto 0x16abe + espera de clic (0x167be) + repintados, y despacho por la tabla de objetos o por el umbral (0x9005) en la cola 0x15022. | test_loop |
| `0x16b9c` | `wipe_72` | wipe 72 | test_textblit |

## Estado (globales A_* usadas)

- `A_BLIT_SCRATCH`
- `A_DAT_1A49E`
- `A_DAT_1A534`
- `A_DAT_1AB40`
- `A_DAT_1AB41`
- `A_DAT_1AB42`
- `A_DAT_1AB43`
- `A_DAT_1ACD4`
- `A_INPUT_BUTTONS`
- `A_TEXT_TABLE`

