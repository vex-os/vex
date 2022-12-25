# SPDX-License-Identifier: BSD-2-Clause 
# Copyright (c), 2022, Kaneru Contributors 
# This is not meant to be run separately, instead
# it is included by the tree root's Makefile.

IMAGE	:= kan.iso
DEPLOY	:= temp/limine-deploy

IFILES	:=
IFILES	+= root/boot/limine-cd-efi.bin
IFILES	+= root/boot/limine-cd.bin
IFILES	+= root/boot/limine.cfg
IFILES	+= root/boot/limine.sys

DCLEAN	+= root
DCLEAN	+= $(DEPLOY)

CLEAN	+= root/boot/kan.elf
CLEAN	+= $(IMAGE)

.PHONY: boot
boot: $(IMAGE)
	@printf "[boot] done!\n"

$(IMAGE): temp/limine-deploy root/boot/kan.elf $(IFILES)
	@printf "[boot] generating $@\n"
	@xorriso \
		-as mkisofs -b boot/limine-cd.bin -no-emul-boot -boot-load-size 4 \
		-boot-info-table --efi-boot boot/limine-cd-efi.bin -efi-boot-part \
		--efi-boot-image --protective-msdos-label -o $@ root
	@$(DEPLOY) $@

$(DEPLOY): boot/x86/limine-deploy.c boot/x86/limine-hdd.h
	@mkdir -p temp
	@printf "[boot] building $@\n"
	@clang -std=c99 -Wall -Wextra -Werror -pedantic -o $@ boot/x86/limine-deploy.c

root/boot/kan.elf: $(KERNEL)
	@mkdir -p root/boot
	@printf "[boot] copying $<\n"
	@cp $< $@

root/boot/%: boot/x86/%
	@mkdir -p root/boot
	@printf "[boot] copying $<\n"
	@cp $< $@
