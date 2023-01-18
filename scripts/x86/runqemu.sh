#!/bin/sh
qargs=
qargs="${qargs} -cdrom ${PWD}/kan.iso"
qargs="${qargs} -debugcon stdio"
qargs="${qargs} -machine type=q35"
qargs="${qargs} -vga std"
qargs="${qargs} -m 128M"
qemu-system-x86_64 ${qargs} ${@}
