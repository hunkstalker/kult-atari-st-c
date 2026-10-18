# input.c

input.c — paquete IKBD y espera de frame.  ikbd_packet_copy @ 0x13686 : copia 5 bytes de A0 (paquete IKBD) a A_IKBD_PACKET (0x1a44f..0x1a453). wait_frame       @ 0x13698 : espera el paquete del IKBD, fija los botones (bit 2 = clic) y llama a GEMDOS Crawio(0xff).  XBIOS Ikbdws(0x19) es no-op en el kit; la llegada del paquete la modela una escritura programada (`schedule`) en el sitio del compare 0x136b0 y el sondeo lo cuenta sched_poll8 (una vez por iteración), como la VBL de 0x13594. 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x13686` | `ikbd_packet_copy` | copia el paquete IKBD de 5 bytes desde A0. | test_input, test_loop |
| `0x13698` | `wait_frame` | wait_frame. | test_input, test_objwait |

## Estado (globales A_* usadas)

- `A_IKBD_PACKET`
- `A_INPUT_BUTTONS`
- `A_SKIP_TRANSITION`

