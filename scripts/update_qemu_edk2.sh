#!/bin/sh

export LANG=C.UTF-8
export LANGUAGE=C.UTF-8
export LC_ALL=C.UTF-8

cd $(dirname $(realpath $(dirname ${0})))

loader=

if command -v wget > /dev/null 2>&1; then
    loader="wget -q --show-progress -O"
elif command -v curl > /dev/null 2>&1; then
    loader="curl -L -o"
else
    >&2 printf "neither 'wget' nor 'curl' is installed\n"
    exit 1
fi

rm -fv boot/qemu/RELEASEAARCH64_QEMU_EFI.fd
rm -fv boot/qemu/RELEASERISCV64_VIRT.fd
rm -fv boot/qemu/RELEASEX64_OVMF.fd

url_base="https://retrage.github.io/edk2-nightly"

${loader} boot/qemu/RELEASEAARCH64_QEMU_EFI.fd "${url_base}/bin/RELEASEAARCH64_QEMU_EFI.fd"
${loader} boot/qemu/RELEASERISCV64_VIRT.fd "${url_base}/bin/RELEASERISCV64_VIRT.fd"
${loader} boot/qemu/RELEASEX64_OVMF.fd "${url_base}/bin/RELEASEX64_OVMF.fd"

exit 0
