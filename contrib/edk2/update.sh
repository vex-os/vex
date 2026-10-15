#!/bin/sh

export LANG=C.UTF-8
export LANGUAGE=C.UTF-8
export LC_ALL=C.UTF-8

cd "$(dirname "$(realpath "${0}")")" || exit 1


url_base="https://retrage.github.io/edk2-nightly"

${SHELL} ../download.sh RELEASEAARCH64_QEMU_EFI.fd    "${url_base}/bin/RELEASEAARCH64_QEMU_EFI.fd"    || exit 1
${SHELL} ../download.sh RELEASERISCV64_VIRT.fd        "${url_base}/bin/RELEASERISCV64_VIRT.fd"        || exit 1
${SHELL} ../download.sh RELEASEX64_OVMF.fd            "${url_base}/bin/RELEASEX64_OVMF.fd"            || exit 1

exit 0
