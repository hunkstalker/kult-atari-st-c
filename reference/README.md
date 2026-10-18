# `reference/` — material del intento anterior (no se ejecuta)

Carpeta **solo de consulta**. No forma parte del build ni de la suite; no la
ejecutes ni la importes desde `test/`.

## `test_f4/`

Las **469 pruebas diferenciales** del intento de reimplementación anterior
(heredado de kult-port), **atadas a los nombres viejos** (`g_fun_XXXX`,
`FUN_0001xxxx`, …) en vez de a los nombres semánticos de este repo
(`work/names.txt`). Se conservan como **referencia de comportamiento**: dicen
qué casos y qué trampas ya se habían cubierto, y sirven para comparar al
escribir la prueba equivalente en `test/`.

- **No se ejecutan**: importan `harness` del kit y símbolos que ya no existen con
  esos nombres. La suite viva es `test/` (494 pruebas, `make test`).
- Contenido: 38 `test_*.py` + `harness.py`, con la misma organización por
  subsistema que `test/` (anim, dispatch, handlers, loop, music, sound, screens,
  select, textblit, textbox, …).
- Uso típico al revisar/portar una función: abrir el `test_f4/test_<módulo>.py`
  correspondiente y comprobar qué rutas y casos límite ya estaban cubiertos, para
  no perder cobertura al reescribir la prueba con los nombres nuevos.

La correspondencia nombre viejo → dirección → nombre nuevo vive en
`work/names.txt` y en los comentarios `/* 0xADDR — … */` de `src/`.
