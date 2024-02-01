## SPDX-License-Identifier: GPL-2.0-only
## Copyright (c) 2024, VX/sys Contributors

KERNEL_IMG := kernel.boot.img
PHONY_TARGETS += bootable
PHONY_TARGETS += update-boot
ALL_DEPS += bootable

CLEAN += $(KERNEL_IMG)

bootable: $(KERNEL_IMG)

$(KERNEL_IMG): $(KERNEL)
	dd if=/dev/zero of=$@ bs=1048576 count=32
	parted $@ mklabel gpt
	parted $@ mkpart ESP fat16 2048s 100%
	parted $@ set 1 esp on
	mkfs.fat -F16 --offset 2048 $@
	mmd -i $@@@2048s ::/EFI
	mmd -i $@@@2048s ::/EFI/BOOT
	mcopy -i $@@@2048s boot/$(MACHINE)/BOOT$(EFI_ARCH).EFI ::/EFI/BOOT/BOOT$(EFI_ARCH).EFI
	mcopy -i $@@@2048s boot/limine.cfg ::/limine.cfg
	mcopy -i $@@@2048s $(KERNEL) ::/kernel.$(LIMINE_ARCH).elf
