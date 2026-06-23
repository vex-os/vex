#!/bin/sh

export LANG=C.UTF-8
export LANGUAGE=C.UTF-8
export LC_ALL=C.UTF-8

cd $(dirname $(realpath $(dirname ${0})))

if test -f "/proc/sys/fs/binfmt_misc/WSLInterop"; then
    UEFI_PATH=$(wslpath -a -w boot/qemu/RELEASEX64_OVMF.fd)
    IMAGE_PATH=$(wslpath -a -w build/kernel/vexsys.boot.img)
    QEMU_EXECUTABLE=qemu-system-x86_64.exe
else
    UEFI_PATH=boot/qemu/RELEASEX64_OVMF.fd
    IMAGE_PATH=build/kernel/vexsys.boot.img
    QEMU_EXECUTABLE=qemu-system-x86_64
fi

logfile="qemu.log"

qargs=
qargs="${qargs} -bios ${UEFI_PATH}"
qargs="${qargs} -drive format=raw,file=${IMAGE_PATH}"
qargs="${qargs} -debugcon stdio"
qargs="${qargs} -machine type=q35"
qargs="${qargs} -vga std"
qargs="${qargs} -m 512M"
qargs="${qargs} -smp 4"

for arg in "$@"; do
    case "${arg}" in
        slow)    qargs="${qargs} -icount shift=0,sleep=on" ;;
        verbose) qargs="${qargs} -d cpu_reset,int -no-reboot -no-shutdown" ;;
        [0-9]*) logfile="qemu_${arg}.log" ;;
    esac
done

rm -f "${logfile}"

{ ${QEMU_EXECUTABLE} ${qargs} 2>&1; printf "\n"; } | tee "${logfile}"

exit 0
