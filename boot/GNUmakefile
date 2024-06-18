## SPDX-License-Identifier: Zlib

KBOOT := ${build_dir}/kernel.boot.img
CLEAN0 += ${KBOOT}

PHONYS += bootable
ALLDEP += bootable

bootable: ${KBOOT}

${KBOOT}: ${KERNEL}
	dd if=/dev/zero of=$@ bs=1048576 count=32
	parted $@ mklabel gpt
	parted $@ mkpart ESP fat16 2048s 100%
	parted $@ set 1 esp on
	mkfs.fat -F16 --offset 2048 $@
	mmd -i $@@@2048s ::/EFI
	mmd -i $@@@2048s ::/EFI/BOOT
	mcopy -i $@@@2048s arch/${ARCH}/boot/BOOT${UEFI_ARCH}.EFI ::/EFI/BOOT/BOOT${UEFI_ARCH}.EFI
	mcopy -i $@@@2048s boot/limine.cfg ::/limine.cfg
	mcopy -i $@@@2048s ${KERNEL} ::/kernel.${LIMINE_ARCH}.elf
