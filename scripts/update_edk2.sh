#!/bin/sh

export LANG=C.UTF-8
export LANGUAGE=C.UTF-8
export LC_ALL=C.UTF-8

cd $(dirname $(realpath $(dirname ${0})))

rm -fv boot/aarch64/RELEASEAARCH64_QEMU_EFI.fd
rm -fv boot/riscv64/RELEASERISCV64_VIRT.fd
rm -fv boot/x86_64/RELEASEX64_OVMF.fd

url_base="https://retrage.github.io/edk2-nightly"
wget -q --show-progress -P boot/aarch64 "${url_base}/bin/RELEASEAARCH64_QEMU_EFI.fd"
wget -q --show-progress -P boot/riscv64 "${url_base}/bin/RELEASERISCV64_VIRT.fd"
wget -q --show-progress -P boot/x86_64  "${url_base}/bin/RELEASEX64_OVMF.fd"
