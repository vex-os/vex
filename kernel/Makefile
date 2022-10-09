## SPDX-License-Identifier: GPL-2.0
## Copyright (c), 2022, Kaneru Contributors
## Requires ARCH, DASH and VEND defined
TOPDIR := ..
ECHO_PREFIX := kernel
include $(TOPDIR)/build/flags.default.mk
include $(TOPDIR)/build/flags.$(ARCH).mk
include $(TOPDIR)/build/rules.mk
include $(TOPDIR)/build/tools.mk

V_MAJOR := 0
V_MINOR := 0
V_PATCH := 0
V_POSTF := dev.0
V_METAD := $(shell git rev-parse --short=8 HEAD 2> /dev/null || echo 0)

CLEAN_LIST :=
PHONY_TARGETS :=

ASFLAGS += -pipe -ffreestanding -O2 -Wall -Wextra -Werror -pedantic
CCFLAGS += -pipe -ffreestanding -O2 -Wall -Wextra -Werror -pedantic -std=gnu99
CPFLAGS += -I$(TOPDIR)/include
LDFLAGS += -nostdlib -static -static-libgcc -lgcc

GLOB_DIRS :=
GLOB_DIRS += .
GLOB_DIRS += $(ARCH)

SOURCES :=
SOURCES += $(shell find $(GLOB_DIRS) -maxdepth 1 -name "*.c")
SOURCES += $(shell find $(GLOB_DIRS) -maxdepth 1 -name "*.S")
SOURCES += $(shell find $(GLOB_DIRS) -maxdepth 1 -name "*.s")

-include $(ARCH)/Makefile

OBJECTS := $(SOURCES:=.o)
KERNEL_NOINIT := kaneru_noinit.elf
KERNEL_INITCALLS_S := kaneru_initcalls.S
KERNEL_INITCALLS_O := kaneru_initcalls.o
KERNEL_LDS := $(ARCH)/kaneru.ld
KERNEL_ELF := $(TOPDIR)/kaneru.elf
LIBKERN_A := $(TOPDIR)/libkern.a
VERSION_H := $(TOPDIR)/include/public/kaneru/version.h

CLEAN_LIST += $(OBJECTS)
CLEAN_LIST += $(KERNEL_NOINIT)
CLEAN_LIST += $(KERNEL_INITCALLS_O)
CLEAN_LIST += $(KERNEL_INITCALLS_S)
CLEAN_LIST += $(KERNEL_LDS)
CLEAN_LIST += $(KERNEL_ELF)

PHONY_TARGETS += frc
frc:

PHONY_TARGETS += all
all: kernel

PHONY_TARGETS += clean
clean:
	@$(RM) -v $(CLEAN_LIST)

PHONY_TARGETS += kernel
kernel: $(KERNEL_ELF)

$(KERNEL_ELF): $(KERNEL_NOINIT) $(KERNEL_INITCALLS_O) $(KERNEL_LDS)
	@$(PRINTF) "linking $@"
	@$(LD) $(LDFLAGS) -T $(KERNEL_LDS) -o $@ $(KERNEL_NOINIT) $(KERNEL_INITCALLS_O)
	@$(RM) -v $(KERNEL_INITCALLS_O) $(KERNEL_INITCALLS_S)

$(KERNEL_NOINIT): $(VERSION_H) $(OBJECTS) $(LIBKERN_A)
	@$(PRINTF) "linking $@"
	@$(LD) $(LDFLAGS) -r -o $@ $(OBJECTS) $(LIBKERN_A)

$(KERNEL_INITCALLS_S): frc
	@$(PRINTF) "generating $@"
	@$(shell $(TOPDIR)/scripts/mkinitcalls.sh $(OBJDUMP) $(KERNEL_NOINIT) > $@)

$(VERSION_H): frc
	@$(PRINTF) "generating $@"
	@$(shell $(TOPDIR)/scripts/mkversion.sh $(V_MAJOR) $(V_MINOR) $(V_PATCH) $(V_METAD) $(V_POSTF) > $@)

# Setup phony targets
.phony: $(PHONY_TARGETS)
