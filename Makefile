# SPDX-License-Identifier: BSD-2-Clause 
# Copyright (c), 2022, Kaneru Contributors 
TARGET	?= x86_64
VERSION	:= 0.0.0-dev.3

CXFLAGS	+= -ffreestanding --target=$(TARGET)-none-elf
CXFLAGS	+= -Wall -Wextra -Werror
CXFLAGS	+= -O3 -std=gnu99

CPFLAGS	+= -D __KERNEL__
CPFLAGS	+= -D __kernel__
CPFLAGS	+= -I include
CPFLAGS	+= -nostdlibinc

LDFLAGS	+= -static
LDFLAGS	+= -nostdlib

%.c.o %.o: %.c
	@printf "compiling $<\n"
	@clang $(CXFLAGS) $(CPFLAGS) -c -o $@ $<

%.S.o %.o: %.S
	@printf "assembling $<\n"
	@clang $(CXFLAGS) $(CPFLAGS) -c -o $@ $<

%.s.o %.o: %.s
	@printf "assembling $<\n"
	@clang $(CXFLAGS) $(CPFLAGS) -c -o $@ $<

SOURCES	:=
OBJECTS	:=
CLEAN	:=

include Makefile.$(TARGET)

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
INIT_O	:= temp/initcalls.o
SYM0_C	:= temp/sym0.c
SYM0_O	:= temp/sym0.o
SYM1_C	:= temp/sym1.c
SYM1_O	:= temp/sym1.o
KBIN_0	:= temp/kan_noinit.o
KBIN_1	:= temp/kan_nosym.elf
LINK	:= temp/link.ld
LINK_S	:= scripts/link.$(LTARGET).lds
KERNEL	:= kernel.elf

CLEAN	+= $(OBJECTS)
CLEAN	+= $(INIT_C)
CLEAN	+= $(INIT_O)
CLEAN	+= $(KBIN_0)
CLEAN	+= $(KBIN_1)
CLEAN	+= $(KERNEL)
CLEAN	+= $(LINK)

.PHONY: force_run
force_run:

.PHONY: all
all: $(KERNEL)

.PHONY: clean
clean:
	@LC_ALL=C rm -vf $(CLEAN)
	@LC_ALL=C rm -vfr temp

# These files depend on the auto-generated header
src/kan/version.c: $(VERHDR)

$(VERHDR): force_run
	@printf "generating $@\n"
	@sh scripts/gen.version.sh $(VERSION) > $@

$(KERNEL): $(INIT_O) $(KBIN_1) $(SYM1_O) $(LINK)
	@printf "linking $@\n"
	@ld.lld $(LDFLAGS) -T $(LINK) -o $@ $(INIT_O) $(KBIN_0) $(SYM1_O)

$(SYM1_C): $(KBIN_1) force_run
	@mkdir -p temp
	@printf "generating $@\n"
	@sh scripts/gen.symtab.sh $(KBIN_1) > $@

$(KBIN_1): $(INIT_O) $(KBIN_0) $(SYM0_O) $(LINK)
	@mkdir -p temp
	@printf "linking $@\n"
	@ld.lld $(LDFLAGS) -T $(LINK) -o $@ $(INIT_O) $(KBIN_0) $(SYM0_O) 

$(INIT_C): $(KBIN_0) force_run
	@mkdir -p temp
	@printf "generating $@\n"
	@sh scripts/gen.initcalls.sh $(KBIN_0) > $@

$(SYM0_C):
	@mkdir -p temp
	@printf "generating $@\n"
	@sh scripts/gen.symtab.sh > $@

$(LINK): $(LINK_S)
	@mkdir -p temp
	@printf "preprocessing $(LINK_S)\n"
	@clang $(CPFLAGS) -D __ASSEMBLER__ -E -xc $(LINK_S) | grep -v "^#" > $@ || true

$(KBIN_0): $(OBJECTS)
	@mkdir -p temp
	@printf "linking $@\n"
	@ld.lld $(LDFLAGS) -r -o $@ $(OBJECTS)
