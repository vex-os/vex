## SPDX-License-Identifier: GPL-2.0
## Copyright (c), 2022, Kaneru Contributors

TREE_ROOT := ..
PRINTF_PREFIX := libk

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

GLOB_PATHS :=
GLOB_PATHS += .

## Blatant lies: it's not a library!
## AR-ing a library is accompanied with
## a fair amount of variable argument ABI
## fuckery which breaks EVERYTHING!
BINARY := $(TREE_ROOT)/libk.o

SOURCES :=
SOURCES += $(shell find $(GLOB_PATHS) -maxdepth 1 -name "*.c")
SOURCES += $(shell find $(GLOB_PATHS) -maxdepth 1 -name "*.S")
SOURCES += $(shell find $(GLOB_PATHS) -maxdepth 1 -name "*.s")

OBJECTS := $(SOURCES:=.o)

CLEAN_LIST += $(BINARY)
CLEAN_LIST += $(OBJECTS)

.PHONY: all clean

all: $(BINARY)

clean:
	@$(RM) -v $(CLEAN_LIST)

$(BINARY): $(OBJECTS)
	@$(PRINTF) "squashing $@"
	@$(LD) $(LDFLAGS) -r -o $@ $(OBJECTS)
