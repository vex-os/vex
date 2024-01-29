## SPDX-License-Identifier: GPL-2.0-only
## Copyright (c) 2023, VX/sys Contributors

SOURCES += x86_64/bxcon.c
SOURCES += x86_64/gdt.c
SOURCES += x86_64/idt.c

DISTCLEAN += $(TEMP)/isr_stubs.S
SOURCES += $(TEMP)/isr_stubs.S

$(TEMP)/isr_stubs.S: x86_64/isr_stubs.sh $(TEMP)
	$(SHELL) x86_64/isr_stubs.sh > $@
