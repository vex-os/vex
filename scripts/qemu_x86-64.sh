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

qemu-system-x86_64 ${qargs}

printf "\n"
