#!/bin/sh

cd $(dirname $(realpath $(dirname ${0})))

if test -f "/proc/sys/fs/binfmt_misc/WSLInterop"; then
    UEFI_PATH=$(wslpath -a -w boot/qemu/RELEASEX64_OVMF.fd)
    IMAGE_PATH=$(wslpath -a -w output/vexsys.boot.img)
    QEMU_EXECUTABLE=qemu-system-x86_64.exe
else
    UEFI_PATH=boot/qemu/RELEASEX64_OVMF.fd
    IMAGE_PATH=output/vexsys.boot.img
    QEMU_EXECUTABLE=qemu-system-x86_64
fi

qargs=
qargs="${qargs} -bios ${UEFI_PATH}"
qargs="${qargs} -drive format=raw,file=${IMAGE_PATH}"
qargs="${qargs} -debugcon stdio"
qargs="${qargs} -machine type=q35"
qargs="${qargs} -vga std"
qargs="${qargs} -m 512M"
#qargs="${qargs} -d cpu_reset -no-reboot -no-shutdown"
#qargs="${qargs} -d int"

${QEMU_EXECUTABLE} ${qargs} || exit 1

printf "\n"
