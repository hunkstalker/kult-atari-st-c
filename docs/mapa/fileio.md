# fileio.c

fileio.c — the resource loader.  load_resource @ 0x1076a: Fopen(cur_filename, 2) -> handle (stored at 0x1a47e); if >= 0, Fread(handle, load_length, load_buffer) and Fclose(handle). The GEMDOS calls go through the kit's os_fopen/os_fread/os_fclose over the staged-file table, which is the same state the oracle's traps are served from. 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x1076a` | `load_resource` | — | test_fileio |
| `0x107a2` | `write_named_file` | write_named_file @ 0x107a2: Fcreate(cur_filename, 0) -> handle; if >= 0, Fwrite(handle, load_length, load_buffer) and Fclose(handle). | test_fileio |
| `0x1077e` | `save_clear_bin` | save_clear_bin @ 0x1077e: stage the clear buffer and write it to A:CLEAR.BIN. | test_fileio |
| `0x106f8` | `load_resource_list` | load_resource_list @ 0x106f8: walk a table of 14-byte entries (10-byte name + 4-byte buffer ptr) terminated by '$'. For each: set cur_filename, Fsfirst to learn the file's size (into the DTA), set load_buffer/load_length, then load it (Fopen/Fread/Fclose). Returns 0, or -1 on the first missing file. The original's tail re-checks a fixed opcode in the image; its only observable is D0=0, which the return states. | test_fileio |
| `0x106e2` | `load_resource_list_a` | FUN_000106e2 @ 0x106e2 and FUN_000106ea @ 0x106ea seed load_resource_list's A5 with a fixed in-image table and fall into it. | test_fileio |
| `0x106ea` | `load_resource_list_b` | — | test_fileio |
| `0x10682` | `load_eche_bank` | FUN_00010682 @ 0x10682: si $1a57c != 0xff y el digito (d0 + '0') cambia, lo guarda en la tabla 0x1a404 (char en +4, d1 en +0xa) y repite load_resource_list(0x1a404) lanzando el comando 0x918e mientras falte el fichero; restaura $1a4e0 al salir. | test_loader |
| `0x1066e` | `load_eche_bank_wrap` | FUN_0001066e @ 0x1066e: llama a 0x10682 con d1 = 0x64c8e. | test_loader |
| `0x10668` | `load_eche_bank_cmd` | FUN_00010668 @ 0x10668: d0 = a6[1], cae en 0x1066e; devuelve a6 + 2. | test_loader |
| `0x107b6` | `gemdos_fopen` | gemdos_fopen @ 0x107b6: Fopen(cur_filename, 2), guarda el handle en $1a47e. | test_fileio |
| `0x107d0` | `gemdos_fcreate` | gemdos_fcreate @ 0x107d0: Fcreate(cur_filename, 0), guarda el handle en $1a47e. | test_fileio |
| `0x107e8` | `gemdos_fread` | gemdos_fread @ 0x107e8: Fread(handle, load_length, load_buffer). | test_fileio |
| `0x10808` | `gemdos_fwrite` | gemdos_fwrite @ 0x10808: Fwrite(handle, load_length, load_buffer). | test_fileio |
| `0x10828` | `gemdos_fclose` | gemdos_fclose @ 0x10828: Fclose(handle). | test_fileio |
| `0x16ed8` | `load_scena` | — | test_fileio |
| `0x16fb4` | `save_scena` | — | test_fileio |
| `0x1682a` | `reset_clear` | — | test_loop |

## Estado (globales A_* usadas)

- `A_CLEAR_BUF`
- `A_CLEAR_NAME`
- `A_CUR_FILENAME`
- `A_DAT_1A750`
- `A_DAT_1ACD8`
- `A_DAT_1AD0D`
- `A_DAT_1AD0F`
- `A_FILE_HANDLE`
- `A_LOAD_BUFFER`
- `A_LOAD_LENGTH`
- `A_SCREEN_PTR_A`
- `A_SCREEN_PTR_B`
- `A_TEMPL_PTR`
- `A_THE_COMMAND`

