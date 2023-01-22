# SPDX-License-Identifier: BSD-2-Clause 
# Copyright (c), 2022, KanOS Contributors

FILES	:=
FILES	+= boot/x86/limine-cd-efi.bin
FILES	+= boot/x86/limine-cd.bin
FILES	+= boot/x86/limine.cfg
FILES	+= boot/x86/limine.sys

ISO		:= kan.iso
SYSROOT	:= temp/sysroot
DEPLOY	:= temp/limine-deploy
ISOBIN	:= $(SYSROOT)/boot/kan.elf

DCLEAN	+= $(SYSROOT)
DCLEAN	+= $(DEPLOY)

CLEAN	+= $(ISO)

.PHONY: x86boot
x86boot: $(ISO)
	@printf "[boot] done!\n"
	@rm -f $(shell find $(SYSROOT) -type f)

$(ISO): $(SYSROOT) $(DEPLOY) force_run
	@printf "[boot] generating $@\n"
	@xorriso \
		-as mkisofs -b boot/limine-cd.bin -no-emul-boot -boot-load-size 4 \
		-boot-info-table --efi-boot boot/limine-cd-efi.bin -efi-boot-part \
		--efi-boot-image --protective-msdos-label -o $@ $(SYSROOT)
	@$(DEPLOY) $@

$(SYSROOT): $(FILES) $(ISOBIN)
	@printf "[boot] setting up sysroot\n"
	@mkdir -p $(SYSROOT)/boot
	@cp --verbose $(FILES) $(SYSROOT)/boot

$(ISOBIN): $(KERNEL)
	@printf "[boot] copying kernel\n"
	@mkdir -p $(SYSROOT)/boot
	@cp --verbose $(KERNEL) $(ISOBIN)

$(DEPLOY): boot/x86/limine-deploy.c boot/x86/limine-hdd.h
	@printf "[boot] building $@\n"
	@clang -std=c99 -Wall -Wextra -Werror -pedantic -o $@ boot/x86/limine-deploy.c
