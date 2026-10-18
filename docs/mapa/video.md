# video.c

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x16210` | `pal_next_byte` | FUN_00016210 @ 0x16210: bump the byte at 0x1a579, index the byte table at 0x2741e by the new value, and store that byte into 0x1acd7. Devuelve el byte en D0 (lo usan 0x17a1a y compania); los llamantes que no lo necesitan lo ignoran. | — |
| `0x16230` | `pal_next_word` | FUN_00016230 @ 0x16230: same bump, but read a big-endian word from 0x2741e[index] and return it in D0. | — |
| `0x1627a` | `pal_threshold` | FUN_0001627a @ 0x1627a: only when the low byte of (0x1ab60 - 0x1a526) reaches 0xb4, latch 0x1a526 = 0x1ab60 and clear bit 6 of each of the 63 six-byte entries at 0x1a8f0 whose byte0 has bit 6 set, byte1 != 0x1acd2 and byte1 < 0x34. | — |
| `0x16310` | `palette_cycle` | (cmd idx 9): umbral 2, ciclo de paleta 0x16310 y flag $1ad0f. | test_loop |
| `0x135e2` | `crt0_init` | FUN_000135e2 @ 0x135e2 (crt0_init): arranque. XBIOS Getrez -> $1a558 (low-res), Physbase -> $1a45e/$1a462(-0x8000)/$1a466(-0x20), GEMDOS Super(0) -> guarda el vector $456 en $1a4d4 y lo apunta a 0x160c2, $502 = 0x1019a, $1a579 = $4bd, Super(restore), XBIOS Kbdvbase -> $1a446 (handler mouse a 0x13686 guardando el anterior en $1a44a) e Ikbdws(0x19) (sin efecto en imagen). | — |

## Estado (globales A_* usadas)

- `A_DAT_1A526`
- `A_DAT_1A579`
- `A_DAT_1AB60`
- `A_DAT_1ABFA`
- `A_DAT_1ACD2`
- `A_DAT_1ACD7`
- `A_LIST_1A8F0`
- `A_PALETTE_PTR`
- `A_PAL_INDEX_CUR`
- `A_PAL_TABLE`
- `A_TBL_2741E`
- `A_THE_COMMAND`

