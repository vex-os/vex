# SPDX-License-Identifier: BSD-2-Clause 
# Copyright (c), 2022, KanOS Contributors
BOOT_C99 ?= c99
BOOT_DEPLOY ?= $(TEMP_DIR)/limine-deploy
BOOT_XORRISO ?= xorriso

BOOT_DIR := $(TEMP_DIR)/sysroot
BOOT_IMAGE := $(TEMP_DIR)/kernel.iso

BOOT_FILES :=
BOOT_FILES += boot/x86_64/limine-cd-efi.bin
BOOT_FILES += boot/x86_64/limine-cd.bin
BOOT_FILES += boot/x86_64/limine.cfg
BOOT_FILES += boot/x86_64/limine.sys
BOOT_FILES += $(KERNEL_BINARY)

CLEAN_0 += $(BOOT_IMAGE)
CLEAN_1 += $(BOOT_ROOT)
CLEAN_1 += $(BOOT_DEPLOY)

PHONY_TARGETS += boot_dirs
PHONY_TARGETS += boot_image
ALL_DEPS += boot_image

boot_dirs: $(BOOT_FILES) | $(TEMP_DIR)
	$(MKDIR) -p $(BOOT_DIR)
	$(MKDIR) -p $(BOOT_DIR)/boot
	$(CP) $^ $(BOOT_DIR)/boot

boot_image: $(BOOT_IMAGE)

$(BOOT_DEPLOY): boot/x86_64/limine-deploy.c | boot/x86_64/limine-hdd.h $(TEMP_DIR)
	$(BOOT_C99) -Wall -Wextra -Werror -pedantic -o $@ boot/x86_64/limine-deploy.c

$(BOOT_IMAGE): $(BOOT_DEPLOY) boot_dirs force_run
	$(BOOT_XORRISO) -as mkisofs -b boot/limine-cd.bin -no-emul-boot \
		-boot-load-size 4 -boot-info-table --efi-boot boot/limine-cd-efi.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label -o $@ $(BOOT_DIR)
	$(BOOT_DEPLOY) $@
