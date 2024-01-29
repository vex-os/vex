## SPDX-License-Identifier: GPL-2.0-only
## Copyright (c) 2023, VX/sys Contributors

SOURCES += kernel/fbcon.c
SOURCES += kernel/initcall.c
SOURCES += kernel/main.c
SOURCES += kernel/panic.c
SOURCES += kernel/pmm.c
SOURCES += kernel/printf.c
SOURCES += kernel/slab.c
SOURCES += kernel/vmm.c

CLEAN += $(TEMP)/version.c
SOURCES += $(TEMP)/version.c

$(TEMP)/version.c: kernel/version.sh $(TEMP) force_run
	$(SHELL) kernel/version.sh $(RELEASE) $(MACHINE) > $@
