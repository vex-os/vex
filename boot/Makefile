## SPDX-License-Identifier: GPL-2.0-only
## Copyright (c), 2022, Kaneru Contributors
## Requires ARCH, DASH and VEND defined
TOPDIR := ..
ECHO_PREFIX := boot
include $(TOPDIR)/build/flags.default.mk
include $(TOPDIR)/build/rules.mk
include $(TOPDIR)/build/tools.mk

ALL_DEPS :=
CLEAN_LIST :=
PHONY_TARGETS :=
IMAGE_ROOT := $(CURDIR)/root
BOOT_PATH := $(IMAGE_ROOT)/boot
BOOT_COPY := $(TOPDIR)/kaneru.elf

ARCH_PATH := $(CURDIR)/$(ARCH)
include $(ARCH_PATH)/Makefile

PHONY_TARGETS += frc
frc:

PHONY_TARGETS += all
all: $(ALL_DEPS)

PHONY_TARGETS += clean
clean:
	@$(RM) -v $(CLEAN_LIST)
	@$(RM) -rv $(IMAGE_ROOT)

PHONY_TARGETS += root
root:
	@$(PRINTF) "setting up image file structure"
	@$(MKDIR) $(IMAGE_ROOT)
	@$(MKDIR) $(BOOT_PATH)

PHONY_TARGETS += boot_files
boot_files: root frc
	@$(PRINTF) "copying files"
	@$(CP) $(BOOT_COPY) $(BOOT_PATH)

# Setup phony targets
.phony: $(PHONY_TARGETS)
