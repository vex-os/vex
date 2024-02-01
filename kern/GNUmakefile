## SPDX-License-Identifier: GPL-2.0-only
## Copyright (c) 2024, VX/sys Contributors

SOURCES += kern/console.c
SOURCES += kern/fbcon.c
SOURCES += kern/initcall.c
SOURCES += kern/main.c
SOURCES += kern/panic.c
SOURCES += kern/pmm.c
SOURCES += kern/printf.c
SOURCES += kern/slab.c
SOURCES += kern/vmm.c

CLEAN += $(TEMP)/version.c
SOURCES += $(TEMP)/version.c

$(TEMP)/version.c: scripts/gen_version.sh $(TEMP) force_run
	$(SHELL) scripts/gen_version.sh $(RELEASE) $(MACHINE) > $@
