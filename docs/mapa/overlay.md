# overlay.c

overlay.c — blitters de planos de los overlays de texto (0x111b6 y satelites).  overlay_compose_planes      @ 0x111b6 : compone D3 filas de un bitmap de A1 (stride 0x1a534) en el descriptor 0xd8f0, con 1..4 planos segun D2 y desplazamiento de pixel D0 y de sub-word D1&3; espera VBL por pasada y aplica el descriptor a A0 (copy_scratch_rows). Repite D1+1 veces. overlay_compose_planes_wrap @ 0x111ac : D3-=1, D2=5, D1=(D1<<2)-1. overlay_compose_planes32    @ 0x114ba : variante de 32 bits; el acarreo vive en D6 y el sub-word usa (3 - (D1&3)). overlay_compose_planes32_wrap @ 0x114b0: wrapper de 0x114ba. overlay_compose_masked      @ 0x117fe : compone dos fuentes (A0 mascara, A1 dato) con ror32(0x80000000,D0); el flag 0x1a53e se resta a D2 por pasada. overlay_compose_masked_wrap @ 0x117de : fija 0x1a53e=1. overlay_compose_shift       @ 0x11afa : variante roxl (flag 0x1a53e). overlay_compose_shift_wrap  @ 0x11ada : fija 0x1a53e=1.  Los registros finales (D0-D3/A0/A1) se dejan en `overlay_blit_regs` (los usan 0x12ce4/0x12d16). Todo el efecto de imagen es el descriptor + el screen. 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x111b6` | `overlay_compose_planes` | — | test_overlay |
| `0x111ac` | `overlay_compose_planes_wrap` | FUN_000111ac @ 0x111ac | test_overlay |
| `0x114ba` | `overlay_compose_planes32` | FUN_000114ba @ 0x114ba: variante de 32 bits de 0x111b6; el acarreo vive en D6 (entrada/salida) y el sub-word usa (3 - (D1&3)). Compone igual en 0xd8f0 y aplica con 0x123a0. FUN_000114b0 @ 0x114b0 fija D3-=1, D2=5, D1=(D1<<2)-1. | test_overlay |
| `0x114b0` | `overlay_compose_planes32_wrap` | — | test_overlay |
| `0x117fe` | `overlay_compose_masked` | FUN_000117fe @ 0x117fe: compone dos fuentes (A0 = mascara, A1 = dato) en 0xd8f0 con la mascara ror32(0x80000000, D0), lo aplica con 0x123a0 y repite. El flag 0x1a53e (lo fija el envoltorio) se resta a D2 cada pasada. FUN_000117de fija 0x1a53e=1 y FUN_000117e6 =0; ambas preparan D1/D2/D3 y A1 = A0-0x8000. | test_overlay |
| `0x117de` | `overlay_compose_masked_wrap` | — | test_overlay |
| `0x11afa` | `overlay_compose_shift` | — | test_overlay |
| `0x11ada` | `overlay_compose_shift_wrap` | — | test_overlay |

## Estado (globales A_* usadas)

- `A_DAT_1A534`
- `A_SAVE_DESC`

