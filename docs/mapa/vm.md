# vm.c

vm.c — core script-VM helpers.  FUN_00012950 @ 0x12950: skip D1 byte-length entries from A4 ([u8 len][data], len includes itself), then A5 = entry+1 and A4 = entry + *entry. Reads only; the two outputs are the updated A4 and A5.  FUN_00013f56 @ 0x13f56: set 0x1a49e = D0, walk the ARPLA text table at 0x24982 to the D1th entry, then run FUN_00014138 over its 3-byte entries up to the end marker. FUN_00013f4c @ 0x13f4c is the entry that seeds D0 = 0 and D1 from A6[1]. 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x12950` | `walk_entry` | walk entry | test_anim, test_dispatch, test_vm |
| `0x13f56` | `run_text_table` | fija 0x1a49e = D0, camina la tabla ARPLA (0x24982) a la entrada D1 y corre 0x14138 sobre sus bloques de 3 bytes hasta el fin (A_DAT_1A4B2). El A6 se salva/restaura: no lo toca. (La copia de filas/0x12f2a es 0x13f2a, otra funcion.) | test_vm |
| `0x13f4c` | `text_table_entry` | D0=0, D1=A6[1] -> FUN_00013f56 | test_vm |

## Estado (globales A_* usadas)

- `A_DAT_1A49E`
- `A_DAT_1A4B2`

