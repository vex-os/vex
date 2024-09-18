#!/bin/sh

cd $(dirname $(realpath $(dirname ${0})))

qargs=
qargs="${qargs} -bios boot/x86_64/RELEASEX64_OVMF.fd"
qargs="${qargs} -drive format=raw,file=build/kernel.boot.img"
qargs="${qargs} -debugcon stdio"
qargs="${qargs} -machine type=q35"
qargs="${qargs} -vga std"
qargs="${qargs} -m 512M"
#qargs="${qargs} -d cpu_reset -d int -no-reboot -no-shutdown"

if [ -f "/proc/sys/fs/binfmt_misc/WSLInterop" ]
then
    # We are running under WSL2; we want to run
    # a host QEMU distribution and expect its
    # directory to be present in PATH
    qemu-system-x86_64.exe ${qargs}
else
    # We are running in an actual unix-like
    # environment; we want to run a system
    # QEMU installation regardless of whether
    # it's a host or a guest distribution
    qemu-system-x86_64 ${qargs}
fi

printf "\n"
