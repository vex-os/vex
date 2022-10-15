## SPDX-License-Identifier: BSD-2-Clause
## Copyright (c), 2022, Kaneru Contributors

ALLS_DEPS	:=
CLEAN_LIST	:=
TOP_DIR		:= ..
include $(TOP_DIR)/build/tools.base.mk

SYS_ROOT	:= ./sysroot
BOOT_DIR	:= $(SYS_ROOT)/boot
SYS_BINARY	:= $(TOP_DIR)/kaneru.elf
BOOT_FILES	:=

include boot.$(ARCH).mk

.PHONY: all clear frc
.PHONY: boot

all: $(ALLS_DEPS)

clean:
	$(RM) $(CLEAN_LIST)
	$(RM) -r $(SYS_ROOT)

boot: $(BOOT_DIR)
	$(CP) $(SYS_BINARY) $(BOOT_DIR)
	$(CP) $(BOOT_FILES) $(BOOT_DIR)

$(BOOT_DIR): $(SYS_ROOT)
	$(MKDIR) $(BOOT_DIR)

$(SYS_ROOT):
	$(MKDIR) $(SYS_ROOT)
