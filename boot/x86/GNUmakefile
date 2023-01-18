# SPDX-License-Identifier: BSD-2-Clause 
# Copyright (c), 2022, KanOS Contributors 
# This is not meant to be run separately, instead
# it is included by the tree root's GNUmakefile.

IMAGE	:= kan.iso

IFILES	:=
IFILES	+= temp/sysroot/boot/limine-cd-efi.bin
IFILES	+= temp/sysroot/boot/limine-cd.bin
IFILES	+= temp/sysroot/boot/limine.cfg
IFILES	+= temp/sysroot/boot/limine.sys

DCLEAN	+= temp/sysroot
DCLEAN	+= temp/limine-deploy

CLEAN	+= temp/sysroot/boot/kan.elf
CLEAN	+= $(IMAGE)

.PHONY: x86boot
x86boot: $(IMAGE)
	@printf "[boot] done!\n"

$(IMAGE): temp/limine-deploy temp/sysroot/boot/kan.elf $(IFILES)
	@printf "[boot] generating $@\n"
	@xorriso \
		-as mkisofs -b boot/limine-cd.bin -no-emul-boot -boot-load-size 4 \
		-boot-info-table --efi-boot boot/limine-cd-efi.bin -efi-boot-part \
		--efi-boot-image --protective-msdos-label -o $@ temp/sysroot
	@temp/limine-deploy $@

temp/limine-deploy: boot/x86/limine-deploy.c boot/x86/limine-hdd.h
	@mkdir -p temp
	@printf "[boot] building $@\n"
	@clang -std=c99 -Wall -Wextra -Werror -pedantic -o $@ boot/x86/limine-deploy.c

temp/sysroot/boot/kan.elf: $(KERNEL)
	@mkdir -p temp/sysroot/boot
	@printf "[boot] copying $<\n"
	@cp $< $@

temp/sysroot/boot/%: boot/x86/%
	@mkdir -p temp/sysroot/boot
	@printf "[boot] copying $<\n"
	@cp $< $@
