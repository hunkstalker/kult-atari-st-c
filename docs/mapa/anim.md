# anim.c

anim.c — FUN_000159d2 @ 0x159d2: intérprete de animación.  Lee una entrada de la tabla A_ANIM_TABLE (0x23ee8, formato [u8 len][data]) elegida por A6[1] (1-based) con `walk_entry` (0x12950), y recorre sus bytes hasta el fin de la entrada (`$1a4b6`):  0xfe  -> $1a47a = sign_ext8(A6[1]); $1a47c = sign_ext8(A6[2]); A6 += 3 0xfd  -> wrappers de sonido FUN_00018f8e/0x98; no-op con el bypass `$1ab4f != 0` 0xfc  -> XBIOS Supexec (FUN_00018fa4); no-op con el bypass; A6 += 1 otro  -> handler del motor de sprites 0x15bde, índice = byte & 7  Los 8 handlers (`0x15a90`..) caen al motor `0x15bde` y quedan PENDIENTES; los tests de este primer corte usan solo 0xfe/0xfd/0xfc (ver src/anim.c en el TODO). 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x15a90` | `anim_bank_select` | FUN_00015a90 @ 0x15a90: elige el banco de sprites activo. A0 = 0xd8f0 + (bit7 ? 0xc80 : 0) + (bit6 ? 0x640 : 0) y lo guarda en $1a4c6; devuelve A0. | test_anim |
| `0x15d7e` | `anim_restore` | FUN_00015d7e @ 0x15d7e: restaura la zona bajo el elemento. Carga $1a476/$1a478 en D2/D3 y $1a472/$1a474 en D0/D1, calcula la dirección (screen_address), corre el blit enmascarado (FUN_0001215e) y limpia $1a478. | test_anim |
| `0x15cd8` | `anim_counter` | FUN_00015cd8 @ 0x15cd8: ajusta avance/recorte de un elemento. Lee los flags $1a574 (0..7) y el contador $1a46e; según ellos avanza/decrementa el contador, ajusta D0/D2 y el puntero fuente A1, y puede anular el contador. Devuelve los registros por punteros (no hay efecto en imagen salvo $1a46e). | test_anim |
| `0x15da6` | `anim_engine_entry` | FUN_00015da6 @ 0x15da6: dibuja/actualiza un elemento. Respalda la zona del buffer trasero en A_ANIM_BACKUP, blitea el sprite (motor de text_blit_fwd), aplica un retardo calibrado y una espera de VBL (FUN_00013594), dibuja con FUN_00012e2e (reveal) o restaura con FUN_0001215e según $1a578, y finalmente devuelve el respaldo al buffer trasero y actualiza $1a472/4/6/8. | test_anim |
| `0x12b40` | `bit_permute_byte` | FUN_00012b40 @ 0x12b40: permutación de bits de un byte (D1 -> D6), usada por el blitter 0x12b04 (orientación invertida). | test_anim |
| `0x12ad6` | `elem_blit` | FUN_00012ad6 @ 0x12ad6: blitter de bytes directo (D2+1 columnas x D3+1 filas). | test_anim |
| `0x12b04` | `elem_blit_rev` | FUN_00012b04 @ 0x12b04: variante invertida (permuta el byte con 0x12b40 y escribe hacia atrás desde A0+2*D2). | test_anim |
| `0x15abe` | `anim_bank_build` | FUN_00015abe @ 0x15abe (handler idx 0): limpia el banco de sprites, elige la entrada `idx` de la tabla 0x27fe2 (walk_entry), copia sus 2 primeros bytes como cabecera del banco y, por cada subentrada de 3 bytes [idx][x_hi|x_flag][x_lo], toma el bitmap idx de la tabla 0x28ad2 y lo blitea (0x12ad6/0x12b04 según el bit7 del byte alto del offset). | test_anim |
| `0x15b94` | `anim_h7` | — | — |
| `0x15bcc` | `anim_h1` | — | — |
| `0x15ba6` | `anim_h6` | — | — |
| `0x15bb2` | `anim_h2` | — | — |
| `0x15bbc` | `anim_h4` | — | — |
| `0x159d2` | `anim_entry_a` | FUN_000159d2 @ 0x159d2: prólogo que lee D1/D0/D2 del script y cae en el cuerpo. | test_anim |
| `0x159e2` | `anim_entry_b` | FUN_000159e2 @ 0x159e2: entrada gemela con D0/D1/D2 ya en registros; conserva A6. | test_anim |
| `0x14f24` | `anim_threshold` | FUN_00014f24 @ 0x14f24: gestiona el umbral 0x1ad20. Si D0 <= 0x1ad20 lo resta y, cuando cambia el nibble alto (o 0x1ad20 < D0), dispara la animacion 0x159e2 con D0/D1/D2 = 0x4a/0x44/0x47. La rama de disparo hace `addq.l #4,a7` en el original (se salta el resto de su llamante), asi que su test usa stop_pc=0x14f5c. | test_anim |
| `0x15f6c` | `script_arg` | — | test_anim |
| `0x15fd4` | `script_alu` | FUN_00015fd4 @ 0x15fd4: ALU sobre D6/D0 según los bits de D7. Con bit6 = 0 es aritmética (add/sub/and/or/eor); con bit6 = 1 es una comparación -> D6 = 0xffff si alguna condición se cumple, si no 0. Devuelve D6. | test_anim |
| `0x1606e` | `script_expr` | FUN_0001606e @ 0x1606e: evalúa una expresión: D6 = primer argumento (0x15f6c) y luego, mientras el byte operador (a6)+ no tenga el bit7, aplica 0x15fd4 con el siguiente argumento. Devuelve D6 y actualiza a6/a0. | test_anim |
| `0x16088` | `script_store` | FUN_00016088 @ 0x16088: lee una dirección (0x15f6c) y el valor (0x1606e) y guarda: word si el bit31 de la dirección está puesto, byte si no. | test_anim |
| `0x160aa` | `script_jump_zero` | — | test_anim |

## Estado (globales A_* usadas)

- `A_ANIM_1A46E`
- `A_ANIM_1A470`
- `A_ANIM_1A472`
- `A_ANIM_1A474`
- `A_ANIM_1A476`
- `A_ANIM_1A478`
- `A_ANIM_1A47A`
- `A_ANIM_1A47C`
- `A_ANIM_1A4A0`
- `A_ANIM_1A574`
- `A_ANIM_1A578`
- `A_ANIM_BACKUP`
- `A_ANIM_SPR_PTR`
- `A_ANIM_TBL_27FE2`
- `A_ANIM_TBL_28AD2`
- `A_DAT_1A49E`
- `A_DAT_1A4B2`
- `A_DAT_1AD20`

