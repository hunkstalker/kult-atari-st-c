# dispatch.c

dispatch.c — el despacho de comandos y la VM de bytes del script.  run_command @ 0x103a8 / run_command_keep_sp @ 0x103ae: leen `the_command` ($1a50e) y despachan por su nibble alto: 0x0   -> script = TEMPL(0x1d402) + comando, y FUN_00010630 (VM de bytes) 0xa/b -> handler de la tabla cmd_jump_table (0x19eca, 144 entradas) 0x9   -> caja de texto (0x10440)  [pendiente] 0xf/otro -> walk_entry(TEMPL, (cmd&0x3ff)-1), $1a4e0 = fin, FUN_00010630  FUN_00010630 @ 0x10630: VM que lee bytes de script y ejecuta el handler de la tabla 0x19f62 mientras el byte esté en 1..0x6a y script no haya llegado a $1a4e0. 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x10630` | `vm_run` | vm run | test_dispatch, test_dispatch_wiring, test_textblit |
| `0x104f4` | `inline_cmd_walk` | FUN_000104f4 @ 0x104f4 (byte 18): the_command = word inline (script[1..2]), walk_entry con (templ, cmd-1) y devuelve A5 (el nuevo stream). | test_dispatch |
| `0x103ae` | `run_command_keep_sp` | repite la carga de A:CLEAR.BIN (0x291c B en 0x1a752) hasta que exista (si falla lanza 0x918e); al exito reinicia la pila (A7 = 0x1d1fe), marca $1ad0f, avanza... | test_dispatch |
| `0x10390` | `dispatch_a6` | FUN_00010390 @ 0x10390: handler de byte que lee un word de script[1..2], lo marca con el nibble 0xf000 (comando de plantilla) y salta al despacho. El comando formado es siempre 0xf000, asi que 0x103ae toma la rama `walk_entry(TEMPL, cmd-1) + vm_run` y deja A6 = fin del sub-guion; el handler DEVUELVE ese A6 (el `bra f_0103ae` deja que el rts del sub-VM vuelva al VM llamante, que continua en esa direccion). | test_dispatch |
| `0x186ee` | `dispatch_cmd` | dispatch cmd | test_dispatch |

## Estado (globales A_* usadas)

- `A_TEMPL_DATA`
- `A_TEMPL_PTR`
- `A_THE_COMMAND`

