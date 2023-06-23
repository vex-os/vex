## SPDX-License-Identifier: BSD-2-Clause
## Copyright (c) 2023, KanOS Contributors

SOURCES += sys/fbcon.c
SOURCES += sys/initcall.c
SOURCES += sys/main.c
SOURCES += sys/panic.c
SOURCES += sys/printf.c

CLEAN += $(TEMP)/version.c
SOURCES += $(TEMP)/version.c

$(TEMP)/version.c: sys/version.sh $(TEMP) force_run
	$(SHELL) sys/version.sh $(RELEASE) $(MACHINE) > $@
