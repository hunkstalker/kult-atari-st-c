# REVISIÓN del C (previa a la fase 3 de kult-port)

Objetivo: auditar el C ya escrito y **renombrado** —antes de re-aplicar encima
las mejoras de kult-port— para asegurar que es fiel, correcto y legible. No se
modifica el comportamiento salvo que la revisión encuentre un fallo real
(entonces: arreglo + prueba + commit).

Este documento es el plan y el prompt de arranque. La revisión se hace en una
**sesión nueva y limpia**, sin tocar nada de kult-port.

## 1. Por qué revisar (riesgos conocidos)

- El **bloque central** se importó del C viejo verificado (F4) y luego se
  **renombró dos veces** (nombres viejos → convenciones nuevas → nombres
  semánticos). Cada pasada es una oportunidad de deriva.
- El renombrado se hizo con un `span` que al principio cogía el **prototipo** en
  vez de la definición (corregido); conviene confirmar que no quedó ninguna
  función a medias.
- Los **tests diferenciales solo prueban las rutas que ejecutan**. La revisión
  debe mirar sobre todo **rutas no cubiertas** y semántica de bajo nivel
  (banderas, anchos, orden de escritura).
- Hay **dos módulos sin test dedicado** (`commands`, `video`) y otros con poca
  densidad de pruebas por función.

## 2. Alcance y prioridades

Origen: **A** = portado a mano al principio (revisado al escribir, bajo riesgo);
**B** = importado del C viejo y renombrado (mayor riesgo).

| prio | módulos | motivo |
|---|---|---|
| **P0** | `commands`, `video`, `dispatch`, `handlers`, `loop`, `scale`, `textbox`, `select`, `mode` | importados, grandes, poca prueba dedicada / sin ficha de test propia |
| **P1** | `anim`, `element`, `icons`, `music`, `sound`, `fileio`, `textblit`, `textform`, `reveal`, `misc`, `vm` | importados, tamaño medio |
| **P2** | `palette`, `cursor`, `input`, `delay`, `boxdraw`, `line`, `maskblit`, `sprite`, `text`, `overlay`, `screens`, `state`, `stubs` | portados a mano o ya auditados; repaso ligero |

Censo (líneas / funciones-definición / pruebas `def test_`):

```
anim 597/34/24   boxdraw 149/9/5    commands 686/18/0*  cursor 71/3/2
delay 32/2/2     dispatch 519/152/21 element 610/44/17  fileio 252/24/20
handlers 864/85/32 icons 199/22/10  input 42/2/4        line 185/5/3
loop 2456/153/96 maskblit 282/18/9  misc 65/16/8        mode 250/31/10
music 602/22/28  overlay 809/12/8   palette 71/6/6      reveal 134/8/4
scale 466/18/6   screens 528/32/6   select 334/25/8     sound 257/23/18
sprite 189/8/7   state 56/4/4       stubs 12/4/1        text 303/18/17
textblit 546/37/25 textbox 434/30/7 textform 171/14/7   video 199/5/0*
vm 47/4/3
```
`*` `commands`/`video` se ejercitan indirectamente (`test_family151.py`,
`test_loop.py`) pero **no tienen `test_<módulo>.py`**.

Total: 12.417 líneas, 456 definiciones, 478 pruebas (418 `def test_` + casos
parametrizados), 0 warnings, `ids_reg=0`.

## 3. Los cuatro ejes (checklist)

### 3.1 Fidelidad de comportamiento vs ASM
Para cada función, comparar el cuerpo C con su `auto_*.s` en
`/app/kult-atari-st-decomp/matching/`:
- [ ] **Anchos**: operaciones `.b`/`.w`/`.l` → tipo correcto (`uint8/16/32_t`),
      sin truncar ni extender de más.
- [ ] **Signo**: `ext.w`/`ext.l`, `lsl/lsr/asr`, comparaciones `bcc/bcs/bge/blt`
      (con/sin signo) bien traducidas.
- [ ] **Banderas**: si el original ramifica por C/Z/N/V, el C reproduce esa
      condición exacta (no una equivalente "casi").
- [ ] **Pre/post incremento** de `aN`/`aN+`/`-aN` y desplazamiento de guion
      (`a6`), incluida la cola compartida (fall-through) entre funciones.
- [ ] **Bucles**: límites, off-by-one, `dbra`, orden de iteración, parada.
- [ ] **Orden de escritura** cuando importa (solapamientos, VRAM/planar).
- [ ] **Registros de salida**: los que la prueba compara (`info["regs"]`) y los
      que se devuelven/por out-param coinciden con el original.
- [ ] **Efectos off-image** (XBIOS Setpalette, PSG, IKBD): van al ledger y se
      comparan; confirmar que el C los emite igual (función, orden, datos).
- [ ] **`movem`/guardas**: si el original preserva registros que el C trata como
      salida, o viceversa.
- [ ] **`rts`/fall-through**: funciones que "caen" en el cuerpo de otra están
      modeladas como llamada/continuación, no duplicadas ni reordenadas.

### 3.2 Nombres y comentarios
- [ ] El nombre C = nombre en `work/names.txt` (convención §4.1).
- [ ] El comentario `/* 0xADDR — … */` corresponde a la etiqueta real del ASM y
      describe **lo que hace** (no un copia-pega erróneo del vecino).
- [ ] Out-params y valores de retorno documentados y coherentes con la firma.
- [ ] Comentarios que citan registros (`d0`, `a6`, …) son **correctos** (es la
      traza al ASM; se permite nombrarlos ahí).
- [ ] No quedan nombres engañosos del primer renombrado (p. ej. un `_tail` que en
      realidad es la cabecera).

### 3.3 Legibilidad / consistencia de estilo (convenciones §4.1)
- [ ] Imagen plana `uint8_t *img` + accesos big-endian (`be16/be32/wr16/wr32`);
      sin `memcpy` de structs ni punteros crudos a mitad de palabra.
- [ ] Offsets vía macros `A_*` de `include/addrs.h` (nada de números mágicos
      repetidos).
- [ ] Helpers compartidos de `include/kult_internal.h` usados donde toca.
- [ ] Tipos correctos y firmas coherentes; sin casts peligrosos.
- [ ] Sin **UB**: shifts ≥ ancho del tipo, overflow con signo, aliasing,
      uninitialized.
- [ ] 0 warnings (`-Wall -Wextra`) y 0 `ids_reg` (`make regscan`).
- [ ] Sin restos de decompilador (`FUN_xxxx`, `loc_`, `sub_`, variables `v1`…).

### 3.4 Cobertura de pruebas
- [ ] Cada entrada del decomp tiene su caso en `test/test_<módulo>.py`.
- [ ] Cada **rama** de una función multi-camino tiene un caso (si el ASM ramifica,
      probar ambos lados).
- [ ] Casos **límite**: 0, `0xff`/`0xffff`, wrap de palabra, frontera de bucle,
      índice fuera de tabla (comportamiento del original, no del C "ideal").
- [ ] Se comparan imagen **y** registros **y** ledger off-image.
- [ ] `commands.c` y `video.c`: crear (o justificar por qué no) `test_<módulo>.py`.

## 4. Procedimiento por módulo (plantilla de sesión)

1. `make regscan` y `make mapa` (snapshot de deuda y fichas).
2. Abrir `docs/mapa/<módulo>.md`: lista de funciones, globales y pruebas.
3. Por cada función (empezando por las de salida pública y las ramificadas):
   - Abrir su `auto_*.s`; recorrer instrucción a instrucción contra el C.
   - Marcar divergencias con la **plantilla de hallazgo** (§5).
   - Comprobar la cobertura de test (§3.4).
4. Al cerrar el módulo: aplicar arreglos + añadir pruebas, `make test` verde,
   `make regscan` sigue en 0, actualizar `docs/mapa/` (`make mapa`).
5. Commit por módulo: `revision(<módulo>): …`.

Orden sugerido: P0 de menor a mayor tamaño para calibrar, luego P1, luego P2.

## 5. Plantilla de hallazgo

```
[H-<n>] <severidad ALTA|MEDIA|BAJA> · <módulo> · <función> (0xADDR)
Qué:      <descripción del problema>
ASM:      <auto_xx.s:NN o fragmento>
C:        src/<módulo>.c:NN
Evidencia:<por qué es un fallo (no estilo)>
Arreglo:  <cambio propuesto> + <test que lo cubre>
Estado:   abierto | arreglado (<commit>) | intencional (divergencia documentada)
```

Severidad: **ALTA** = resultado/efecto distinto al original; **MEDIA** = solo en
rutas no probadas / riesgo de UB; **BAJA** = nombre/comentario/estilo.

## 6. Criterio de cierre

- Todos los módulos **P0 y P1** revisados con checklist completa.
- Hallazgos ALTA/MEDIA arreglados **con prueba** o marcados *intencional* con
  justificación (p. ej. reconstrucción de una cola del original).
- `make test` verde (478+), `make regscan` `ids_reg=0`, `make core` OK.
- `docs/REVISION.md` con el **registro de hallazgos** y `docs/ESTADO.md`
  actualizado. Solo entonces se abre la fase 3.

## 7. Prompt para la sesión de revisión (copiar y pegar)

---
Trabajamos en la reimplementación en C de **Kult: The Temple of Flying Saucers**
(Atari ST) en `/app/kult-atari-st-c`. El decomp ASM canónico
(`/app/kult-atari-st-decomp`, byte a byte, **no se toca**) es la verdad.

Antes de tocar nada de kult-port, quiero **revisar el C ya escrito y renombrado**.
Lee, por este orden: `docs/REVISION.md` (este plan), `docs/ESTADO.md`,
`docs/PLAN.md` (§4.1 convenciones) y `docs/REFERENCIA_F4.md`.

Haz la revisión siguiendo `docs/REVISION.md`: los cuatro ejes (fidelidad vs ASM,
nombres/comentarios, legibilidad/estilo, cobertura de tests), empezando por la
prioridad P0. Por cada función compara el cuerpo C con su `auto_*.s` en
`/app/kult-atari-st-decomp/matching/`, registra hallazgos con la plantilla §5 y,
si hay fallo real, arréglalo **con prueba** y deja `make test` verde.

Reglas: reproducir/medir antes de tocar, una hipótesis un cambio, toda corrección
deja test, commit por módulo. Todo en español. No toques kult-port.
---

## 8. Registro de hallazgos

(Se rellena durante la revisión. Vacío al crear el plan.)

| id | sev | módulo | función | estado |
|---|---|---|---|---|
| — | — | — | — | — |
