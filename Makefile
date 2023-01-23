# SPDX-License-Identifier: BSD-2-Clause 
# Copyright (c), 2022, KanOS Contributors 
TARGET := x86_64
VERSION := 0.0.0-dev.4

CP ?= cp
GREP ?= grep
MKDIR ?= mkdir
SH ?= sh
RM ?= rm

BLOBS :=
SOURCES :=
OBJECTS :=
CLEAN :=
DISTCLEAN :=
ALL_DEPS :=

# This should define these:
#	CC (the C compiler)
#	LD (the linker)
include conf/tools.mk

# This should define these:
#	C_TARGET (--target argument for clang)
#	L_TARGET (full target name, e.g. x86_64)
#	S_TARGET (a short target name, e.g. x86)
include Makefile.$(TARGET)

ifeq ($(findstring clang,$(CC)),clang)
include Makefile.clang
endif

# UNDONE: implement me!
ifeq ($(findstring gcc,$(CC)),gcc)
include Makefile.gcc
endif

CFLAGS += -ffreestanding
CFLAGS += -Wall -Wextra -Werror
CFLAGS += -Wno-unused-parameter
CFLAGS += -Wno-pointer-sign
CFLAGS += -funsigned-char
CFLAGS += -O2

CPPFLAGS += -D __KERNEL__
CPPFLAGS += -D __kernel__
CPPFLAGS += -I include
CPPFLAGS += -nostdlibinc

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
SEARCH += src/kan
SEARCH += src/libk
SEARCH += src/$(S_TARGET)

# A blob is a binary object that
# is embedded to the kernel binary.
# TODO: add blobs to the list here.
BLOBS += kfont

SOURCES += $(wildcard $(addsuffix /*.c,$(SEARCH)))
SOURCES += $(wildcard $(addsuffix /*.S,$(SEARCH)))
SOURCES += $(wildcard $(addsuffix /*.s,$(SEARCH)))
SOURCES += $(foreach blob,$(BLOBS),src/blob/$(blob).S)
OBJECTS += $(addsuffix .o,$(SOURCES))

VERSION_H := include/conf/version.h
INITCALLS_C := temp/initcalls.c
INITCALLS_O := temp/initcalls.c.o
SYM0_C := temp/sym0.c
SYM0_O := temp/sym0.c.o
SYM1_C := temp/sym1.c
SYM1_O := temp/sym1.c.o
KBIN_NOINIT := temp/kan_noinit.o
KBIN_NOSYMS := temp/kan_nosyms.elf
KERNEL := kan.elf
LD_SCRIPT := temp/ldscript.ld
LDS_SCRIPT := scripts/$(S_TARGET)/ldscript.lds
TEMP := temp/

DISTCLEAN += $(SYM0_C)
DISTCLEAN += $(SYM0_O)
DISTCLEAN += $(TEMP)

CLEAN += $(OBJECTS)
CLEAN += $(INITCALLS_C)
CLEAN += $(INITCALLS_O)
CLEAN += $(SYM1_C)
CLEAN += $(SYM1_O)
CLEAN += $(KBIN_NOINIT)
CLEAN += $(KBIN_NOSYMS)
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

# These files depend on the auto-generated header.
# TODO: add here more files that depend on it!
src/kan/version.c: $(VERSION_H)

$(VERSION_H): force
	$(SH) scripts/gen_version.sh $(VERSION) > $@

$(KERNEL): $(INITCALLS_O) $(KBIN_NOSYMS) $(KBIN_NOINIT) $(SYM1_O) $(LD_SCRIPT)
	$(LD) $(LDFLAGS) -T $(LD_SCRIPT) -o $@ $(INITCALLS_O) $(KBIN_NOINIT) $(SYM1_O)

$(SYM1_C): $(TEMP) $(KBIN_NOSYMS) force
	$(SH) scripts/gen_symtab.sh $(KBIN_NOINIT) > $@

$(KBIN_NOSYMS): $(TEMP) $(INITCALLS_O) $(KBIN_NOINIT) $(SYM0_O) $(LD_SCRIPT)
	$(LD) $(LDFLAGS) -T $(LD_SCRIPT) -o $@ $(INITCALLS_O) $(KBIN_NOINIT) $(SYM0_O)

$(INITCALLS_C): $(TEMP) $(KBIN_NOINIT) force
	$(SH) scripts/gen_initcalls.sh $(KBIN_NOINIT) > $@

$(SYM0_C): $(TEMP)
	$(SH) scripts/gen_symtab.sh > $@

$(LD_SCRIPT): $(TEMP) $(LDS_SCRIPT)
	$(CC) $(CPPFLAGS) -E -xc -D __ASSEMBLER__ $(LDS_SCRIPT) | $(GREP) -v "^#" > $@ || true

$(KBIN_NOINIT): $(TEMP) $(OBJECTS)
	$(LD) $(LDFLAGS) -r -o $@ $(OBJECTS)

$(TEMP):
	$(MKDIR) -p $@
