export LC_ALL=C.UTF-8
export LANGUAGE=C.UTF-8
export LANG=C.UTF-8

ARCH ?= amd64

INTERM_DIR ?= build
OUTPUT_DIR ?= output

IMAGE_0 := ${INTERM_DIR}/vexsys.boot.0.img	# Has partition table but not files
IMAGE_1 := ${INTERM_DIR}/vexsys.boot.1.img	# Has partition table and files but not the kernel binary
IMAGE_2 := ${OUTPUT_DIR}/vexsys.boot.img	# Has partition table, files, and the kernel binary

SFDISK_SCRIPT := ${INTERM_DIR}/vexsys.boot.sfdisk.txt

override CC := clang
override LD := ld.lld

override CFLAGS := 
override CFLAGS += -Wall -Wextra -Werror
override CFLAGS += -Wno-sign-compare
override CFLAGS += -I${OUTPUT_DIR}/include
override CFLAGS += -mno-red-zone -ffreestanding -fno-pic -fno-pie -nostdlib

ifeq (${ARCH},amd64)
EFI_ARCH := X64
override CFLAGS += --target=x86_64-none-elf
override CFLAGS += -m64 -march=x86-64 
endif

OBJ_CRT0 := ${INTERM_DIR}/crt0.S.o
OBJ_UTEST := ${INTERM_DIR}/utest.c.o
UTEST_ELF := ${INTERM_DIR}/utest.elf

KERNEL_BINARY := ${OUTPUT_DIR}/vexsys.elf

PHONIES :=
PHONIES += all
PHONIES += clean

.PHONY: all clean distclean bootimg

all: ${IMAGE_2}
	@true

clean:
	@rm -vrf ${INTERM_DIR}
	@rm -vf ${IMAGE_2}

${IMAGE_2}: ${IMAGE_1} ${KERNEL_BINARY} ${UTEST_ELF}
	@printf "  IMAG3\t%s\n" $@
	@cp ${IMAGE_1} $@
	@mcopy -i $@@@2048s ${KERNEL_BINARY} ::/vexsys.elf
	@mcopy -i $@@@2048s ${UTEST_ELF} ::/utest.elf

${IMAGE_1}: ${IMAGE_0} limine/BOOT${EFI_ARCH}.EFI limine.conf
	@printf "  IMAG2\t%s\n" $@
	@cp ${IMAGE_0} $@
	@mcopy -i $@@@2048s limine/BOOT${EFI_ARCH}.EFI ::/EFI/BOOT/BOOT${EFI_ARCH}.EFI
	@mcopy -i $@@@2048s limine.conf ::/limine.conf

${IMAGE_0}: ${SFDISK_SCRIPT}
	@printf "  IMAG1\t%s\n" $@
	@dd if=/dev/zero of=$@ bs=1M count=32
	@sfdisk --force $@ < $<
	@mkfs.fat -F16 --offset 2048 $@
	@command -v echfs-utils > /dev/null 2>&1 && echfs-utils -g -p 1 $@ quick-format 512 || true
	@mmd -i $@@@2048s ::/EFI
	@mmd -i $@@@2048s ::/EFI/BOOT

${SFDISK_SCRIPT}:
	@printf "  GEN\t%s\n" $@
	@truncate -s 0 $@
	@printf "label: gpt\n" > $@
	@printf "start=2048,size=8192,type=C12A7328-F81F-11D2-BA4B-00A0C93EC93B,name=ESP\n" >> $@
	@printf "type=EBD0A0A2-B9E5-4433-87C0-68B6B72699C7,name=DATA\n" >> $@

${UTEST_ELF}: ${OBJ_CRT0} ${OBJ_UTEST} utest/link.ld
	@printf "U LD\t%s\n" "$@"
	@${LD} -m elf_x86_64 -nostdlib -T utest/link.ld -o $@ ${OBJ_CRT0} ${OBJ_UTEST}

${OBJ_CRT0}: utest/crt0.S
	@printf "U AS\t%s\n" "$<"
	@test -d $(dir $@) || mkdir -p $(dir $@)
	@${CC} ${CFLAGS} -c -o $@ $<

${OBJ_UTEST}: utest/utest.c | ${OUTPUT_DIR}/include	
	@printf "U CC\t%s\n" "$<"
	@test -d $(dir $@) || mkdir -p $(dir $@)
	@${CC} ${CFLAGS} -c -o $@ $<
