# music.c

music.c — handlers del motor de musica del PSG (0x3aa38/0x3aa8a/0x3aa7c/0x3a85e).  Empezando por las hojas: 0x3aa7c es el write-verify del protocolo directo del chip (selecciona D6, escribe D0, relee $ff8800 hasta que devuelve D0); 0x3aa38 es el handler del `trap #3` (vector 0x23 = 0x8c: lo instala init_hardware con Setexc) y 0x3aa8a su gemelo que escribe la tabla sombra en la imagen ($3ae7c). 0x3a85e es el silenciador que emite tres `trap #3` (registros 8/9/10 a 0).  La semantica del trap es del CPU (TRAP #n -> vector n+32) y el handler vive en la imagen: el candidato no tiene traps, asi que llama a la reconstruccion del handler (psg_trap3), exactamente como el oraculo llega a el via vector.  Los accesos PSG van por psg_port_write/psg_port_read (ledger + fichero del kit). El read-back del write-verify sale de lo recien escrito, asi que el bucle siempre da una pasada; el ledger del oraculo registra WRITE+READ por acceso y el candidato igual. 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x3aa7c` | `psg_write_verify` | FUN_0003aa7c @ 0x3aa7c: escribe el registro D6 con D0 y verifica releyendo $ff8800. En el modelo el chip devuelve lo escrito, asi que la verificacion pasa a la primera; aun asi el READ es un evento del ledger y hay que emitirlo. | test_music |
| `0x3aa38` | `psg_trap3` | FUN_0003aa38 @ 0x3aa38: handler del `trap #3`. D2 es el registro base (bit 15: modo par relativo, D6 = 2*D7 + D2), D7 el desplazamiento y D0 el valor. Con D2==7 hace el RMW del mixer: lee el registro, conserva D0[0..5] y los bits 6-7 leidos.  D0 se restaura al final (el original lo salva en la pila); D5/D6/A5 son scratch. | test_music |
| `0x3aa8a` | `psg_shadow` | FUN_0003aa8a @ 0x3aa8a: gemelo del handler que escribe la tabla sombra $3ae7c en vez del chip. Modo par relativo: word de D0 en 0(a5,d6.w); si no, byte. | test_music |
| `0x3a85e` | `music_silence` | FUN_0003a85e @ 0x3a85e: silencia los tres canales emitiendo `trap #3` con D0=0 y D2=8 (registros de volumen 8/9/10), D7 de 0 a 2. El original fija D2 con `move.w` (conserva la mitad alta) y D0/D7 con `moveq` (32 bits). | test_music |
| `0x3a604` | `mfp_setup` | FUN_0003a604 @ 0x3a604: mfp_setup. Habilita Timer A (IERA/IMRA bit 5), lo pone en delay con /? (TACR=4) y recarga TADR=0xAA; instala el reproductor 0x3a872 en el vector $134 (Timer A con VR=0x40 de TOS). | test_music |
| `0x3a5c2` | `music_install` | FUN_0003a5c2 @ 0x3a5c2: Supexec(0x3a604) y Setexc(0x23,0x3aa38)/Setexc(0x24,0x3aab6). Devuelve en D0 el handler anterior del ultimo vector (0x90). | test_music |
| `0x3a57c` | `music_cmd_table` | FUN_0003a57c @ 0x3a57c: recorre la tabla A0 ({word clave, long handler}, fin en 0) y salta al handler cuya clave es D1. Devuelve el carry resultante: 0 encontrado, 1 agotada la tabla (`ori.b #1,ccr`). | test_music |
| `0x3a62e` | `music_load` | FUN_0003a62e @ 0x3a62e: carga una cancion. `selector` indexa la cabecera `base` (0x3b1ac): word de offset al bloque; de ahi salen la duracion, las tablas del estado (4/8/0xc/0x10), el puntero de cancion (canal[0]) y los flags/volumenes iniciales. Termina silenciando (0x3a85e) y limpiando el contador de estado. D2 entra sin tocar. | test_music |
| `0x3a734` | `music_cmd` | FUN_0003a734 @ 0x3a734: procesa el comando D1 (word) con valor D5 (word) sobre los canales que seleccionan D1[0..2] (si son 0, los tres). Devuelve D0 = 0xffff o el minimo 0x24 visto. Antes despacha D1 a 0x3a57c (comandos 0x400/0x800/0x1000). | test_music |
| `0x3a556` | `init_hardware` | FUN_0003a556 @ 0x3a556: init_hardware. D2 entra sin tocar y llega hasta 0x3a85e. | test_music |
| `0x3a712` | `music_stop2` | FUN_0003a712 @ 0x3a712: para el Timer A (TACR=0) y deshabilita su interrupcion. El `tst.b $fffa19` del original espera a que el chip tome el 0; el modelo lo sirve 0 en la primera lectura, asi que no se transcribe. | test_music |
| `0x3a6f4` | `music_stop` | FUN_0003a6f4 @ 0x3a6f4: Supexec(0x3a712) + silenciar. D2 entra sin tocar. | test_music |
| `0x3a992` | `music_note` | FUN_0003a992 @ 0x3a992: un paso del generador de envolvente de la voz A0. Avanza el indice del instrumento (A0[0xa]), acumula la modulacion (A0[0] += byte con signo) y el acumulador A0[2]; devuelve el periodo ajustado (D0 - A0[0]) con suelo en 0. Los flags A0[0xb]: bit0 = aplica modulacion, bit2 = ciclo terminado. | test_music |
| `0x3ad5a` | `music_h1` | cmd 0 (0x3ad5a): transposicion desde el stream. | test_music |
| `0x3ad6a` | `music_h2` | cmd 2 (0x3ad6a): cambia el instrumento de la voz 2 (channel[0x14]) con la tabla del estado [8]; flag 0. | test_music |
| `0x3adb4` | `music_h3` | cmd 1 (0x3adb4): como el anterior pero con el flag 0x80 (loop) en la voz 2. | test_music |
| `0x3ae00` | `music_h4` | cmd 3 (0x3ae00): envolvente/volumen de la voz (channel[0x18]) a partir de dos bytes; limpia el bit de canal en A0[6] via la tabla de bits. | test_music |
| `0x3ae68` | `music_h5` | cmd 4 (0x3ae68): volumen directo (channel[0x2b]) y lo refleja en el stream. No reescribe channel[8] (el original consume el byte con (a0)+ sin guardarlo). | test_music |
| `0x3aab6` | `music_tick` | FUN_0003aab6 @ 0x3aab6: handler del `trap #4` (vector 0x24). Un tick de un canal: fade de volumen, nota o comando (transposicion, instrumentos, envolvente, duracion), fin de patron/cancion y el bloque de canal enlazado. A3 = canal, A6 = estado global, D7 = indice de canal. Termina en el `rte` 0x3ad58 (aqui, return). | test_music |

## Estado (globales A_* usadas)

(ninguna)

