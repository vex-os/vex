## SPDX-License-Identifier: BSD-2-Clause
## Copyright (c) 2023, KanOS Contributors

SOURCES += kern/console.c
SOURCES += kern/init.c
SOURCES += kern/kprintf.c
SOURCES += kern/main.c
SOURCES += kern/panic.c
SOURCES += kern/pmm.c
SOURCES += kern/slab.c
SOURCES += kern/vmm.c

CLEAN += $(TEMP)/version.c
SOURCES += $(TEMP)/version.c

$(TEMP)/version.c: kern/version.sh $(TEMP) force_run
	$(SHELL) kern/version.sh $(RELEASE) $(MACHINE) > $@
