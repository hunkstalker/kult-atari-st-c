# dispatch.c

dispatch.c — el despacho de comandos y la VM de bytes del script.  run_command @ 0x103a8 / run_command_keep_sp @ 0x103ae: leen `the_command` ($1a50e) y despachan por su nibble alto: 0x0   -> a6 = TEMPL(0x1d402) + comando, y FUN_00010630 (VM de bytes) 0xa/b -> handler de la tabla cmd_jump_table (0x19eca, 144 entradas) 0x9   -> caja de texto (0x10440)  [pendiente] 0xf/otro -> walk_entry(TEMPL, (cmd&0x3ff)-1), $1a4e0 = fin, FUN_00010630  FUN_00010630 @ 0x10630: VM que lee bytes de a6 y ejecuta el handler de la tabla 0x19f62 mientras el byte esté en 1..0x6a y a6 no haya llegado a $1a4e0. 

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x10630` | `vm_run` | — | test_dispatch, test_dispatch_wiring, test_textblit |
| `0x104f4` | `inline_cmd_walk` | FUN_000104f4 @ 0x104f4 (byte 18): the_command = word inline (a6[1..2]), walk_entry con (templ, cmd-1) y devuelve A5 (el nuevo stream). | test_dispatch |
| `0x103ae` | `run_command_keep_sp` | — | test_dispatch |
| `0x10390` | `dispatch_a6` | FUN_00010390 @ 0x10390: handler de byte que lee un word de a6[1..2], lo marca con el nibble 0xf000 (comando de plantilla) y salta al despacho. | test_dispatch |
| `0x186ee` | `dispatch_cmd` | — | test_dispatch |

## Estado (globales A_* usadas)

- `A_TEMPL_DATA`
- `A_TEMPL_PTR`
- `A_THE_COMMAND`

