#!/bin/sh
qargs=
qargs="${qargs} -cdrom ${PWD}/build/kernel.iso"
qargs="${qargs} -debugcon stdio"
qargs="${qargs} -machine type=q35"
qargs="${qargs} -vga std"
qargs="${qargs} -m 512M"
# qargs="${qargs} -d cpu_reset -d int -no-reboot -no-shutdown"
qemu-system-x86_64 ${qargs} ${@}
printf "\n"
