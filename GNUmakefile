## SPDX-License-Identifier: Zlib

export LC_ALL=C
export LANGUAGE=C
export LANG=C

RELEASE ?= 0.0.1-dev.17
SYSNAME ?= Vex

ARCH ?= x86_64
TOOLCHAIN ?= llvm

SOURCES :=
OBJECTS :=

CLEAN0 :=
CLEAN1 :=

ALLDEP :=
PHONYS :=

build_dir := build

include config/arch.${ARCH}.mk
include config/toolchain.${TOOLCHAIN}.mk

CFLAGS += -fPIE
CFLAGS += -ffreestanding
CFLAGS += -Wall -Wextra -Werror
CFLAGS += -Wno-unused-parameter
CFLAGS += -Wno-pointer-sign
CFLAGS += -funsigned-char
CFLAGS += -O2

CPPFLAGS += -D __kernel__
CPPFLAGS += -D __KERNEL__
CPPFLAGS += -I arch/${ARCH}/include
CPPFLAGS += -I contrib/limine/include
CPPFLAGS += -I include

LDFLAGS += -nostdlib

%.c.o: %.c | ${build_dir}
	${CC} ${CFLAGS} ${CPPFLAGS} -c -o $@ $<
#	${CC} ${CFLAGS} ${CPPFLAGS} -S -o ${build_dir}/$(notdir $<).S $<
%.S.o: %.S | ${build_dir}
	${CC} ${CFLAGS} ${CPPFLAGS} -c -o $@ $<
%.s.o: %.s | ${build_dir}
	${CC} ${CFLAGS} ${CPPFLAGS} -c -o $@ $<

LDSCRIPT := ${build_dir}/link.ld
KERNEL := ${build_dir}/kernel.elf
KBOOT := ${build_dir}/kernel.boot.img

include arch/${ARCH}/src/GNUmakefile
include libk/GNUmakefile
include src/GNUmakefile

OBJECTS += ${SOURCES:=.o}

CLEAN0 += ${OBJECTS}
CLEAN0 += ${LDSCRIPT}
CLEAN0 += ${KERNEL}
CLEAN0 += ${KBOOT}

CLEAN1 += ${build_dir}

PHONYS += all
PHONYS += force_run
PHONYS += clean distclean
PHONYS += kernel
PHONYS += bootable

ALLDEP += kernel
ALLDEP += bootable

all: ${ALLDEP}

force_run:

clean:
	rm -vrf ${CLEAN0}

distclean:
	rm -vrf ${CLEAN0}
	rm -vrf ${CLEAN1}

kernel: ${KERNEL}

bootable: ${KBOOT}

${build_dir}:
	mkdir -p ${build_dir}

${KERNEL}: ${OBJECTS} ${LDSCRIPT} ${build_dir}
	${LD} ${LDFLAGS} -T ${LDSCRIPT} -pie -o $@ ${OBJECTS}

${LDSCRIPT}: config/link.${ARCH}.lds ${build_dir}
	${CC} ${CPPFLAGS} -E -xc -D __ASSEMBLER__ config/link.${ARCH}.lds | grep -v "^#" > $@ || true

${KBOOT}: ${KERNEL}
	dd if=/dev/zero of=$@ bs=1048576 count=32
	parted $@ mklabel gpt
	parted $@ mkpart ESP fat16 2048s 100%
	parted $@ set 1 esp on
	mkfs.fat -F16 --offset 2048 $@
	mmd -i $@@@2048s ::/EFI
	mmd -i $@@@2048s ::/EFI/BOOT
	mcopy -i $@@@2048s boot/${ARCH}/BOOT${UEFI_ARCH}.EFI ::/EFI/BOOT/BOOT${UEFI_ARCH}.EFI
	mcopy -i $@@@2048s boot/limine.cfg ::/limine.cfg
	mcopy -i $@@@2048s ${KERNEL} ::/kernel.${LIMINE_ARCH}.elf
