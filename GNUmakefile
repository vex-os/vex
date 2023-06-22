## SPDX-License-Identifier: BSD-2-Clause
## Copyright (c) 2023, KanOS Contributors

export LC_ALL=C
export LANGUAGE=C
export LANG=C

MACHINE ?= x86_64
TOOLCHAIN ?= llvm
RELEASE ?= 0.0.1-dev.9

SOURCES :=
OBJECTS :=
CLEAN :=
DISTCLEAN :=
ALL_DEPS :=
PHONY_TARGETS :=

TEMP := build
MINC := include/machine

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

%.c.o: %.c | $(TEMP) $(MINC)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
%.S.o: %.S | $(TEMP) $(MINC)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
%.s.o: %.s | $(TEMP) $(MINC)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

INIT_C := $(TEMP)/initcalls.c
INIT_O := $(TEMP)/initcalls.o
NOINIT_O := $(TEMP)/noinit.o
LDSCRIPT := $(TEMP)/ldscript.ld
KERNEL := kan.elf

include boot/GNUmakefile
include kernel/GNUmakefile
include libkern/GNUmakefile
include $(MACHINE)/GNUmakefile

OBJECTS += $(SOURCES:=.o)

DISTCLEAN += $(TEMP) $(MINC)
CLEAN += $(OBJECTS)
CLEAN += $(INIT_C)
CLEAN += $(INIT_O)
CLEAN += $(KERNEL)
CLEAN += $(NOINIT_O)
CLEAN += $(LDSCRIPT)

PHONY_TARGETS += all
PHONY_TARGETS += kernel clean distclean
PHONY_TARGETS += force_run
ALL_DEPS += kernel

all: $(ALL_DEPS)

kernel: $(KERNEL)

clean:
	rm -vrf $(CLEAN)

distclean:
	rm -vrf $(CLEAN)
	rm -vrf $(DISTCLEAN)

$(TEMP):
	mkdir -p $(TEMP)

$(MINC):
	ln -sf $(abspath include/$(MACHINE)) ./include/machine

force_run:

$(INIT_C): $(NOINIT_O) | initcalls.sh $(TEMP) $(MINC)
	$(SHELL) initcalls.sh $^ > $@

$(KERNEL): $(INIT_O) $(NOINIT_O) | $(LDSCRIPT) $(TEMP) $(MINC)
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -o $@ $^

$(NOINIT_O): $(OBJECTS) | $(TEMP) $(MINC)
	$(LD) $(LDFLAGS) -r -o $@ $^

$(LDSCRIPT): $(MACHINE)/ldscript.lds | $(TEMP) $(MINC)
	$(CC) $(CPPFLAGS) -E -xc -D __ASSEMBLER__ $^ | grep -v "^#" > $@ || true
