# SPDX-License-Identifier: BSD-2-Clause 
# Copyright (c), 2022, KanOS Contributors 
TARGET	?= x86_64
VERSION	:= 0.0.0-dev.3

CXFLAGS	+= -ffreestanding --target=$(CTARGET)
CXFLAGS	+= -Wall -Wextra -Werror
CXFLAGS += -Wno-unused-parameter
CXFLAGS	+= -O3 -std=gnu99

CPFLAGS	+= -D __KERNEL__
CPFLAGS	+= -D __kernel__
CPFLAGS	+= -I include
CPFLAGS	+= -nostdlibinc

LDFLAGS	+= -static
LDFLAGS	+= -nostdlib

%.c.o: %.c
	@printf "[kernel] compiling $<\n"
	@clang $(CXFLAGS) $(CPFLAGS) -c -o $@ $<

%.S.o: %.S
	@printf "[kernel] assembling $<\n"
	@clang $(CXFLAGS) $(CPFLAGS) -c -o $@ $<

%.s.o: %.s
	@printf "[kernel] assembling $<\n"
	@clang $(CXFLAGS) $(CPFLAGS) -c -o $@ $<

SOURCES	:=
OBJECTS	:=
DCLEAN	:=
CLEAN	:=

include GNUmakefile.$(TARGET)

GLOBS	:=
GLOBS	+= src/kan
GLOBS	+= src/libk
GLOBS	+= src/$(STARGET)
SOURCES	+= $(shell find $(GLOBS) -maxdepth 1 -name "*.c")
SOURCES	+= $(shell find $(GLOBS) -maxdepth 1 -name "*.S")
SOURCES += $(shell find $(GLOBS) -maxdepth 1 -name "*.s")
OBJECTS += $(SOURCES:=.o)

VERHDR	:= include/conf/version.h
INIT_C	:= temp/initcalls.c
INIT_O	:= temp/initcalls.c.o
SYM0_C	:= temp/sym0.c
SYM0_O	:= temp/sym0.c.o
SYM1_C	:= temp/sym1.c
SYM1_O	:= temp/sym1.c.o
KBIN_0	:= temp/kan_noinit.o
KBIN_1	:= temp/kan_nosyms.elf
KERNEL	:= kan.elf
LINK	:= temp/ldscript.ld
LINK_S	:= scripts/$(STARGET)/ldscript.lds

DCLEAN	+= $(SYM0_C)
DCLEAN	+= $(SYM0_O)
DCLEAN	+= temp

CLEAN	+= $(OBJECTS)
CLEAN	+= $(INIT_C)
CLEAN	+= $(INIT_O)
CLEAN	+= $(SYM1_C)
CLEAN	+= $(SYM1_O)
CLEAN	+= $(KBIN_0)
CLEAN	+= $(KBIN_1)
CLEAN	+= $(KERNEL)
CLEAN	+= $(LINK)

-include boot/$(STARGET)/GNUmakefile
-include boot/$(LTARGET)/GNUmakefile

.PHONY: force_run
force_run:

.PHONY: all
all: kernel

.PHONY: kernel
kernel: $(KERNEL)
	@printf "[kernel] done!\n"

.PHONY: clean
clean:
	@LC_ALL=C rm -vrf $(CLEAN)

.PHONY: distclean
distclean:
	@LC_ALL=C rm -vrf $(DCLEAN)
	@LC_ALL=C rm -vrf $(CLEAN)

# These files depend on the auto-generated header
src/kan/version.c: $(VERHDR)

$(VERHDR): force_run
	@printf "[kernel] generating $@\n"
	@sh scripts/genversion.sh $(VERSION) > $@

$(KERNEL): $(INIT_O) $(KBIN_1) $(SYM1_O) $(LINK)
	@printf "[kernel] linking $@\n"
	@ld.lld $(LDFLAGS) -T $(LINK) -o $@ $(INIT_O) $(KBIN_0) $(SYM1_O)

$(SYM1_C): $(KBIN_1) force_run
	@mkdir -p temp
	@printf "[kernel] generating $@\n"
	@sh scripts/gensymtab.sh $(KBIN_1) > $@

$(KBIN_1): $(INIT_O) $(KBIN_0) $(SYM0_O) $(LINK)
	@mkdir -p temp
	@printf "[kernel] linking $@\n"
	@ld.lld $(LDFLAGS) -T $(LINK) -o $@ $(INIT_O) $(KBIN_0) $(SYM0_O) 

$(INIT_C): $(KBIN_0) force_run
	@mkdir -p temp
	@printf "[kernel] generating $@\n"
	@sh scripts/geninitcalls.sh $(KBIN_0) > $@

$(SYM0_C):
	@mkdir -p temp
	@printf "[kernel] generating $@\n"
	@sh scripts/gensymtab.sh > $@

$(LINK): $(LINK_S)
	@mkdir -p temp
	@printf "[kernel] preprocessing $(LINK_S)\n"
	@clang $(CPFLAGS) -D __ASSEMBLER__ -E -xc $(LINK_S) | grep -v "^#" > $@ || true

$(KBIN_0): $(OBJECTS)
	@mkdir -p temp
	@printf "[kernel] linking $@\n"
	@ld.lld $(LDFLAGS) -r -o $@ $(OBJECTS)
