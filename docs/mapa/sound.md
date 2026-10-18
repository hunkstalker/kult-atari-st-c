# sound.c

sound.c — envoltorio de escritura del PSG (0x1929a).  FUN_0001929a @ 0x1929a (ym_write): `move.w #$2700,sr` (sin efecto en imagen), selecciona el registro D0 escribiendo $ff8800, y para el registro 7 (mixer) hace un read-modify-write: lee el puerto de DATOS $ff8802, fusiona los bits 0-5 de D1 y conserva los bits 6-7 (direccion de los puertos A/B). Despues escribe D1 en el registro. En hardware real, leer $ff8802 es bus abierto (0xFF): el chip solo responde por $ff8800; el kit sirve OS_PSG_DATA_READ (ver os.h/TRAP_MODEL.md).  El candidato no tiene un "select" propio (su API selecciona y accede en la misma llamada), asi que un select con D0 > 0x0f diverge del original — que lo rechaza dejando el latch anterior y escribiendo el dato ahi. Ningun llamante lo hace. 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x1929a` | `ym_write` | — | test_sound |
| `0x19282` | `ym_reset` | FUN_00019282 @ 0x19282: silencia/resetea el PSG: escribe D1 (0 inicial) en los registros 0xd..0 y luego 0x18 en el mixer (registro 7). D1 NO se recarga en el bucle: ym_write lo pisa con el resultado del RMW al pasar por el registro 7, y los registros 6..0 reciben ese valor (el ledger del oraculo lo confirma). | test_sound |
| `0x18fc4` | `sound_timer_start` | FUN_00018fc4 @ 0x18fc4: entrada canonica (D1 = 0). | test_sound |
| `0x18fc6` | `sound_timer_start_body` | FUN_00018fc6 @ 0x18fc6: entrada con D1 explicito (la usa 0x18f7a). | test_sound |
| `0x19084` | `sound_timer_stop` | FUN_00019084 @ 0x19084: para el Timer A: TACR=0, IERB=0, IERA=0 y limpia 0x192d0. El `tst.b $fa19; bne` espera a que TACR quede a 0; el modelo lo sirve 0. | test_sound |
| `0x1909e` | `sound_off` | FUN_0001909e @ 0x1909e: apaga el sonido: limpia 0x484, resetea el PSG (0x19282), silencia el mixer (ym_write 7 con 0x3f) y para el Timer A (0x19084). | test_sound |
| `0x1906a` | `sound_vector` | FUN_0001906a @ 0x1906a: arranca el Timer A (0x18fc4), apunta el vector $134 a la variante 0x190f6 y reinicia el puntero de sample a 0x192ca. | test_sound |
| `0x18f7a` | `supexec_b` | ---- envoltorios XBIOS Supexec (0x18f6e..0x18fa4) ----  Todos saltan el `trap #14/$26` cuando 0x1ab4f != 0 (el original tambien): el candidato ejecuta la rutina destino directamente. Cada envoltorio elige una entrada distinta; 0x18f7a usa 0x18fc6 (D1 explicito) y 0x18f8e la canonica 0x18fc4. | test_sound |
| `0x18f84` | `sound_stop_supexec` | — | test_sound |
| `0x18f8e` | `sound_play_a` | — | test_sound |
| `0x18f98` | `sound_play_b` | — | test_sound |
| `0x18fa2` | `supexec_a` | — | test_sound |
| `0x18fa4` | `sound_stop` | — | test_sound |
| `0x18f6e` | `supexec_wrap` | FUN_00018f6e @ 0x18f6e: handler de byte que lee D4/D1 de A6[1]/A6[2], A6 += 3 y cae en 0x18f7a. | test_sound |
| `0x190b8` | `sound_isr_load` | FUN_000190b8 @ 0x190b8: carga el siguiente bloque de samples (entrada propia). | test_sound |
| `0x1910e` | `sound_isr_fwd` | FUN_0001910e @ 0x1910e: ISR PCM hacia delante (vector $134). | test_sound |
| `0x190f6` | `sound_isr_back` | FUN_000190f6 @ 0x190f6: ISR PCM hacia atras (vector $134, variante 0x1906a). | test_sound |

## Estado (globales A_* usadas)

- `A_FRAME_1AB4F`

