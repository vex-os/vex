## SPDX-License-Identifier: BSD-2-Clause
## Copyright (c) 2023, KanOS Contributors

export LC_ALL=C
export LANGUAGE=C
export LANG=C

MACHINE ?= x86_64
TOOLCHAIN ?= llvm
RELEASE ?= 0.0.1-dev.8

CP ?= cp
LN ?= ln
RM ?= rm

GREP ?= grep
MKDIR ?= mkdir
TRUE ?= true

SOURCES :=
OBJECTS :=
CLEAN :=
DISTCLEAN :=
ALL_DEPS :=
PHONY_TARGETS :=

TEMP := build
MACH := include/machine

include GNUmakefile.$(MACHINE)
include GNUmakefile.$(TOOLCHAIN)

CFLAGS += -ffreestanding
CFLAGS += -Wall -Wextra -Werror
CFLAGS += -Wno-unused-parameter
CFLAGS += -Wno-pointer-sign
CFLAGS += -funsigned-char
CFLAGS += -O2

CPPFLAGS += -D __kernel__
CPPFLAGS += -D __KERNEL__
CPPFLAGS += -I include
CPPFLAGS += -I usr.include

LDFLAGS += -static
LDFLAGS += -nostdlib

%.c.o: %.c | $(TEMP) $(MACH)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
%.S.o: %.S | $(TEMP) $(MACH)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
%.s.o: %.s | $(TEMP) $(MACH)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

include kern/GNUmakefile
include libkern/GNUmakefile
include $(MACHINE)/GNUmakefile

INITCALLS_C := $(TEMP)/initcalls.c
INITCALLS_O := $(TEMP)/initcalls.o
KERNEL_BINARY := $(TEMP)/kernel.elf
KERNEL_NOINIT := $(TEMP)/kernel_noinit.o
LDSCRIPT := $(TEMP)/ldscript.ld

OBJECTS += $(SOURCES:=.o)

DISTCLEAN += $(TEMP) $(MACH)
CLEAN += $(OBJECTS)
CLEAN += $(INITCALLS_C)
CLEAN += $(INITCALLS_O)
CLEAN += $(KERNEL_BINARY)
CLEAN += $(KERNEL_NOINIT)
CLEAN += $(LDSCRIPT)

PHONY_TARGETS += all
PHONY_TARGETS += kernel clean distclean
PHONY_TARGETS += force_run
ALL_DEPS += kernel

-include boot/$(MACHINE)/GNUmakefile

all: $(ALL_DEPS)

kernel: $(KERNEL_BINARY)

clean:
	$(RM) -vrf $(CLEAN)

distclean:
	$(RM) -vrf $(CLEAN)
	$(RM) -vrf $(DISTCLEAN)

$(TEMP):
	$(MKDIR) -p $(TEMP)

$(MACH):
	$(LN) -sf $(abspath include/$(MACHINE)) ./include/machine

force_run:

$(INITCALLS_C): $(KERNEL_NOINIT) | initcalls.sh $(TEMP) $(MACH)
	$(SHELL) initcalls.sh $^ > $@

$(KERNEL_BINARY): $(INITCALLS_O) $(KERNEL_NOINIT) | $(LDSCRIPT) $(TEMP) $(MACH)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -o $@ $^

$(KERNEL_NOINIT): $(OBJECTS) | $(TEMP) $(MACH)
	$(LD) $(LDFLAGS) -r -o $@ $^

$(LDSCRIPT): $(MACHINE)/ldscript.lds | $(TEMP) $(MACH)
	$(CC) $(CPPFLAGS) -E -xc -D __ASSEMBLER__ $^ | $(GREP) -v "^#" > $@ || $(TRUE)
