## SPDX-License-Identifier: GPL-2.0
## Copyright (c), 2022, Kaneru Contributors

XORRISO := xorriso
LIMINE_DEPLOY := $(TREE_ROOT)/tools/limine-deploy/limine-deploy

KERNEL_ISO := $(TREE_ROOT)/kaneru.iso

ALLS_DEPS += img_image
CLEAN_LIST += $(KERNEL_ISO)

DIR_BOOT_FILES += $(TARGET)/limine-cd-efi.bin
DIR_BOOT_FILES += $(TARGET)/limine-cd.bin
DIR_BOOT_FILES += $(TARGET)/limine.cfg
DIR_BOOT_FILES += $(TARGET)/limine.sys

.PHONY: img_image

img_image: $(KERNEL_ISO)

$(KERNEL_ISO): $(LIMINE_DEPLOY) img_files frc
	@$(PRINTF) "generating $@"
	@$(XORRISO) -as mkisofs -b boot/limine-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot boot/limine-cd-efi.bin -efi-boot-part \
		--efi-boot-image --protective-msdos-label ./root -o $@
	@$(LIMINE_DEPLOY) $@
