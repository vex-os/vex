## SPDX-License-Identifier: GPL-2.0-only

SOURCES += x86_64/bxcon.c
SOURCES += x86_64/gdt.c
SOURCES += x86_64/idt.c

DISTCLEAN += $(TEMP)/isr_stubs.S
SOURCES += $(TEMP)/isr_stubs.S

$(TEMP)/isr_stubs.S: scripts/gen_isr_stubs_x86.sh $(TEMP)
	$(SHELL) scripts/gen_isr_stubs_x86.sh > $@
