## SPDX-License-Identifier: GPL-2.0
## Copyright (c), 2022, Kaneru Contributors

BOOT_COPY += $(ARCH_PATH)/limine-cd-efi.bin
BOOT_COPY += $(ARCH_PATH)/limine-cd.bin
BOOT_COPY += $(ARCH_PATH)/limine.cfg
BOOT_COPY += $(ARCH_PATH)/limine.sys

XORRISO := xorriso
LIMINE_DEPLOY := $(TOPDIR)/tools/limine-deploy/limine-deploy
KERNEL_BOOTIMG := $(TOPDIR)/kaneru.iso

CLEAN_LIST += $(KERNEL_BOOTIMG)

ALL_DEPS += bootimg
PHONY_TARGETS += bootimg
bootimg: $(KERNEL_BOOTIMG)

$(KERNEL_BOOTIMG): boot_files $(LIMINE_DEPLOY)
	@$(PRINTF) "generating $@"
	@$(XORRISO) -as mkisofs -b boot/limine-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot boot/limine-cd-efi.bin -efi-boot-part \
		--efi-boot-image --protective-msdos-label ./root -o $@
	@$(LIMINE_DEPLOY) $@
