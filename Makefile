# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c), 2023, KanOS Contributors
MACHINE ?= x86_64
TOOLCHAIN ?= clang
VERSION ?= 0.0.0-dev.5

CP ?= cp
LN ?= ln
RM ?= rm

GREP ?= grep
MKDIR ?= mkdir
TRUE ?= true

SOURCES :=
OBJECTS :=
CLEAN_0 :=
CLEAN_1 :=
ALL_DEPS :=

# Build directories
MACH_INC := include/machine
TEMP_DIR := temp

# Machine makefile provides compiler options
# that are common across toolchains and defines
# the following variables:
#	CLANG_TARGET - Clang target argument (eg. x86_64-none-elf)
#	GCC_TARGET - GNU CC target triplet (eg. x86_64-elf-unknown)
include build/machine.$(MACHINE).mk

# Toolchain makefile provides compiler options
# that are unique to the specific toolchain and
# re-defines the following variables:
#	CC - A C compiler (GNU CC compatible, can compile assembly)
#	LD - An object file linker
include build/toolchain.$(TOOLCHAIN).mk

# Kernel CFLAGS
CFLAGS += -ffreestanding
CFLAGS += -Wall -Wextra -Werror
CFLAGS += -Wno-unused-parameter
CFLAGS += -Wno-pointer-sign
CFLAGS += -funsigned-char
CFLAGS += -O2

# Kernel CPPFLAGS
CPPFLAGS += -D __kernel__
CPPFLAGS += -I include
CPPFLAGS += -I usr.include

# Kernel LDFLAGS
LDFLAGS += -static
LDFLAGS += -nostdlib

%.c.o: %.c | build_dirs
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
%.S.o: %.S | build_dirs
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
%.s.o: %.s | build_dirs
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

SEARCH :=
SEARCH += $(MACHINE)
SEARCH += lib/bitmap
SEARCH += lib/ctype
SEARCH += lib/printf
SEARCH += lib/stdlib
SEARCH += lib/string
SEARCH += lib/strings
SEARCH += lib/strtoi
SEARCH += lib/wchar
SEARCH += sys

# Generated objects/sources
VERSION_C := $(TEMP_DIR)/version.c
INITCALLS_C := $(TEMP_DIR)/initcalls.c
INITCALLS_O := $(TEMP_DIR)/initcalls.o
KERNEL_BINARY := $(TEMP_DIR)/kernel.elf
KERNEL_NOINIT := $(TEMP_DIR)/kernel_noinit.o
LDSCRIPT := $(TEMP_DIR)/ldscript.ld

# Gather sources
SOURCES += $(VERSION_C)
SOURCES += $(wildcard $(addsuffix /*.c,$(SEARCH)))
SOURCES += $(wildcard $(addsuffix /*.S,$(SEARCH)))
SOURCES += $(wildcard $(addsuffix /*.s,$(SEARCH)))

# Gather objects
OBJECTS += $(addsuffix .o,$(SOURCES))

# DISTCLEAN list
CLEAN_1 += $(MACH_INC)
CLEAN_1 += $(TEMP_DIR)

# CLEAN list
CLEAN_0 += $(OBJECTS)
CLEAN_0 += $(INITCALLS_C)
CLEAN_0 += $(INITCALLS_O)
CLEAN_0 += $(KERNEL_BINARY)
CLEAN_0 += $(KERNEL_NOINIT)
CLEAN_0 += $(LDSCRIPT)

PHONY_TARGETS :=
PHONY_TARGETS += all
PHONY_TARGETS += kernel clean distclean
PHONY_TARGETS += build_dirs force_run
ALL_DEPS += kernel

-include boot/$(MACHINE)/Makefile

all: $(ALL_DEPS)

kernel: $(KERNEL_BINARY)

clean:
	$(RM) -vrf $(CLEAN_0)

distclean:
	$(RM) -vrf $(CLEAN_0)
	$(RM) -vrf $(CLEAN_1)

build_dirs:
	$(RM) -vrf $(MACH_INC)
	$(LN) -sf $(abspath include/$(MACHINE)) $(abspath $(MACH_INC))
	$(MKDIR) -p $(TEMP_DIR)

force_run:

$(VERSION_C): build_dirs
	$(SHELL) build/gen.version.sh $(VERSION) $(MACHINE) > $@

$(INITCALLS_C): $(KERNEL_NOINIT) | build_dirs
	$(SHELL) build/gen.initcalls.sh $^ > $@

$(KERNEL_BINARY): $(INITCALLS_O) $(KERNEL_NOINIT) | $(LDSCRIPT) build_dirs
	$(LD) $(LDFLAGS) -T $(LDSCRIPT) -o $@ $^

$(KERNEL_NOINIT): $(OBJECTS) | build_dirs
	$(LD) $(LDFLAGS) -r -o $@ $^

$(LDSCRIPT): build/ldscript.$(MACHINE).lds | build_dirs
	$(CC) $(CPPFLAGS) -E -xc -D __ASSEMBLER__ $^ | $(GREP) -v "^#" > $@ || $(TRUE)
