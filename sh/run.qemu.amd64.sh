#!/bin/sh

cd $(dirname $(realpath $(dirname ${0})))

if test -f "/proc/sys/fs/binfmt_misc/WSLInterop"; then
    UEFI_PATH=$(wslpath -a -w boot/qemu/RELEASEX64_OVMF.fd)
    IMAGE_PATH=$(wslpath -a -w build/vexsys.boot.img)
    QEMU_EXECUTABLE=qemu-system-x86_64.exe
else
    UEFI_PATH=boot/qemu/RELEASEX64_OVMF.fd
    IMAGE_PATH=build/vexsys.boot.img
    QEMU_EXECUTABLE=qemu-system-x86_64
fi

qargs=
qargs="${qargs} -bios ${UEFI_PATH}"
qargs="${qargs} -drive format=raw,file=${IMAGE_PATH}"
qargs="${qargs} -debugcon stdio"
qargs="${qargs} -machine type=q35"
qargs="${qargs} -vga std"
qargs="${qargs} -m 512M"

# only log stuff if argv[1] is verbose
if test "${1}" = "verbose"; then
    qargs="${qargs} -d int -D qemu.debug.log"
fi

{ ${QEMU_EXECUTABLE} ${qargs} 2>&1; printf "\n"; } | tee qemu.log
