## SPDX-License-Identifier: BSD-2-Clause
## Copyright (c) 2023, KanOS Contributors

MACHINE ?= x86_64
TOOLCHAIN ?= llvm
VERSION ?= 0.0.0-dev.7

CP ?= cp
LN ?= ln
RM ?= rm

GREP ?= grep
MKDIR ?= mkdir
TRUE ?= true

SOURCES :=
OBJECTS :=
CLEANS :=
DISTCLEANS :=
ALL_DEPS :=

TEMP_DIR := temp

# Machine makefile provides compiler options that
# are common across toolchains and defines the following:
#	GCC_PREFIX	- GNU toolchain target prefix (eg. x86_64-elf-unknown)
#	LLVM_TARGET	- LLVM toolchain target (eg. x86-64-none-elf)
include config/machine.$(MACHINE).mk

# Toolchain makefile provides compiler options that differ
# from toolchain to toolchain and [re-]defines the following:
#	CC	- An ISO C99 compiler with GNU C extensions support
#	LD	- An object file linker
include config/toolchain.$(TOOLCHAIN).mk

CFLAGS += -ffreestanding
CFLAGS += -Wall -Wextra -Werror
CFLAGS += -Wno-unused-parameter
CFLAGS += -Wno-pointer-sign
CFLAGS += -funsigned-char
CFLAGS += -O2

CPPFLAGS += -D __kernel__
CPPFLAGS += -I include
CPPFLAGS += -I include.$(MACHINE)
CPPFLAGS += -I usr.include

LDFLAGS += -static
LDFLAGS += -nostdlib

%.c.o: %.c | build_dirs
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
%.S.o: %.S | build_dirs
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
%.s.o: %.s | build_dirs
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

SEARCH :=
SEARCH += blob
SEARCH += kern
SEARCH += kern.$(MACHINE)
SEARCH += libkern/ctype
SEARCH += libkern/inttypes
SEARCH += libkern/printf
SEARCH += libkern/stdlib
SEARCH += libkern/string
SEARCH += libkern/strings

VERSION_C := $(TEMP_DIR)/version.c
INITCALLS_C := $(TEMP_DIR)/initcalls.c
INITCALLS_O := $(TEMP_DIR)/initcalls.o
KERNEL_BINARY := $(TEMP_DIR)/kernel.elf
KERNEL_NOINIT := $(TEMP_DIR)/kernel_noinit.o
LDSCRIPT := $(TEMP_DIR)/ldscript.ld

SOURCES += $(VERSION_C)
SOURCES += $(wildcard $(addsuffix /*.c,$(SEARCH)))
SOURCES += $(wildcard $(addsuffix /*.S,$(SEARCH)))
SOURCES += $(wildcard $(addsuffix /*.s,$(SEARCH)))
OBJECTS += $(addsuffix .o,$(SOURCES))

CLEANS += $(OBJECTS)
CLEANS += $(INITCALLS_C)
CLEANS += $(INITCALLS_O)
CLEANS += $(KERNEL_BINARY)
CLEANS += $(KERNEL_NOINIT)
CLEANS += $(LDSCRIPT)
DISTCLEANS += $(TEMP_DIR)

PHONY_TARGETS :=
PHONY_TARGETS += all
PHONY_TARGETS += kernel clean distclean
PHONY_TARGETS += force_run build_dirs
ALL_DEPS += kernel

# The kernel is designed to be a package
# in the future system (hopefully), but to test
# things out and boot the thing this boot crutch exists
-include boot.$(MACHINE)/GNUmakefile

all: $(ALL_DEPS)

kernel: $(KERNEL_BINARY)

clean:
	$(RM) -vrf $(CLEANS)

distclean:
	$(RM) -vrf $(CLEANS)
	$(RM) -vrf $(DISTCLEANS)

$(TEMP_DIR):
	$(MKDIR) -p $(TEMP_DIR)

force_run:

build_dirs: $(TEMP_DIR)

$(VERSION_C): $(TEMP_DIR)
	$(SHELL) scripts/gen_version.sh $(VERSION) $(MACHINE) > $@

$(INITCALLS_C): $(KERNEL_NOINIT) | build_dirs
	$(SHELL) scripts/gen_initcalls.sh $^ > $@

$(KERNEL_BINARY): $(INITCALLS_O) $(KERNEL_NOINIT) | $(LDSCRIPT) build_dirs
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -o $@ $^

$(KERNEL_NOINIT): $(OBJECTS) | build_dirs
	$(LD) $(LDFLAGS) -r -o $@ $^

$(LDSCRIPT): config/ldscript.$(MACHINE).lds | build_dirs
	$(CC) $(CPPFLAGS) -E -xc -D __ASSEMBLER__ $^ | $(GREP) -v "^#" > $@ || $(TRUE)
