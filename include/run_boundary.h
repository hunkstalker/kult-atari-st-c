/* run_boundary.h — retorno no local de run_command (0x103a8) para 0xfxxx.
 *
 * El original guarda A7 en $1a826 y 0x103ae lo restaura para comandos 0xfxxx;
 * el rts del VM vuelve entonces al llamante de run_command. En C el equivalente
 * es setjmp/longjmp. La pila de boundaries vive en handlers.c.
 */
#ifndef KULT_RUN_BOUNDARY_H
#define KULT_RUN_BOUNDARY_H

#include <setjmp.h>

typedef struct kult_run_boundary {
    jmp_buf env;
    struct kult_run_boundary *prev;
} kult_run_boundary;

extern kult_run_boundary *g_kult_run_boundary_top;

static inline int run_unwind_boundary_active(void) {
    return g_kult_run_boundary_top != 0;
}

static inline void run_unwind_to_boundary(void) {
    if (g_kult_run_boundary_top)
        longjmp(g_kult_run_boundary_top->env, 1);
}

#endif
