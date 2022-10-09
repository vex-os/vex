## SPDX-License-Identifier: GPL-2.0
## Copyright (c), 2022, Kaneru Contributors
TOPDIR := ..
ECHO_PREFIX := libkern
include $(TOPDIR)/build/flags.default.mk
include $(TOPDIR)/build/flags.$(ARCH).mk
include $(TOPDIR)/build/rules.mk
include $(TOPDIR)/build/tools.mk

CLEAN_LIST :=
PHONY_TARGETS :=

ARFLAGS += -rcs
ASFLAGS += -pipe -ffreestanding -O2 -Wall -Wextra -Werror -pedantic
CCFLAGS += -pipe -ffreestanding -O2 -Wall -Wextra -Werror -pedantic -std=gnu99
CPFLAGS += -I$(TOPDIR)/include

GLOB_DIRS :=
GLOB_DIRS += .

SOURCES :=
SOURCES += $(shell find $(GLOB_DIRS) -maxdepth 1 -name "*.c")
SOURCES += $(shell find $(GLOB_DIRS) -maxdepth 1 -name "*.S")
SOURCES += $(shell find $(GLOB_DIRS) -maxdepth 1 -name "*.s")

OBJECTS := $(SOURCES:=.o)
LIBKERN_A := $(TOPDIR)/libkern.a

CLEAN_LIST += $(OBJECTS)
CLEAN_LIST += $(LIBKERN_A)

PHONY_TARGETS += frc
frc:

PHONY_TARGETS += all
all: libkern

PHONY_TARGETS += clean
clean:
	@$(RM) -v $(CLEAN_LIST)

PHONY_TARGETS += libkern
libkern: $(LIBKERN_A)

$(LIBKERN_A): $(OBJECTS)
	@$(PRINTF) "linking $@"
	@$(AR) $(ARFLAGS) $@ $(OBJECTS)

# Setup phony targets
.phony: $(PHONY_TARGETS)
