# Kult reconstruction. The shared candidate/oracle/test rules live in the kit.
KIT  := tools/recreate_kit
GAME := kult
# The toolchain here is gcc (Alpine); the kit defaults to clang.
CC   := gcc
include $(KIT)/kit.mk

# Regenera include/addrs.h desde work/names.txt (unica fuente de los globales).
.PHONY: addrs
addrs:
	$(PY) tools/gen_addrs.py --write

# Regenera docs/mapa/ (fichas por modulo + mapa) desde src/, names.txt y test/.
.PHONY: mapa
mapa:
	$(PY) tools/gen_mapa.py
