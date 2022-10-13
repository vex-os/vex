## SPDX-License-Identifier: GPL-2.0
## Copyright (c), 2022, Kaneru Contributors

TREE_ROOT := ..
PRINTF_PREFIX := kern

include $(TREE_ROOT)/scripts/mk/common.mk
include $(TREE_ROOT)/scripts/mk/$(TARGET).mk

CLEAN_LIST :=

XXFLAGS :=
XXFLAGS += -O2 -pipe
XXFLAGS += -pedantic -Wall -Wextra -Werror
XXFLAGS += -ffreestanding -std=gnu99

ASFLAGS += $(XXFLAGS)
CCFLAGS += $(XXFLAGS)
CPFLAGS += -I$(TREE_ROOT)/include
CPFLAGS += -D__KERNEL__ -D__kernel__
LDFLAGS += -static -nostdlib -lgcc

GLOB_PATHS :=
GLOB_PATHS += kern
GLOB_PATHS += libk/ctype
GLOB_PATHS += libk/sprintf
GLOB_PATHS += libk/string
GLOB_PATHS += $(TARGET)

BINARY := $(TREE_ROOT)/kaneru.elf
BINARY_NOINIT := kaneru_noinit.o
LD_SCRIPT := $(TARGET)/$(TARGET).ld

INITCALL_LIST_C := initcall_list.c
INITCALL_LIST_O := initcall_list.o
VERSION_H := $(TREE_ROOT)/include/sys/version.h

SOURCES :=
SOURCES += $(shell find $(GLOB_PATHS) -maxdepth 1 -name "*.c")
SOURCES += $(shell find $(GLOB_PATHS) -maxdepth 1 -name "*.S")
SOURCES += $(shell find $(GLOB_PATHS) -maxdepth 1 -name "*.s")

OBJECTS := $(SOURCES:=.o)

CLEAN_LIST += $(BINARY)
CLEAN_LIST += $(BINARY_NOINIT)
CLEAN_LIST += $(LD_SCRIPT)

CLEAN_LIST += $(INITCALL_LIST_C)
CLEAN_LIST += $(INITCALL_LIST_O)

CLEAN_LIST += $(OBJECTS)

.PHONY: all clean frc

all: $(BINARY)

clean:
	@$(RM) -v $(CLEAN_LIST)

$(BINARY): $(BINARY_NOINIT) $(LD_SCRIPT) $(INITCALL_LIST_O)
	@$(PRINTF) "linking $@"
	@$(LD) $(LDFLAGS) -T $(LD_SCRIPT) -o $@ $(BINARY_NOINIT) $(INITCALL_LIST_O)
	@$(RM) $(INITCALL_LIST_C) $(INITCALL_LIST_O)

$(BINARY_NOINIT): $(VERSION_H) $(OBJECTS)
	@$(PRINTF) "squashing $@"
	@$(LD) $(LDFLAGS) -r -o $@ $(OBJECTS)

$(VERSION_H): frc
	@$(PRINTF) "generating $@"
	@$(shell $(TREE_ROOT)/scripts/version.sh "$(VERSION)" "$(GIT_REV)" > $@)

$(INITCALL_LIST_C): frc $(BINARY_NOINIT)
	@$(PRINTF) "generating $@"
	@$(shell $(TREE_ROOT)/scripts/initcall_list.sh "$(OD)" "$(BINARY_NOINIT)" > $@)
