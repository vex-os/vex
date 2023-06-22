## SPDX-License-Identifier: BSD-2-Clause
## Copyright (c) 2023, KanOS Contributors

KERNEL_IMG := boot.img
BIOS_DEPLOY := $(TEMP)/limine-deploy
PHONY_TARGETS += bootable
PHONY_TARGETS += update-boot
ALL_DEPS += bootable

CLEAN += $(KERNEL_IMG)
DISTCLEAN += $(BIOS_DEPLOY)

bootable: $(KERNEL_IMG)

update-boot:
	$(SHELL) boot/update.sh

$(KERNEL_IMG): $(KERNEL) | $(BIOS_DEPLOY)
	dd if=/dev/zero of=$@ bs=1048576 count=32
	parted $@ mklabel gpt
	parted $@ mkpart KAN fat32 2048s 100%
	parted $@ set 1 esp on
	mkfs.fat -F16 --offset 2048 $@
	mmd -i $@@@2048s ::/EFI
	mmd -i $@@@2048s ::/EFI/BOOT
	mcopy -i $@@@2048s boot/x86_64/BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI
	mcopy -i $@@@2048s boot/x86_64/limine-bios.sys ::/limine-bios.sys
	mcopy -i $@@@2048s boot/limine.cfg ::/limine.cfg
	mcopy -i $@@@2048s $(KERNEL) ::/kernel.$(LIMINE_ARCH).elf
	$(BIOS_DEPLOY) bios-install $@

$(BIOS_DEPLOY): boot/x86_64/limine.c | boot/x86_64/limine-bios-hdd.h
	cc -Wall -Wextra -Werror -o $@ $<
