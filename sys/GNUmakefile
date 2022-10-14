## SPDX-License-Identifier: BSD-2-Clause
## Copyright (c), 2022, Kaneru Contributors

CLEAN_LIST	:=
TOP_DIR		:= ..
LOG_PFX		:= sys
include $(TOP_DIR)/build/base.mk
include $(TOP_DIR)/build/target.mk

XXFLAGS		:=
XXFLAGS		+= -O2 -pipe
XXFLAGS		+= -Wall -Wextra -Werror
XXFLAGS		+= -ffreestanding
XXFLAGS		+= -std=gnu99

ASFLAGS		+= $(XXFLAGS)
CCFLAGS		+= $(XXFLAGS)
CPFLAGS		+= -D __KERNEL__
CPFLAGS		+= -D __kernel__
CPFLAGS		+= -I $(TOP_DIR)/include
LDFLAGS		+= -static -nostdlib

GLOB_PATHS	:=
GLOB_PATHS	+= kern
GLOB_PATHS	+= libkern
GLOB_PATHS	+= $(ARCH)

TARGET_BASE	:= kaneru_base.o
TARGET_LDS	:= sys.$(ARCH).ld
TARGET	:= $(TOP_DIR)/kaneru.elf

INITCALLS_C	:= initcalls.c
INITCALLS_O	:= initcalls.o
VERSION_H	:= $(TOP_DIR)/include/psys/version.h

SOURCES		:=
SOURCES		+= $(shell find $(GLOB_PATHS) -maxdepth 1 -name "*.c")
SOURCES		+= $(shell find $(GLOB_PATHS) -maxdepth 1 -name "*.S")

OBJECTS		:= $(SOURCES:=.o)

CLEAN_LIST	+= $(TARGET)
CLEAN_LIST	+= $(TARGET_BASE)
CLEAN_LIST	+= $(TARGET_LDS)
CLEAN_LIST	+= $(INITCALLS_C)
CLEAN_LIST	+= $(INITCALLS_O)
CLEAN_LIST	+= $(OBJECTS)

include sys.$(ARCH).mk

.PHONY: all clean frc

all: $(TARGET)

clean:
	@$(RMV) $(CLEAN_LIST)

frc:

$(TARGET): $(TARGET_BASE) $(TARGET_LDS) $(INITCALLS_O)
	@$(LOG) "linking $@"
	@$(LLD) $(LDFLAGS) -T $(TARGET_LDS) -o $@ $(TARGET_BASE) $(INITCALLS_O)
	@$(RM) $(INITCALLS_C) $(INITCALLS_O)

$(TARGET_BASE): $(OBJECTS)
	@$(LOG) "squashing $@"
	@$(LLD) $(LDFLAGS) -r -o $@ $(OBJECTS)

$(VERSION_H): frc
	@$(LOG) "generating $@"
	@$(shell ./gen.version.sh "$(VERSION)" "$(GIT_REV)" > $@)

$(INITCALLS_C): $(TARGET_BASE)
	@$(LOG) "generating $@"
	@$(shell ./gen.initcalls.sh "$(OBJDUMP)" "$(TARGET_BASE)" > $@)

$(SOURCES): $(VERSION_H)
