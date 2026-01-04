PHONIES += bootimg
ALLDEPS += bootimg

BOOTIMG ?= ${output_dir}/vexsys.boot.img
BOOTIMG_SIZE_MB ?= 64

bootimg: ${BOOTIMG}

${BOOTIMG}:	${KERNEL} boot/uefi/BOOT${EFI_ARCH}.EFI boot/limine.conf boot/misc/splash.png
	@printf "  IMAG\t%s\n" "$@"
	@dd if=/dev/zero of=$@ bs=1M count=${BOOTIMG_SIZE_MB}
	@parted $@ mklabel gpt
	@parted $@ mkpart ESP fat16 2048s 50%
	@parted $@ mkpart BIOS fat16 50% 100%
	@parted $@ set 1 esp on
	@parted $@ set 2 bios_grub on
	@mkfs.fat -F16 --offset 2048 $@
	@mmd -i $@@@2048s ::/EFI
	@mmd -i $@@@2048s ::/EFI/BOOT
	@mcopy -i $@@@2048s boot/uefi/BOOT${EFI_ARCH}.EFI ::/EFI/BOOT/BOOT${EFI_ARCH}.EFI
	@mcopy -i $@@@2048s boot/misc/splash.png ::/splash.png
	@mcopy -i $@@@2048s boot/limine.conf ::/limine.conf
	@mcopy -i $@@@2048s ${KERNEL} ::/vexsys.elf
