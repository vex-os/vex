#!/bin/sh

export LANG=C.UTF-8
export LANGUAGE=C.UTF-8
export LC_ALL=C.UTF-8

cd $(dirname $(realpath $(dirname ${0})))

downloader=

if command -v wget > /dev/null 2>&1; then
    downloader="wget -q --show-progress -O"
elif command -v curl > /dev/null 2>&1; then
    downloader="curl -L -o"
else
    >&2 printf "error: no download tool found\n"
    exit 1
fi

rm -fv boot/qemu/RELEASEAARCH64_QEMU_EFI.fd
rm -fv boot/qemu/RELEASERISCV64_VIRT.fd
rm -fv boot/qemu/RELEASEX64_OVMF.fd

url_base="https://retrage.github.io/edk2-nightly"

${downloader} boot/qemu/RELEASEAARCH64_QEMU_EFI.fd "${url_base}/bin/RELEASEAARCH64_QEMU_EFI.fd"
${downloader} boot/qemu/RELEASERISCV64_VIRT.fd "${url_base}/bin/RELEASERISCV64_VIRT.fd"
${downloader} boot/qemu/RELEASEX64_OVMF.fd "${url_base}/bin/RELEASEX64_OVMF.fd"

exit 0
