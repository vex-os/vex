#!/bin/sh

export LANG=C.UTF-8
export LANGUAGE=C.UTF-8
export LC_ALL=C.UTF-8

cd $(dirname $(realpath $(dirname ${0})))

rm -fv boot/RELEASEAARCH64_QEMU_EFI.fd
rm -fv boot/RELEASERISCV64_VIRT.fd
rm -fv boot/RELEASEX64_OVMF.fd

url_base="https://retrage.github.io/edk2-nightly"
wget -q --show-progress -P boot "${url_base}/bin/RELEASEAARCH64_QEMU_EFI.fd"
wget -q --show-progress -P boot "${url_base}/bin/RELEASERISCV64_VIRT.fd"
wget -q --show-progress -P boot "${url_base}/bin/RELEASEX64_OVMF.fd"
