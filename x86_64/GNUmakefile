## SPDX-License-Identifier: BSD-2-Clause
## Copyright (c) 2023, KanOS Contributors

SOURCES += x86_64/bxcon.c
SOURCES += x86_64/gdt.c
SOURCES += x86_64/idt.c

DISTCLEAN += $(TEMP)/isr_stubs.S
SOURCES += $(TEMP)/isr_stubs.S

$(TEMP)/isr_stubs.S: x86_64/isr_stubs.sh $(TEMP)
	$(SHELL) x86_64/isr_stubs.sh > $@
