# select.c

select.c — seleccion de samples y su ventana (0x15492/0x15510).  0x15492 recorre la tabla de 0x3f entradas de samples (0x1a8f0, 6 B) y para cada una cuyo word cumple `(word & D2) == D3` copia el registro a 0x1a6ae+4/6/8 (nombre, longitud y color 0x3f-indice) y lo dibuja con 0x15510. La primera coincidencia abre la ventana (0x1276c), arranca el sample (0x18f8e con D4=0x14) y toma el flag de $1a488; al final publica el contador en $1a510. 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x15510` | `sample_select2` | FUN_00015510 @ 0x15510: imprime el registro A2 (nombre en A2[0], fila en A2[2]) con 0x11020 y D4; avanza A2 0xa y devuelve D6 incrementado (el original lo hace en los registros, y 0x15492 encadena registros de 10 bytes). | test_select |
| `0x15492` | `sample_select` | FUN_00015492 @ 0x15492: recorre la tabla 0x1a8f0 y publica el registro elegido. Devuelve D6 (el contador que el original deja en el registro y en $1a510). | test_select |
| `0x14db2` | `sample_menu` | FUN_00014db2 @ 0x14db2: marco + lista de samples + bucle de espera + Timer A. | test_select |
| `0x14d76` | `sample_menu_entry_a` | FUN_00014d76 @ 0x14d76: entrada del menu (D2/D3 = 0x8000, filtro 0x00ff). | test_select |
| `0x14d90` | `sample_menu_entry_b` | FUN_00014d90 @ 0x14d90: entrada del menu con el filtro de $1acd2. | test_select |
| `0x149da` | `action_menu` | FUN_000149da @ 0x149da: entrada del menu de acciones; devuelve el A6 avanzado (el original lo deja en el registro). | test_select |
| `0x14ac6` | `sample_menu_a` | Un handler de byte de la VM (tabla 0x19f62): recibe a6 y devuelve el a6 nuevo. El avance de A6 es el neto del handler original (los `addq`/push-pop), dato qu... | test_loop |
| `0x14bfa` | `sample_menu_b` | FUN_00014bfa @ 0x14bfa: variante del menu de samples (comando inicial 0x9099). Como 0x14ac6 pero exige que la entrada elegida este en {0x68,0x7a,0x6b,0x69}, corre 0xc1c6 (que puede fijar $1ad04) y comparte las colas. A6 = entry+1. | test_loop |

## Estado (globales A_* usadas)

(ninguna)

