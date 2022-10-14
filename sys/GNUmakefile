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

ASFLAGS		+= $(XXFLAGS) $(KCFLAGS)
CCFLAGS		+= $(XXFLAGS) $(KCFLAGS)
CPFLAGS		+= -D __KERNEL__
CPFLAGS		+= -D __kernel__
CPFLAGS		+= -I $(TOP_DIR)/include
CPFLAGS		+= -I $(TOP_DIR)/public
LDFLAGS		+= -static -nostdlib

GLOB_PATHS	:=
GLOB_PATHS	+= kern
GLOB_PATHS	+= libk
GLOB_PATHS	+= $(ARCH)

TARGET_LDS	:= sys.$(ARCH).ld
TARGET_NIC	:= kaneru_noinit.o
TARGET	:= $(TOP_DIR)/kaneru.elf

INITCALLS_C	:= initcalls.c
INITCALLS_O	:= initcalls.o
VERSION_H	:= $(TOP_DIR)/public/kaneru/version.h

SOURCES		:=
SOURCES		+= $(shell find $(GLOB_PATHS) -maxdepth 1 -name "*.c")
SOURCES		+= $(shell find $(GLOB_PATHS) -maxdepth 1 -name "*.S")

OBJECTS		:= $(SOURCES:=.o)

CLEAN_LIST	+= $(TARGET_LDS)
CLEAN_LIST	+= $(TARGET_NIC)
CLEAN_LIST	+= $(TARGET)
CLEAN_LIST	+= $(INITCALLS_C)
CLEAN_LIST	+= $(INITCALLS_O)
CLEAN_LIST	+= $(OBJECTS)

include sys.$(ARCH).mk

.PHONY: all clean frc

all: $(TARGET)

clean:
	@$(RMV) $(CLEAN_LIST)

frc:

$(TARGET): $(TARGET_NIC) $(TARGET_LDS) $(INITCALLS_O)
	@$(LOG) "linking $@"
	@$(LLD) $(LDFLAGS) -T $(TARGET_LDS) -o $@ $(TARGET_NIC) $(INITCALLS_O)
	@$(RM) $(INITCALLS_C) $(INITCALLS_O)

$(TARGET_NIC): $(OBJECTS)
	@$(LOG) "squashing $@"
	@$(LLD) $(LDFLAGS) -r -o $@ $(OBJECTS)

$(VERSION_H): frc
	@$(LOG) "generating $@"
	@$(shell ./gen.version.sh "$(VERSION)" "$(GIT_REV)" > $@)

$(INITCALLS_C): $(TARGET_NIC)
	@$(LOG) "generating $@"
	@$(shell ./gen.initcalls.sh "$(OBJDUMP)" "$(TARGET_NIC)" > $@)

$(SOURCES): $(VERSION_H)
