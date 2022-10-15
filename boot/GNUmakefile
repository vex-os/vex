## SPDX-License-Identifier: BSD-2-Clause
## Copyright (c), 2022, Kaneru Contributors

ALLS_DEPS	:=
CLEAN_LIST	:=
TOP_DIR		:= ..
LOG_PFX		:= boot
include $(TOP_DIR)/build/base.mk

SYS_ROOT	:= ./sysroot
BOOT_DIR	:= $(SYS_ROOT)/boot
SYS_BINARY	:= $(TOP_DIR)/kaneru.elf
BOOT_FILES	:=

include boot.$(ARCH).mk

.PHONY: all clear frc
.PHONY: boot

all: $(ALLS_DEPS)

clean:
	@$(RMV) $(CLEAN_LIST)
	@$(RMV) -r $(SYS_ROOT)

boot: $(BOOT_DIR)
	@$(LOG) "copying files"
	@$(CP) $(SYS_BINARY) $(BOOT_DIR)
	@$(CP) $(BOOT_FILES) $(BOOT_DIR)

$(BOOT_DIR): $(SYS_ROOT)
	@$(LOG) "creating /boot/"
	@$(MKDIR) $(BOOT_DIR)

$(SYS_ROOT):
	@$(LOG) "creating /"
	@$(MKDIR) $(SYS_ROOT)
