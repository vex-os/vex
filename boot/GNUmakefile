## SPDX-License-Identifier: GPL-2.0
## Copyright (c), 2022, Kaneru Contributors
## Requires ARCH, DASH and VEND defined

TREE_ROOT := ..
PRINTF_PREFIX := boot

include $(TREE_ROOT)/scripts/mk/common.mk
include $(TREE_ROOT)/scripts/mk/$(TARGET).mk

ALLS_DEPS :=
CLEAN_LIST :=

KERNEL_BINARY := $(TREE_ROOT)/kaneru.elf

IMG_ROOT := root
DIR_BOOT := $(IMG_ROOT)/boot
DIR_BOOT_FILES := $(KERNEL_BINARY)

include $(TARGET)/GNUmakefile

.PHONY: all clear frc
.PHONY: img_root img_files

all: $(ALLS_DEPS)

clean:
	@$(RM) -v $(CLEAN_LIST)
	@$(RM) -v -r $(IMG_ROOT)

img_root:
	@$(PRINTF) "creating directory hierarchy"
	@$(MKDIR) $(IMG_ROOT)
	@$(MKDIR) $(DIR_BOOT)

img_files: $(DIR_BOOT_FILES) img_root
	@$(PRINTF) "copying files"
	@$(CP) $(DIR_BOOT_FILES) $(DIR_BOOT)
