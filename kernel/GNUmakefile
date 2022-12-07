## SPDX-License-Identifier: BSD-2-Clause
## Copyright (c), 2022, Kaneru Contributors

CLEAN_LIST	:=
TOP_DIR		:= ..
include $(TOP_DIR)/build/tools.base.mk
include $(TOP_DIR)/build/tools.compilers.mk

XXFLAGS		:=
XXFLAGS		+= -ffreestanding --target=$(ARCH)-none-elf
XXFLAGS		+= -Wall -Wextra -Werror
XXFLAGS		+= -O2 -std=gnu99

ASFLAGS		+= $(XXFLAGS)
CCFLAGS		+= $(XXFLAGS)
CPFLAGS		+= -D __KERNEL__
CPFLAGS		+= -D __kernel__
CPFLAGS		+= -I $(TOP_DIR)/include
CPFLAGS		+= -I $(TOP_DIR)/public
CPFLAGS		+= -nostdlibinc
LDFLAGS		+= -static -nostdlib

GLOB_PATHS	:=
GLOB_PATHS	+= lib
GLOB_PATHS	+= sys
GLOB_PATHS	+= $(ARCH)

TARGET_LDS	:= sys.$(ARCH).ld
TARGET_NIC	:= kaneru_noinit.o
TARGET_NST	:= kaneru_nosym.elf
TARGET		:= $(TOP_DIR)/kaneru.elf

INITCALLS_C	:= initcalls.c
INITCALLS_O	:= initcalls.o
NULLSYM_C	:= symtab.stub.c
NULLSYM_O	:= symtab.stub.o
SYMTAB_C	:= symtab.c
SYMTAB_O	:= symtab.o
VERSION_H	:= $(TOP_DIR)/include/kaneru/version.h

SOURCES		:=
SOURCES		+= $(shell find $(GLOB_PATHS) -maxdepth 1 -name "*.c")
SOURCES		+= $(shell find $(GLOB_PATHS) -maxdepth 1 -name "*.S")

include sys.$(ARCH).mk

OBJECTS		:= $(SOURCES:=.o)

CLEAN_LIST	+= $(TARGET_LDS)
CLEAN_LIST	+= $(TARGET_NIC)
CLEAN_LIST	+= $(TARGET_NST)
CLEAN_LIST	+= $(TARGET)
CLEAN_LIST	+= $(INITCALLS_C)
CLEAN_LIST	+= $(INITCALLS_O)
CLEAN_LIST	+= $(NULLSYM_O)
CLEAN_LIST	+= $(SYMTAB_C)
CLEAN_LIST	+= $(SYMTAB_O)
CLEAN_LIST	+= $(OBJECTS)

.PHONY: all clean frc

all: $(TARGET)

clean:
	$(RM) $(CLEAN_LIST)

frc:

$(TARGET): $(TARGET_NST) $(TARGET_NIC) $(TARGET_LDS) $(INITCALLS_O) $(SYMTAB_O)
	$(LD) $(LDFLAGS) -T $(TARGET_LDS) -o $@ $(TARGET_NIC) $(INITCALLS_O) $(SYMTAB_O)

$(TARGET_NST): $(TARGET_NIC) $(TARGET_LDS) $(INITCALLS_O) $(NULLSYM_O)
	$(LD) $(LDFLAGS) -T $(TARGET_LDS) -o $@ $(TARGET_NIC) $(INITCALLS_O) $(NULLSYM_O)

$(TARGET_NIC): $(OBJECTS)
	$(LD) $(LDFLAGS) -r -o $@ $(OBJECTS)

$(VERSION_H): frc
	$(shell ./gen.version.sh "$(VERSION)" "$(GIT_REV)" > $@)

$(INITCALLS_C): $(TARGET_NIC) frc
	$(shell ./gen.initcalls.sh "$(OBJDUMP)" "$(TARGET_NIC)" > $@)

$(SYMTAB_C): $(TARGET_NST) frc
	$(shell ./gen.symtab.sh "$(OBJDUMP)" "$(TARGET_NST)" > $@)

$(SOURCES): $(VERSION_H)
