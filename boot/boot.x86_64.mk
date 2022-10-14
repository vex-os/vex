## SPDX-License-Identifier: BSD-2-Clause
## Copyright (c), 2022, Kaneru Contributors

TARGET		:= $(TOP_DIR)/kaneru.iso
XORRISO		:= xorriso
LIMINE_DEPL := $(TOP_DIR)/tools/limine-deploy/limine-deploy

ALLS_DEPS	+= iso
CLEAN_LIST	+= $(TARGET)

BOOT_FILES	+= $(ARCH)/limine-cd-efi.bin
BOOT_FILES	+= $(ARCH)/limine-cd.bin
BOOT_FILES	+= $(ARCH)/limine.cfg
BOOT_FILES	+= $(ARCH)/limine.sys

.PHONY: iso

iso: $(TARGET)

$(TARGET): $(LIMINE_DEPL) boot frc
	@$(LOG) "generating $@"
	@$(XORRISO) -as mkisofs -b boot/limine-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot boot/limine-cd-efi.bin -efi-boot-part \
		--efi-boot-image --protective-msdos-label $(SYS_ROOT) -o $@
	@$(LIMINE_DEPL) $@
