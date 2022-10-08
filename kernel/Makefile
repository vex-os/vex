## SPDX-License-Identifier: GPL-2.0-only
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
V_METAD := $(shell git rev-parse --short=16 HEAD 2> /dev/null || echo 0)

CLEAN_LIST :=
PHONY_TARGETS :=

ASFLAGS += -pipe -ffreestanding -O2 -Wall -Wextra -Werror -pedantic
CCFLAGS += -pipe -ffreestanding -O2 -Wall -Wextra -Werror -pedantic -std=gnu99
CPFLAGS += -I$(TOPDIR)/include
CPFLAGS += -D__DEMOS__
CPFLAGS += -D__demos__
LDFLAGS += -nostdlib -static -static-libgcc -lgcc

GLOB_DIRS :=
GLOB_DIRS += .
GLOB_DIRS += libk
GLOB_DIRS += $(ARCH)

SOURCES :=
SOURCES += $(shell find $(GLOB_DIRS) -maxdepth 1 -name "*.c")
SOURCES += $(shell find $(GLOB_DIRS) -maxdepth 1 -name "*.S")
SOURCES += $(shell find $(GLOB_DIRS) -maxdepth 1 -name "*.s")

-include $(ARCH)/Makefile

OBJECTS := $(SOURCES:=.o)
KERNEL_LDS := $(ARCH)/kaneru.ld
KERNEL_ELF := $(TOPDIR)/kaneru.elf
VERSION_H := $(TOPDIR)/include/public/kaneru/version.h

CLEAN_LIST += $(OBJECTS)
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

$(KERNEL_ELF): $(OBJECTS) $(KERNEL_LDS) $(VERSION_H)
	@$(PRINTF) "linking $@"
	@$(LD) $(LDFLAGS) -T $(KERNEL_LDS) -o $@ $(OBJECTS)

$(VERSION_H): frc
	@$(PRINTF) "generating $@"
	@$(shell $(TOPDIR)/scripts/mkver.sh $(V_MAJOR) $(V_MINOR) $(V_PATCH) $(V_METAD) $(V_POSTF) > $@)

# Setup phony targets
.phony: $(PHONY_TARGETS)
