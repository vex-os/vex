## SPDX-License-Identifier: BSD-2-Clause
## Copyright (c) 2023, KanOS Contributors

SOURCES += kernel/console.c
SOURCES += kernel/init.c
SOURCES += kernel/kprintf.c
SOURCES += kernel/main.c
SOURCES += kernel/panic.c
SOURCES += kernel/pmm.c
SOURCES += kernel/slab.c
SOURCES += kernel/vmm.c

CLEAN += $(TEMP)/version.c
SOURCES += $(TEMP)/version.c

$(TEMP)/version.c: kernel/version.sh $(TEMP) force_run
	$(SHELL) kernel/version.sh $(RELEASE) $(MACHINE) > $@
