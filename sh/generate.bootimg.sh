#!/bin/sh

export LANG=C.UTF-8
export LANGUAGE=C.UTF-8
export LC_ALL=C.UTF-8

cd $(dirname $(realpath $(dirname ${0})))

BOOTIMG=${1}
EFI_ARCH=${2}

KERNEL=output/vexsys.elf
LIMINE_CONF=boot/limine.conf
EFI_BINARY=boot/uefi/BOOT${EFI_ARCH}.EFI
ROOTFS_DIR=build/root

cd $(dirname $(realpath $(dirname ${0})))

# ESP is fixed at 4 MiB (8192 sectors starting at 2048)
ESP_SIZE_MIB=4
MIN_SIZE_MIB=64

if test -d "${ROOTFS_DIR}"; then
    ROOTFS_SIZE_MIB=$(du -sm "${ROOTFS_DIR}" | awk '{print $1}')
    # 20% overhead for the data partition
    DATA_SIZE_MIB=$(( ROOTFS_SIZE_MIB + ROOTFS_SIZE_MIB / 5 + 1 ))
else
    DATA_SIZE_MIB=0
fi

BOOTIMG_SIZE_MIB=$(( ESP_SIZE_MIB + DATA_SIZE_MIB + 5 ))
if test ${BOOTIMG_SIZE_MIB} -lt ${MIN_SIZE_MIB}; then
    BOOTIMG_SIZE_MIB=${MIN_SIZE_MIB}
fi

dd if=/dev/zero of=${BOOTIMG} bs=1M count=${BOOTIMG_SIZE_MIB}

{
    printf 'label: gpt\n'
    printf 'start=2048,size=8192,type=C12A7328-F81F-11D2-BA4B-00A0C93EC93B,name=ESP\n'
    printf 'type=0FC63DAF-8483-4772-8E79-3D69D8477DE4,name=DATA\n'
} | sfdisk ${BOOTIMG}

mkfs.fat -F16 --offset 2048 ${BOOTIMG}
mkfs.ext2 -F -E offset=$((10240 * 512)) ${BOOTIMG}

mmd -i ${BOOTIMG}@@2048s ::/EFI
mmd -i ${BOOTIMG}@@2048s ::/EFI/BOOT
mcopy -i ${BOOTIMG}@@2048s ${EFI_BINARY} ::/EFI/BOOT/BOOT${EFI_ARCH}.EFI
mcopy -i ${BOOTIMG}@@2048s ${LIMINE_CONF} ::/limine.conf
mcopy -i ${BOOTIMG}@@2048s ${KERNEL} ::/vexsys.elf

if test -d "${ROOTFS_DIR}"; then
    mcopy -i ${BOOTIMG}@@$((10240 * 512))b -s ${ROOTFS_DIR}/. ::/
fi
