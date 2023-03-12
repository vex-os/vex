# SPDX-License-Identifier: BSD-2-Clause 
# Copyright (c), 2022, KanOS Contributors 
TARGET ?= x86_64
TOOLCHAIN ?= clang
VERSION := 0.0.0-dev.4

CP ?= cp
GREP ?= grep
MKDIR ?= mkdir
SH ?= sh
RM ?= rm

SOURCES :=
OBJECTS :=
CLEAN :=
DISTCLEAN :=
ALL_DEPS :=

# This should define these:
#	C_TARGET (target argument for clang, e.g x86_64-none-elf)
#	G_TARGET (target triplet for gcc, e.g. x86_64-elf)
#	L_TARGET (full target name, e.g. x86_64)
#	S_TARGET (a short target name, e.g. x86)
include Makefile.$(TARGET)

# This should define those:
#	CC (the c compiler)
#	LD (the linker)
include Makefile.$(TOOLCHAIN)

CFLAGS += -ffreestanding
CFLAGS += -Wall -Wextra -Werror
CFLAGS += -Wno-unused-parameter
CFLAGS += -Wno-pointer-sign
CFLAGS += -funsigned-char
CFLAGS += -O2

CPPFLAGS += -D __KERNEL__
CPPFLAGS += -D __kernel__
CPPFLAGS += -I include

LDFLAGS += -static
LDFLAGS += -nostdlib

%.c.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

%.S.o: %.S
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

%.s.o: %.s
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

# The build system will look
# for source files in these directories
SEARCH :=
SEARCH += src/lib/bitmap
SEARCH += src/lib/ctype
SEARCH += src/lib/printf
SEARCH += src/lib/stdlib
SEARCH += src/lib/string
SEARCH += src/lib/strings
SEARCH += src/lib/strtoi
SEARCH += src/lib/wchar
SEARCH += src/sys
SEARCH += src/$(S_TARGET)

SOURCES += $(wildcard $(addsuffix /*.c,$(SEARCH)))
SOURCES += $(wildcard $(addsuffix /*.S,$(SEARCH)))
SOURCES += $(wildcard $(addsuffix /*.s,$(SEARCH)))
OBJECTS += $(addsuffix .o,$(SOURCES))

INITCALLS_C := $(GEN)/initcalls.c
INITCALLS_O := $(GEN)/initcalls.c.o
KBIN_NOINIT := $(GEN)/kan_noinit.o
VERSION_C := $(GEN)/version.c
KERNEL := kan.elf
LD_SCRIPT := $(GEN)/ldscript.ld
LDS_SCRIPT := scripts/$(S_TARGET)/ldscript.lds
GEN := gen/

SOURCES += $(VERSION_C)

DISTCLEAN += $(GEN)

CLEAN += $(OBJECTS)
CLEAN += $(INITCALLS_C)
CLEAN += $(INITCALLS_O)
CLEAN += $(KBIN_NOINIT)
CLEAN += $(KERNEL)
CLEAN += $(LD_SCRIPT)

-include boot/$(L_TARGET)/Makefile.boot
-include boot/$(S_TARGET)/Makefile.boot

PHONY_TARGETS :=
PHONY_TARGETS += force all
PHONY_TARGETS += kernel clean distclean
ALL_DEPS += kernel

force:

all: $(ALL_DEPS)

kernel: $(KERNEL)

clean:
	$(RM) -vrf $(CLEAN)

distclean:
	$(RM) -vrf $(DISTCLEAN)
	$(RM) -vrf $(CLEAN)

$(VERSION_C): force
	$(SH) scripts/gen.version.sh $(VERSION) > $@

$(KERNEL): $(GEN) $(INITCALLS_O) $(KBIN_NOINIT) $(SYM0_O) $(LD_SCRIPT)
	$(LD) $(LDFLAGS) -T $(LD_SCRIPT) -o $@ $(INITCALLS_O) $(KBIN_NOINIT) $(SYM0_O)

$(INITCALLS_C): $(GEN) $(KBIN_NOINIT) force
	$(SH) scripts/gen.initcalls.sh $(KBIN_NOINIT) > $@

$(LD_SCRIPT): $(GEN) $(LDS_SCRIPT)
	$(CC) $(CPPFLAGS) -E -xc -D __ASSEMBLER__ $(LDS_SCRIPT) | $(GREP) -v "^#" > $@ || true

$(KBIN_NOINIT): $(GEN) $(OBJECTS)
	$(LD) $(LDFLAGS) -r -o $@ $(OBJECTS)

$(GEN):
	$(MKDIR) -p $@
