# stubs.c

stubs.c — funciones que son un `rts` pelado (no-ops verificables). 0x135e0, 0x160c0, 0x16dba, 0x19312.

## Funciones

| dir | nombre | qué hace | pruebas |
|---|---|---|---|
| `0x135e0` | `noop_135e0` | `rts` (stub sin efecto) | test_stubs |
| `0x160c0` | `noop_160c0` | `rts` (stub sin efecto) | test_stubs |
| `0x16dba` | `noop_16dba` | `rts` (stub sin efecto) | test_stubs |
| `0x19312` | `noop_19312` | `rts` (stub sin efecto; FDC fuera de alcance) | test_stubs |

## Estado (globales A_* usadas)

(ninguna)

