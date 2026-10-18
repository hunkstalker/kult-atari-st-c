/* fileio.c — the resource loader.
 *
 * load_resource @ 0x1076a: Fopen(cur_filename, 2) -> handle (stored at 0x1a47e); if >= 0,
 * Fread(handle, load_length, load_buffer) and Fclose(handle). The GEMDOS calls go through the
 * kit's os_fopen/os_fread/os_fclose over the staged-file table, which is the same state the
 * oracle's traps are served from.
 */
#include <stdint.h>

#include "machine.h"
#include "os.h"
#include "addrs.h"

/* 0x1076a — load_fond @ 0x13568: fija el nombre 0x1a41e y carga una pantalla de 0x7d22 bytes en screen_ptr_b-0x22 (cae en load_named_screen). FUN_00013572 @ 0x13572: sol... */
void load_resource(uint8_t *img) {
    int32_t h = os_fopen(img, be32(img + A_CUR_FILENAME));
    wr16(img + A_FILE_HANDLE, (uint16_t)h);
    if (h >= 0) {
        os_fread(img, (uint16_t)h, be32(img + A_LOAD_LENGTH), be32(img + A_LOAD_BUFFER));
        os_fclose(img, (uint16_t)h);
    }
}

/* KULT_DTA: the game's GEMDOS DTA, image 0x45ad6 relocated by the 0x10000 load base. */
#define KULT_DTA  0x55ad6u
#define KULT_DTA_SIZE (KULT_DTA + 0x1a)

/* write_named_file @ 0x107a2: Fcreate(cur_filename, 0) -> handle; if >= 0,
 * Fwrite(handle, load_length, load_buffer) and Fclose(handle). */
void write_named_file(uint8_t *img) {
    int32_t h = os_fcreate(img, be32(img + A_CUR_FILENAME));
    wr16(img + A_FILE_HANDLE, (uint16_t)h);
    if (h < 0)
        return;
    os_fwrite(img, (uint16_t)h, be32(img + A_LOAD_LENGTH), be32(img + A_LOAD_BUFFER));
    os_fclose(img, (uint16_t)h);
}

/* save_clear_bin @ 0x1077e: stage the clear buffer and write it to A:CLEAR.BIN. */
void save_clear_bin(uint8_t *img) {
    wr32(img + A_LOAD_BUFFER, A_CLEAR_BUF);
    wr32(img + A_LOAD_LENGTH, 0x291c);
    wr32(img + A_CUR_FILENAME, A_CLEAR_NAME);
    write_named_file(img);
}

/* load_resource_list @ 0x106f8: walk a table of 14-byte entries (10-byte name + 4-byte buffer
 * ptr) terminated by '$'. For each: set cur_filename, Fsfirst to learn the file's size (into the
 * DTA), set load_buffer/load_length, then load it (Fopen/Fread/Fclose). Returns 0, or -1 on the
 * first missing file. The original's tail re-checks a fixed opcode in the image; its only
 * observable is D0=0, which the return states. */
int32_t load_resource_list(uint8_t *img, uint32_t table) {
    os_fsetdta(KULT_DTA);
    while (img[table] != 0x24) {                    /* '$' terminates the table */
        wr32(img + A_CUR_FILENAME, table);
        if (os_fsfirst(img, table) < 0)
            return -1;
        wr32(img + A_LOAD_BUFFER, be32(img + table + 10));
        wr32(img + A_LOAD_LENGTH, be32(img + KULT_DTA_SIZE));
        int32_t h = os_fopen(img, be32(img + A_CUR_FILENAME));
        wr16(img + A_FILE_HANDLE, (uint16_t)h);
        if (h < 0)
            return -1;
        os_fread(img, (uint16_t)h, be32(img + A_LOAD_LENGTH), be32(img + A_LOAD_BUFFER));
        os_fclose(img, (uint16_t)h);
        table += 0xe;
    }
    return 0;
}

/* FUN_000106e2 @ 0x106e2 and FUN_000106ea @ 0x106ea seed load_resource_list's A5 with a fixed
 * in-image table and fall into it. */
int32_t load_resource_list_a(uint8_t *img) {
    return load_resource_list(img, 0x1a3aa);
}

/* 0x106ea — A5=0x1a3e4 -> load_resource_list */
int32_t load_resource_list_b(uint8_t *img) {
    return load_resource_list(img, 0x1a3e4);
}

void run_command_keep_sp(uint8_t *, uint32_t);

/* FUN_00010682 @ 0x10682: si $1a57c != 0xff y el digito (d0 + '0') cambia, lo guarda
 * en la tabla 0x1a404 (char en +4, d1 en +0xa) y repite load_resource_list(0x1a404)
 * lanzando el comando 0x918e mientras falte el fichero; restaura $1a4e0 al salir. */
void load_eche_bank(uint8_t *img, uint32_t digit, uint32_t val) {
    uint32_t saved = be32(img + A_TEMPL_PTR);
    uint8_t c = (uint8_t)((digit & 0xff) + 0x30);
    if (img[0x1a57c] != 0xff && c != img[0x1a57c]) {
        img[0x1a408] = c;
        wr32(img + 0x1a40e, val);
        img[0x1a57c] = c;
        for (;;) {
            if (load_resource_list(img, 0x1a404) >= 0)
                break;
            wr16(img + A_THE_COMMAND, 0x918e);
            run_command_keep_sp(img, 0);
        }
    }
    wr32(img + A_TEMPL_PTR, saved);
}

/* FUN_0001066e @ 0x1066e: llama a 0x10682 con d1 = 0x64c8e. */
void load_eche_bank_wrap(uint8_t *img, uint32_t digit) {
    load_eche_bank(img, digit, 0x64c8e);
}

/* FUN_00010668 @ 0x10668: d0 = a6[1], cae en 0x1066e; devuelve a6 + 2. */
uint32_t load_eche_bank_cmd(uint8_t *img, uint32_t script) {
    load_eche_bank_wrap(img, img[script + 1]);
    return script + 2;
}

/* Las primitivas GEMDOS del loader (0x107b6..0x10828), por si se invocan sueltas.
 * Cada una es un trap envuelto: os_fopen(name) -> handle en $1a47e, etc. Devuelven
 * el valor del trap en D0, que los llamantes (0x16ed8/0x16fb4) comprueban. */

/* gemdos_fopen @ 0x107b6: Fopen(cur_filename, 2), guarda el handle en $1a47e. */
int32_t gemdos_fopen(uint8_t *img) {
    int32_t h = os_fopen(img, be32(img + A_CUR_FILENAME));
    wr16(img + A_FILE_HANDLE, (uint16_t)h);
    return h;
}

/* gemdos_fcreate @ 0x107d0: Fcreate(cur_filename, 0), guarda el handle en $1a47e. */
int32_t gemdos_fcreate(uint8_t *img) {
    int32_t h = os_fcreate(img, be32(img + A_CUR_FILENAME));
    wr16(img + A_FILE_HANDLE, (uint16_t)h);
    return h;
}

/* gemdos_fread @ 0x107e8: Fread(handle, load_length, load_buffer). */
int32_t gemdos_fread(uint8_t *img) {
    return os_fread(img, be16(img + A_FILE_HANDLE), be32(img + A_LOAD_LENGTH),
                    be32(img + A_LOAD_BUFFER));
}

/* gemdos_fwrite @ 0x10808: Fwrite(handle, load_length, load_buffer). */
int32_t gemdos_fwrite(uint8_t *img) {
    return os_fwrite(img, be16(img + A_FILE_HANDLE), be32(img + A_LOAD_LENGTH),
                     be32(img + A_LOAD_BUFFER));
}

/* gemdos_fclose @ 0x10828: Fclose(handle). */
int32_t gemdos_fclose(uint8_t *img) {
    return os_fclose(img, be16(img + A_FILE_HANDLE));
}

/* ---- carga/guardado de la sala (0x16ed8 / 0x16fb4) ----
 *
 * Lectura: SCENA.BIN -> 0x1a752 (0x291c B), screen_ptr_b (0x7d00), copy_screen,
 * paleta y otra lectura de 0x7d00 en screen_ptr_b; despues 0x16f90 (hints +0x101a8)
 * y 0x16460 (tabla de iconos). Guardado: al reves, restando 0x101a8 a los punteros
 * de hint y escribiendo los dos buffers. Los dos devuelven por CCR: el original
 * hace `ori.b #$1,ccr` en el error, asi que el C devuelve 1/0. */
void copy_screen(uint8_t *);
void set_palette_current(uint8_t *);
void hint_table_relocate(uint8_t *);
void icon_table(uint8_t *);
void draw_hint_icons(uint8_t *);

/* 0x16ed8 — carga SCENA.BIN (hints + 2 x 32000 B) */
int32_t load_scena(uint8_t *img) {
    img[A_DAT_1AD0D] = 1;
    wr32(img + A_CUR_FILENAME, 0x1a414);           /* "SCENA.BIN" */
    if (gemdos_fopen(img) < 0)
        goto error;
    wr32(img + A_LOAD_LENGTH, 0x291c);
    wr32(img + A_LOAD_BUFFER, A_CLEAR_BUF);
    if (gemdos_fread(img) != 0x291c)
        goto error;
    wr32(img + A_LOAD_BUFFER, be32(img + A_SCREEN_PTR_B));
    wr32(img + A_LOAD_LENGTH, 0x7d00);
    if (gemdos_fread(img) != 0x7d00)
        goto error;
    copy_screen(img);
    set_palette_current(img);
    if (gemdos_fread(img) != 0x7d00)
        goto error;
    if (gemdos_fclose(img) < 0)
        goto error;
    hint_table_relocate(img);
    icon_table(img);
    img[A_DAT_1AD0D] = 0;
    img[A_DAT_1A750] = 0;
    return 0;
error:
    img[A_DAT_1AD0D] = 0;
    img[A_DAT_1A750] = 0xff;
    return 1;
}

/* 0x16fb4 — guarda SCENA.BIN (inverso de 0x16ed8) */
int32_t save_scena(uint8_t *img) {
    img[A_DAT_1AD0D] = 1;
    draw_hint_icons(img);                              /* draw_hint_icons */
    for (uint32_t p = 0x1a752; p != 0x1a82e; p += 4) {
        if (be32(img + p) != 0)
            wr32(img + p, (uint32_t)(be32(img + p) - 0x101a8));
    }
    wr32(img + A_CUR_FILENAME, 0x1a414);           /* "SCENA.BIN" */
    if (gemdos_fcreate(img) < 0)
        goto error;
    wr32(img + A_LOAD_LENGTH, 0x291c);
    wr32(img + A_LOAD_BUFFER, A_CLEAR_BUF);
    if (gemdos_fwrite(img) != 0x291c)
        goto error;
    wr32(img + A_LOAD_BUFFER, be32(img + A_SCREEN_PTR_A));
    wr32(img + A_LOAD_LENGTH, 0x7d00);
    if (gemdos_fwrite(img) != 0x7d00)
        goto error;
    wr32(img + A_LOAD_BUFFER, be32(img + A_SCREEN_PTR_B));
    if (gemdos_fwrite(img) != 0x7d00)
        goto error;
    if (gemdos_fclose(img) < 0)
        goto error;
    hint_table_relocate(img);
    img[A_DAT_1AD0D] = 0;
    img[A_DAT_1A750] = 0;
    return 0;
error:
    hint_table_relocate(img);
    img[A_DAT_1AD0D] = 0;
    img[A_DAT_1A750] = 0xff;
    return 1;
}

/* FUN_0001682a @ 0x1682a: repite la carga de A:CLEAR.BIN (0x291c B en 0x1a752) hasta
 * que exista (si falla lanza 0x918e); al exito reinicia la pila (A7 = 0x1d1fe), marca
 * $1ad0f, avanza la tabla 0x2741e y salta a 0x10158 (el test lo corta ahi). */
void run_command_keep_sp(uint8_t *, uint32_t);
uint32_t pal_next_byte(uint8_t *);

/* 0x1682a — ctx: bucle A:CLEAR.BIN + the_command=0x918e */
void reset_clear(uint8_t *img) {
    wr32(img + A_LOAD_BUFFER, A_CLEAR_BUF);
    wr32(img + A_LOAD_LENGTH, 0x291c);
    wr32(img + A_CUR_FILENAME, A_CLEAR_NAME);
    for (;;) {
        load_resource(img);
        if ((int16_t)be16(img + A_FILE_HANDLE) >= 0)
            break;
        wr16(img + A_THE_COMMAND, 0x918e);
        run_command_keep_sp(img, 0);
    }
    img[A_DAT_1AD0F] = 0xff;
    /* A7 = 0x1d1fe: el `bsr 0x16210` deja su direccion de retorno en 0x1d1fa */
    wr32(img + 0x1d1fa, 0x16862);
    pal_next_byte(img);
    img[A_DAT_1ACD8] = 2;
}
