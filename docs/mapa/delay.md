# delay.c

delay.c — esperas de VBL y retardo por ciclos del original.  wait_vbl      @ 0x13594 : gira hasta que cambie el contador de VBL 0x1acd4. wait_vbl_even @ 0x16d1e : gira hasta que cambie y el nuevo valor sea par.  El contador lo incrementa el handler de VBL (fuera de la imagen), así que la liberación se declara con el modelo de escrituras programadas del kit (`schedule`/`wait_sites`) y el sondeo se cuenta con sched_poll8 una vez por iteración (TRAP_MODEL.md Fase 8). 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x13594` | `wait_vbl` | espera de VBL. (El original deja D0 con el valor anterior del contador; ningún llamante lo consume, así que el out-param no se modela.) | test_delay, test_video |
| `0x16d1e` | `wait_vbl_even` | espera de VBL + contador par (paso de los wipes de texto). | test_delay |

## Estado (globales A_* usadas)

- `A_DAT_1ACD4`

