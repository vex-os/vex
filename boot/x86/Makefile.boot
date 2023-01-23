# SPDX-License-Identifier: BSD-2-Clause 
# Copyright (c), 2022, KanOS Contributors
BOOT_XORRISO ?= xorriso
BOOT_C99 ?= c99
BOOT_IMAGE ?= kan.iso
BOOT_ROOT := temp/x86boot
BOOT_DEPLOY := temp/limine-deploy
BOOT_KERNEL := $(BOOT_ROOT)/boot/kan.elf

# These are copied to /boot/
BOOT_BFILES :=
BOOT_BFILES += boot/x86/limine-cd-efi.bin
BOOT_BFILES += boot/x86/limine-cd.bin
BOOT_BFILES += boot/x86/limine.cfg
BOOT_BFILES += boot/x86/limine.sys

DISTCLEAN += $(BOOT_ROOT)
DISTCLEAN += $(BOOT_DEPLOY)

CLEAN += $(BOOT_IMAGE)

PHONY_TARGETS += x86boot
ALL_DEPS += x86boot

x86boot: $(BOOT_IMAGE)

$(BOOT_IMAGE): $(BOOT_ROOT) $(BOOT_KERNEL) $(BOOT_DEPLOY) force
	$(BOOT_XORRISO) -as mkisofs -b boot/limine-cd.bin -no-emul-boot \
		-boot-load-size 4 -boot-info-table --efi-boot boot/limine-cd-efi.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label -o $@ $(BOOT_ROOT)
	$(BOOT_DEPLOY) $@

$(BOOT_ROOT): $(TEMP) $(BOOT_BFILES)
	$(MKDIR) -p $(BOOT_ROOT)
	$(MKDIR) -p $(BOOT_ROOT)/boot
	$(CP) $(BOOT_BFILES) $(BOOT_ROOT)/boot

$(BOOT_KERNEL): $(BOOT_ROOT) $(KERNEL)
	$(CP) $(KERNEL) $(BOOT_KERNEL)

$(BOOT_DEPLOY): $(TEMP) boot/x86/limine-deploy.c boot/x86/limine-hdd.h
	$(BOOT_C99) -Wall -Wextra -Werror -pedantic -o $@ boot/x86/limine-deploy.c
