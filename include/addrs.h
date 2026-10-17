/* addrs.h — named Ghidra addresses for KULT.PRG globals (load base 0x10000).
 *
 * Source of truth is work/names.txt; these mirror the `var` lines for the globals
 * the reconstruction touches. Add entries as functions are ported.
 */
#ifndef KULT_ADDRS_H
#define KULT_ADDRS_H

/* ---- screen buffers (crt0_init @ 0x135e2) ---- */
#define A_SCREEN_PTR_A 0x1a45e   /* front/logical buffer (physbase; copy_screen's DEST) */
#define A_SCREEN_PTR_B 0x1a462   /* back buffer (physbase - 0x8000; copy_screen's SOURCE) */

/* ---- input / cursor (wait_frame @ 0x13698) ---- */
#define A_INPUT_BUTTONS 0x1a456  /* bitmask; bit2 = click/fuego */
#define A_CURSOR_X      0x1a450  /* cursor x (pixels) */
#define A_CURSOR_Y      0x1a452  /* cursor y (pixels) */

/* ---- text bank cursor (FUN_00010f8e @ 0x10f8e) ---- */
#define A_CUR_STR_END   0x1a4dc  /* end of the current string entry */

/* ---- text rendering (print_char @ 0x10838) ---- */
#define A_CURSOR_TEXT_X 0x1a50a  /* text cursor x (columns) */
#define A_CURSOR_TEXT_Y 0x1a50c  /* text cursor y (rows) */
#define A_SCREEN_DRAW_BASE 0x1a46a /* logical-vs-draw buffer offset subtracted by print_char */
#define A_COLOR_TABLE   0x1a4d8  /* pointer to the font colour-expansion table */
#define A_FONT_DATA     0x1d202  /* embedded CARAC.BIN font (6 bytes/glyph) */
#define A_TEXT_END_FLAG 0x1a508  /* set to 0xff when print_word reaches cur_str_end */
#define A_TEXT_BOX_W    0x1a512  /* text-box width (columns) measured by 0x10998 */
#define A_TEXT_BOX_H    0x1a514  /* text-box height (rows) computed by the 0x9000 handler */
#define A_TEXT_BOX_SPAN 0x1a52e  /* box frame width in pixels (set by 0x10e66) */
#define A_TEXT_BOX_Y    0x1a530  /* box top-left row in text cells */
#define A_TEXT_BOX_X    0x1a532  /* box top-left column in text cells */

/* ---- overlay de texto (0x109fe / 0x10a88, modo alternativo del cursor) ---- */
#define A_MODE_FLAG     0x1acc6  /* == 0x87 desactiva el overlay de texto */
#define A_OVERLAY_X     0x1aa6a  /* posición X del overlay de texto */
#define A_OVERLAY_Y     0x1aa6c  /* posición Y del overlay de texto */
#define A_TEXT_ROW      0x1a513  /* fila/altura del overlay */
#define A_TEXT_SEL_A    0x1ab45  /* selector de entrada del overlay A (0x41b76) */
#define A_TEXT_SEL_B    0x1ab46  /* selector de entrada del overlay B (0x41586) */
#define A_TEXT_COL      0x1ab49  /* ancho del overlay */

/* ---- video / palette (set_palette @ 0x135da -> 0x16300) ---- */
#define A_PALETTE_PTR   0x1a466  /* pointer to the 16-word colour table (XBIOS Setpalette arg) */

/* ---- sprite blitter (blit_sprite @ 0x124f8) ---- */
#define A_BLIT_SCRATCH  0x55b02  /* 4-byte nibble-expansion scratch buffer (image-visible) */
#define A_SAVE_DESC     0xd8f0   /* screen save/restore descriptor (save @ 0x1258a, blit @ 0x124f2) */

/* ---- resource loader (load_resource @ 0x1076a) ---- */
#define A_CUR_FILENAME  0x1a55a  /* char* filename the loader opens */
#define A_LOAD_BUFFER   0x1a55e  /* destination buffer */
#define A_LOAD_LENGTH   0x1a562  /* byte count */
#define A_FILE_HANDLE   0x1a47e  /* last GEMDOS file handle (word) */
#define A_TEXT_INDEX    0x1a482  /* text index stored by position_text (0x10fc4) */

/* ---- cursor save/restore (FUN_0001060a @ 0x1060a, FUN_00010526 @ 0x10526) ---- */
#define A_CURSOR_SAVE_DEST 0x19ace  /* dest pointer for the 16x16 restore */
#define A_CURSOR_SAVE_BUF  0x199ce  /* 16 rows x 16 bytes saved buffer */
#define A_CURSOR_SPRITE_PTR 0x199ca /* pointer to the 16x16 cursor sprite */
#define A_CLIP_X 0x19700            /* screen origin x (cursor coords are relative) */
#define A_CLIP_Y 0x19702            /* screen origin y */
#define A_CURSOR_HOTSPOT 0x1a48a    /* nonzero enables the 4th word of the cursor */
#define A_IKBD_PACKET   0x1a44f     /* 5-byte IKBD packet copied by the handler */
#define A_SKIP_TRANSITION 0x1a570   /* 0xff en wait_frame cuando el paquete pide botón */
#define A_CLEAR_BUF     0x1a752     /* source buffer written to A:CLEAR.BIN */
#define A_CLEAR_NAME    0x1a439     /* "A:CLEAR.BIN" string */

/* ---- FUN_000172ba: limpiar flag indirecto ---- */
#define A_PTR_1A806     0x1a806     /* puntero a un byte que se pone a 0 */
#define A_W_1ACAA       0x1acaa     /* word que se pone a 0 */

/* ---- script-VM text entry (FUN_00014138 @ 0x14138) ---- */
#define A_TEXT_TABLE    0x41fb0     /* tabla de entradas de texto (imagen 0x31fb0) */
#define A_DAT_1A49E     0x1a49e     /* bit-LSB consumido por el efecto 'S' */
#define A_DAT_1A534     0x1a534     /* stride de fila del blit de texto */
#define A_DAT_1A4B2     0x1a4b2     /* fin de la lista de entradas de 3 bytes */
#define A_PAL_TABLE     0x1a10a     /* tabla de paletas (5 words por entrada) */
#define A_PAL_INDEX_CUR 0x1acf4     /* indice de paleta actual (byte) */

/* ---- masked row blitter (FUN_00012698 @ 0x12698, FUN_000123c0 @ 0x123c0) ---- */
#define A_BLIT_MASK_FIRST 0x1a538   /* mascara de la primera palabra (calculada por 0x12698) */
#define A_BLIT_MASK_LAST  0x1a53a   /* mascara de la ultima palabra */
#define A_BLIT_GROUPS     0x1a53c   /* numero de grupos de 4 palabras del blit */
#define A_BLIT_D8EA       0x0d8ea   /* byte de TOS comparado con 0x41 por FUN_0001215e */
#define A_DAT_1D402       0x1d402   /* marcador que FUN_0001215e limpia */

/* ---- interprete de animacion (FUN_000159d2 @ 0x159d2) ---- */
#define A_ANIM_TABLE    0x23ee8   /* tabla de entradas de animacion (imagen 0x13ee8) */
#define A_ANIM_END      0x1a4b6   /* puntero (long) al fin de la entrada actual */
#define A_ANIM_1A472    0x1a472   /* word: A6[2] (parametro d0 del script) */
#define A_ANIM_1A474    0x1a474   /* word: A6[3] (parametro d2 del script) */
#define A_ANIM_1A476    0x1a476   /* word: se pone a 0 */
#define A_ANIM_1A478    0x1a478   /* word: se pone a 0 */
#define A_ANIM_1A47A    0x1a47a   /* word: sign-ext de A6[1] del opcode 0xfe */
#define A_ANIM_1A47C    0x1a47c   /* word: sign-ext de A6[2] del opcode 0xfe */

/* ---- despacho de comandos (run_command_keep_sp @ 0x103ae, FUN_00010630 @ 0x10630) ---- */
#define A_THE_COMMAND   0x1a50e   /* word: comando actual */
#define A_SCRIPT_END    0x1a4e0   /* long: fin de la entrada de plantilla (templ_ptr) */
#define A_TEMPL_DATA    0x1d402   /* TEMPL.BIN (plantillas de script), imagen 0xd402 */
#define A_CALL_STACK    0x1a7d6   /* pila de llamadas de script (0x13522/0x13552) */
#define A_RUN_STACK     0x1a826   /* A7 guardado por run_command (0x103a8) */

/* ---- reveal pixelado (FUN_00012e2e @ 0x12e2e, FUN_00012ef6 @ 0x12ef6) ---- */
#define A_REV_X0     0x1a532   /* word: d0 (x) << 2 */
#define A_REV_Y0     0x1a530   /* word: d1 (y) */
#define A_REV_W      0x1a52e   /* word: d2 (ancho) << 2 */
#define A_REV_TOTAL  0x1a49c   /* word: d2 * d3 (total de pixels) */

/* ---- motor de sprites de animacion (handlers de 0x159d2, engine 0x15bde) ---- */
#define A_ANIM_SPR_PTR  0x1a4c6   /* puntero al sprite/descriptor activo (lo fija 0x15a90) */
#define A_ANIM_TBL_27FE2 0x27fe2  /* tabla de entradas (walk_entry) del handler 0x15abe */
#define A_ANIM_TBL_28AD2 0x28ad2  /* tabla de bitmaps indexada por 0x10f8e (handler 0x15abe) */
#define A_ANIM_1A574    0x1a574   /* byte de flags del elemento (bits 0..2 significativos) */
#define A_ANIM_1A46E    0x1a46e   /* word: contador de pasos del elemento */
#define A_ANIM_1A470    0x1a470   /* word: ancho (A6[1] & 0xfff8 >> 2) */
#define A_ANIM_1A577    0x1a577   /* byte: A6[1] & 0x7f >> 1 (ancho/2) */
#define A_ANIM_1A56A    0x1a56a   /* word: 0x1f4 (retardo) */
#define A_ANIM_1A578    0x1a578   /* byte: flag "primer frame" */
#define A_ANIM_1A4A0    0x1a4a0   /* word: filas del blit de 0x15da6 */
#define A_ANIM_BACKUP   0x3b1ac   /* buffer de respaldo de 0x15da6 */

/* ---- seleccion de objeto bajo el cursor (FUN_00018d28 @ 0x18d28) ---- */
#define A_SEL_OBJECT    0x1a81a   /* long: copiado a 0x1a7d2 */
#define A_SEL_ICON_PTR  0x1a81e   /* puntero a la celda que apunta al descriptor */
#define A_SEL_1A7D2     0x1a7d2   /* long: objeto seleccionado */
#define A_SEL_1AB4A     0x1ab4a   /* byte: copia de 0x1acc9 - 1 */
#define A_SEL_1ACC9     0x1acc9   /* byte origen del contador */

/* ---- lista de objetos y busqueda por clase (0x1778e/0x177bc, 0x166da/0x166ec) ---- */
#define A_OBJ_BEGIN     0x1a7ca   /* long: inicio de la lista de objetos (8 B/entrada) */
#define A_OBJ_END       0x1a7ce   /* long: fin de la lista de objetos */
#define A_OBJ_TBL       0x1aa72   /* tabla de clases (byte/entrada) */
#define A_ICON_TBL      0x1a776   /* tabla de descriptores de icono (long/entrada) */
#define A_1AB48         0x1ab48   /* byte: selector A por defecto */
#define A_CUR_CLASS     0x1ad0f   /* byte: clase del objeto bajo el cursor */
#define A_CLASS_IDX     0x1ace6   /* byte: indice de clase en 0x1aa72 */
#define A_HIT_MASK      0x1acd9   /* byte: mascara de hit-test */
#define A_HIT_VALUE     0x1acda   /* byte: valor de hit-test */
#define A_DAT_1ACE9     0x1ace9   /* byte: indice de la entrada del overlay (0x14966) */
#define A_DAT_1A75E     0x1a75e   /* long: puntero a la entrada 6 B de 0x1a8f0 (0x14966) */
#define A_TBL_1A6AE     0x1a6ae   /* tabla de rects/acciones de 10 B (0x14966) */
#define A_DAT_1A74E     0x1a74e   /* byte: mascara de hit-test guardada (0x167be/0x122ee) */
#define A_DAT_1A74F     0x1a74f   /* byte: valor de hit-test guardado (0x167be) */
#define A_DAT_1AD20     0x1ad20   /* byte: contador del umbral de la animacion (0x14f24) */
#define A_DAT_1ACEC     0x1acec   /* byte: fase/estado de la musica */
#define A_DAT_1ACFD     0x1acfd   /* byte: sub-estado */
#define A_DAT_1AD14     0x1ad14   /* byte: flag de transicion */
#define A_DAT_1AC06     0x1ac06   /* word: comando pendiente */

/* ---- bucle principal (game_loop @ 0x101be) ---- */
#define A_DAT_1ACF2     0x1acf2   /* byte: umbral de comandos por frame */
#define A_DAT_1ACE7     0x1ace7   /* byte: comandos ejecutados */
#define A_DAT_1ABF8     0x1abf8   /* word: comando pendiente de objeto */
#define A_DAT_1AD0B     0x1ad0b   /* byte: pausa del bucle */
#define A_DAT_1ACAC     0x1acac   /* word: temporizador A */
#define A_DAT_1ACAA     0x1acaa   /* word: comando temporizador A */
#define A_DAT_1ACB0     0x1acb0   /* word: temporizador B */
#define A_DAT_1ACAE     0x1acae   /* word: comando temporizador B */
#define A_DAT_1ACD3     0x1acd3   /* byte: flag de accion de objeto */
#define A_DAT_1ACE8     0x1ace8   /* byte: contador de accion de objeto */
#define A_DAT_1AD0D     0x1ad0d   /* byte: flag de carga/guardado de sala en curso */
#define A_DAT_1A750     0x1a750   /* byte: estado de la carga/guardado de sala */
#define A_DAT_1AD1D     0x1ad1d   /* byte: flag de animacion de script (0x188ce) */
#define A_DAT_1AAF9     0x1aaf9   /* byte: comparado con $1acd2 en 0x188ce */
#define A_DAT_1AB08     0x1ab08   /* byte: habilita la segunda animacion de 0x188ce */
#define A_DAT_1AD00     0x1ad00   /* byte: indice rotatorio de 0x17480 (+5, tope 0x18) */
#define A_DAT_1A572     0x1a572   /* byte: flag de formulario/transicion de 0x1460a */
#define A_DAT_1AD0F     0x1ad0f   /* byte: flags de entrada de 0x1460a (bit3 = formulario) */
#define A_DAT_1ACC8     0x1acc8   /* byte: indice de objeto guardado por 0x1460a */
#define A_DAT_1ACFE     0x1acfe   /* byte: copia de $1acd2 guardada por 0x1460a */
#define A_DAT_1AD25     0x1ad25   /* byte: indice de tabla de texto pendiente (0x1460a) */
#define A_DAT_1ACD8     0x1acd8   /* byte: flag puesto por 0x1682a tras cargar CLEAR.BIN */
#define A_DAT_1AD2C     0x1ad2c   /* byte: contador de entradas de sample (0x18538) */
#define A_DAT_1A4D2     0x1a4d2   /* word: flag de fin del juego */

/* ---- script_step (0x17e3c): punteros de guion y estado de overlay ---- */
#define A_SCRIPT_PTR    0x1a822   /* long: puntero al byte actual del guion */
#define A_OVL_ENTRY     0x1aae0   /* base de la entrada de overlay [0..3] */
#define A_1AAB3         0x1aab3   /* byte: caracter actual */
#define A_1AAB4         0x1aab4   /* byte: flags de entrada */
#define A_1AAB6         0x1aab6   /* byte: atributo */
#define A_1AAE1         0x1aae1   /* byte: flags (bit5/bit6) */
#define A_1AAD6         0x1aad6   /* byte: eco del caracter */
#define A_1AADB         0x1aadb   /* byte: eco del caracter */
#define A_DAT_1AD05     0x1ad05   /* byte: comando actual del guion */
#define A_DAT_1AD06     0x1ad06   /* byte: siguiente comando */
#define A_DAT_1AD07     0x1ad07   /* byte: flag de dos bytes */
#define A_DAT_1ACF6     0x1acf6   /* byte: flags de modo */
#define A_DAT_1ACF7     0x1acf7   /* byte: flag de fin de secuencia */
#define A_DAT_1ACB2     0x1acb2   /* word: proximo umbral del guion */

/* ---- comando de elemento / icono (FUN_000128a4, 0x1285a, 0x1296c, 0x129c8) ---- */
#define A_ELEM_CLASS    0x1a772   /* puntero al descriptor; [3]&0xf8 es la clase buscada */
#define A_ELEM_TBL_19E19 0x19e19  /* tabla de 6 bytes por clase (0x19e19..0x19ea9) */
#define A_ELEM_TBL_19EA9 0x19ea9  /* fin de la tabla anterior */
#define A_ELEM_1AB4C    0x1ab4c   /* byte copiado de la entrada encontrada */
#define A_ELEM_1AB4D    0x1ab4d   /* byte copiado de la entrada encontrada */
#define A_ELEM_TBL_1A252 0x1a252  /* tabla de patrones de 10 bytes (0x1296c) */
#define A_ELEM_BUILD    0x3b1ae   /* bitmap construido por 0x1296c */
#define A_ELEM_1A528    0x1a528   /* puntero guardado por 0x128a4 */
#define A_ELEM_1A52C    0x1a52c   /* byte (alto) leido de 0x3b1ae */
#define A_ELEM_1A52E    0x1a52e   /* byte (ancho) leido de 0x3b1af */
#define A_ELEM_1AB4E    0x1ab4e   /* byte: clase del elemento */
#define A_ELEM_1A532    0x1a532   /* word: x del elemento */
#define A_ELEM_1A530    0x1a530   /* word: y del elemento */
#define A_ELEM_TBL_2751E 0x2751e  /* lista de entradas de elemento recorrida por 0x129c8 */
#define A_ELEM_TBL_31072 0x31072  /* tabla de texto referida por 0x129c8 */
#define A_DAT_1AD0A       0x1ad0a  /* copia de 0x1a851 usada por 0x128a4/0x120a8 */
#define A_ELEM_TBL_23E9C  0x23e9c  /* lista de elementos de 0x12a4a (walk_entry) */
#define A_ELEM_BUILD2     0x3b1ac  /* bitmap construido/leido por 0x12a4a */
#define A_DAT_1A484       0x1a484  /* word limpiada por 0x121fa */

/* ---- save enmascarado (FUN_000127fa) ---- */
#define A_SAVE_1A4A2     0x1a4a2   /* word: mascara de la ultima palabra */
#define A_SAVE_1A4A4     0x1a4a4   /* word: filas del save */

/* ---- handlers de revelado por caja (FUN_00012ea2/12e08) ---- */
#define A_DAT_1AB40      0x1ab40   /* byte: x0 de la caja */
#define A_DAT_1AB41      0x1ab41   /* byte: x1 de la caja */
#define A_DAT_1AB42      0x1ab42   /* byte: y0 de la caja */
#define A_DAT_1AB43      0x1ab43   /* byte: y1 de la caja */

/* ---- input por frame / seleccion por cursor (0x1031c, 0x1478a/14798/147ce) ---- */
#define A_FRAME_1AB4F    0x1ab4f   /* byte: si != 0 espera con el cursor bajo */
#define A_1AB44          0x1ab44   /* byte: selector A pintado (0x1ab45 eco) */
#define A_1AB47          0x1ab47   /* byte: selector B pintado (0x1ab46 eco) */
#define A_1AB48A         0x1a48a   /* byte: flag de hotspot del cursor */

/* ---- fila/paleta helpers (FUN_000159bc, FUN_00016210/230/27a) ---- */
#define A_DAT_1A579       0x1a579   /* indice incremental de la tabla 0x2741e */
#define A_DAT_1ACD7       0x1acd7   /* byte leido de la tabla 0x2741e por 0x16210 */
#define A_TBL_2741E       0x2741e   /* tabla de bytes indexada por 0x1a579 */
#define A_DAT_1AB60       0x1ab60   /* contador comparado en FUN_0001627a */
#define A_DAT_1A526       0x1a526   /* marca del ultimo umbral de FUN_0001627a */
#define A_LIST_1A8F0      0x1a8f0   /* lista de entradas de 6 bytes (0x1627a, 0x178cc) */
#define A_DAT_1ACD2       0x1acd2   /* byte comparado con el campo 1 de cada entrada */
#define A_DAT_1ACD4       0x1acd4   /* contador de VBL (lo incrementa el handler de VBL) */

/* ---- modo de escena / lista de objetos (0x1442c/0x1443e/0x14496/0x14588) ---- */
#define A_DAT_1ACC7       0x1acc7   /* byte: valor de modo que 0x1442c salva y restaura */
#define A_DAT_1ACCB       0x1accb   /* byte: copia del modo anterior (0x1443e) */
#define A_DAT_1AD32       0x1ad32   /* byte: si != 0 fuerza el marcador 0xe5 en 0x1a695 */
#define A_DAT_1A695       0x1a695   /* buffer de 8 bytes del marcador de modo (0x1443e) */
#define A_MODE_TABLE      0x1ad38   /* tabla de entradas [len][datos] indexada por 0x1acc6 (1-based) */
#define A_DAT_1AB4B       0x1ab4b   /* byte: nº de objetos empaquetados de la entrada de modo */
#define A_DAT_1AB62       0x1ab62   /* tabla de 5 words por objeto (0x14496) */
#define A_DAT_1A762       0x1a762   /* long: copia de object_list_begin */
#define A_DAT_1ACEB       0x1aceb   /* byte: contador de objetos (lo limpia 0x14496) */
#define A_DAT_1ABFA       0x1abfa   /* word: comando de 0x163a8 (lo limpia 0x14588) */
#define A_DAT_1AB5C       0x1ab5c   /* word: comando 0xa01c (0x14588) */
#define A_DAT_1AB5E       0x1ab5e   /* word: 0 (0x14588) */
#define A_DAT_1AB56       0x1ab56   /* word: 0xf7 (0x14588) */
#define A_DAT_1AB50       0x1ab50   /* word: 0x9048 (0x14588) */
#define A_DAT_1AB52       0x1ab52   /* word: 0xa01d (0x14588) */
#define A_DAT_1AB54       0x1ab54   /* word: 0 (0x14588) */
#define A_DAT_1AB58       0x1ab58   /* word: 0x9048 (0x14588) */
#define A_DAT_1AB5A       0x1ab5a   /* word: 0x9048 (0x14588) */
#define A_DAT_1AD09       0x1ad09   /* byte: lo limpia 0x14588 */
#define A_DAT_1AD11       0x1ad11   /* byte: lo limpia 0x14588 */
#define A_DAT_1AD27       0x1ad27   /* byte: lo limpia 0x14588 */
#define A_DAT_1ACD1       0x1acd1   /* byte: lo limpia 0x14588 */
#define A_DAT_1ACF9       0x1acf9   /* byte: lo limpia 0x14588 */
#define A_DAT_1ACFA       0x1acfa   /* byte: lo limpia 0x14588 */
#define A_DAT_1ACCE       0x1acce   /* byte: lo limpia 0x14588 */
#define A_DAT_1ACFF       0x1acff   /* byte: lo limpia 0x14588 */
#define A_DAT_1ACFC       0x1acfc   /* byte: flags de objeto (bit7) y fin de secuencia */
#define A_DAT_1ACF1       0x1acf1   /* byte: indice de color de la tabla de texto (0x13f2a) */
#define A_DAT_D8EB        0x0d8eb   /* byte: 0x52 permite a 0x14496 salir del reintento */

#endif
