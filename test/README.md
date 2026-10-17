# test/ — pruebas diferenciales

- `harness.py` — encuaderna el arnés (`tools/recreate_kit`) con este proyecto.
- `test_*.py` — una prueba por pieza reconstruida (empiezan a escribirse con el
  piloto de paleta).

Cada prueba ejecuta el **original** en el oráculo (Musashi) con una entrada y
compara con tu C (memoria + registros; el hardware va por el *ledger*).
Patrón: ver `reference/test_f4/` (pruebas del intento anterior) y la doc del kit
en `tools/recreate_kit/README.md`.
