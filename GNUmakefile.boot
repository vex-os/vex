BOOTABLE_IMAGE_0 := ${BUILD_DIR}/vexsys.boot.0.img	# Has partition table but not files
BOOTABLE_IMAGE_1 := ${BUILD_DIR}/vexsys.boot.1.img	# Has partition table and files but not the kernel binary
BOOTABLE_IMAGE_2 := ${BUILD_DIR}/vexsys.boot.img	# Has partition table, files, and the kernel binary
SFDISK_SCRIPT := ${BUILD_DIR}/vexsys.boot.sfdisk.txt

# UTEST_ELF is a userspace ET_EXEC binary (usr/utest/utest.c + crt0),
# built outside the kernel's own object/link pipeline since it needs a
# completely different ABI (flat userspace ELF, no -mcmodel=kernel,
# no -fPIE, no kernel CPPFLAGS). It exists purely so kmain.c has a real
# file to feed through load_binary() - see init/kmain.c.
UTEST_CRT0   := ${BUILD_DIR}/usr/entry/sysv.crt0.amd64.S.o
UTEST_OBJECT := ${BUILD_DIR}/usr/utest/utest.c.o
UTEST_ELF    := ${BUILD_DIR}/utest.elf

UTEST_FLAGS := -m64 -march=x86-64 -mno-red-zone -ffreestanding -fno-pic -fno-pie -nostdlib
UTEST_FLAGS += -I${CURDIR}/include

ifeq (${ENVT},clang)
UTEST_FLAGS += --target=x86_64-none-elf
endif

CLEAN_S += ${BOOTABLE_IMAGE_0}
CLEAN_S += ${BOOTABLE_IMAGE_1}
CLEAN_S += ${BOOTABLE_IMAGE_2}
CLEAN_S += ${SFDISK_SCRIPT}
CLEAN_S += ${UTEST_CRT0}
CLEAN_S += ${UTEST_OBJECT}
CLEAN_S += ${UTEST_ELF}

PHONIES += bootimg

bootimg: ${BOOTABLE_IMAGE_2}
	@true

${SFDISK_SCRIPT}: | ${BUILD_DIR}
	@printf "  GEN\t%s\n" $@
	@truncate -s 0 $@
	@printf "label: gpt\n" > $@
	@printf "start=2048,size=8192,type=C12A7328-F81F-11D2-BA4B-00A0C93EC93B,name=ESP\n" >> $@
	@printf "type=EBD0A0A2-B9E5-4433-87C0-68B6B72699C7,name=DATA\n" >> $@

${BOOTABLE_IMAGE_0}: ${SFDISK_SCRIPT}
	@printf "  IMAG1\t%s\n" $@
	@dd if=/dev/zero of=$@ bs=1M count=32
	@sfdisk --force $@ < $<
	@mkfs.fat -F16 --offset 2048 $@
	@command -v echfs-utils > /dev/null 2>&1 && echfs-utils -g -p 1 $@ quick-format 512 || true
	@mmd -i $@@@2048s ::/EFI
	@mmd -i $@@@2048s ::/EFI/BOOT

${BOOTABLE_IMAGE_1}: ${BOOTABLE_IMAGE_0} boot/uefi/BOOT${EFI_ARCH}.EFI boot/limine.conf
	@printf "  IMAG2\t%s\n" $@
	@cp ${BOOTABLE_IMAGE_0} $@
	@mcopy -i $@@@2048s boot/uefi/BOOT${EFI_ARCH}.EFI ::/EFI/BOOT/BOOT${EFI_ARCH}.EFI
	@mcopy -i $@@@2048s boot/limine.conf ::/limine.conf

${UTEST_CRT0}: usr/entry/sysv.crt0.amd64.S | ${BUILD_DIR}
	@printf "U AS\t%s\n" "$<"
	@test -d $(dir $@) || mkdir -p $(dir $@)
	@${CC} ${UTEST_FLAGS} -c -o $@ $<

${UTEST_OBJECT}: usr/utest/utest.c | ${BUILD_DIR}
	@printf "U CC\t%s\n" "$<"
	@test -d $(dir $@) || mkdir -p $(dir $@)
	@${CC} ${UTEST_FLAGS} -c -o $@ $<

${UTEST_ELF}: ${UTEST_CRT0} ${UTEST_OBJECT} usr/utest/utest.lds
	@printf "U LD\t%s\n" "$@"
	@${LD} -m elf_x86_64 -nostdlib -T usr/utest/utest.lds -o $@ ${UTEST_CRT0} ${UTEST_OBJECT}

${BOOTABLE_IMAGE_2}: ${BOOTABLE_IMAGE_1} ${KERNEL_BINARY} ${UTEST_ELF}
	@printf "  IMAG3\t%s\n" $@
	@cp ${BOOTABLE_IMAGE_1} $@
	@mcopy -i $@@@2048s ${KERNEL_BINARY} ::/vexsys.elf
	@mcopy -i $@@@2048s ${UTEST_ELF} ::/utest.elf
