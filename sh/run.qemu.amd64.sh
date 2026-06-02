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

if test -f "/proc/sys/fs/binfmt_misc/WSLInterop"; then
    CDROM_PATH=$(wslpath -a -w test.iso)
else
    CDROM_PATH=test.iso
fi

qargs=
qargs="${qargs} -bios ${UEFI_PATH}"
qargs="${qargs} -drive format=raw,file=${IMAGE_PATH}"
qargs="${qargs} -drive format=raw,file=${CDROM_PATH},media=cdrom,if=none,id=cdrom0"
qargs="${qargs} -device ahci,id=ahci0"
qargs="${qargs} -device ide-cd,drive=cdrom0,bus=ahci0.1"
qargs="${qargs} -debugcon stdio"
qargs="${qargs} -machine type=q35"
qargs="${qargs} -vga std"
qargs="${qargs} -m 512M"
qargs="${qargs} -smp 4"

# only log stuff if argv[1] is verbose
if test "${1}" = "verbose"; then
    qargs="${qargs} -d cpu_reset,int -no-reboot -no-shutdown"
fi

{ ${QEMU_EXECUTABLE} ${qargs} 2>&1; printf "\n"; } | tee qemu.log
