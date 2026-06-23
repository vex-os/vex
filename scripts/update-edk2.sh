#!/bin/sh

export LANG=C.UTF-8
export LANGUAGE=C.UTF-8
export LC_ALL=C.UTF-8

cd $(dirname $(realpath $(dirname ${0})))

url_base="https://retrage.github.io/edk2-nightly"

mkdir -p boot/qemu || exit 1

${SHELL} scripts/download-clean.sh boot/qemu/RELEASEAARCH64_QEMU_EFI.fd "${url_base}/bin/RELEASEAARCH64_QEMU_EFI.fd"    || exit 1
${SHELL} scripts/download-clean.sh boot/qemu/RELEASERISCV64_VIRT.fd     "${url_base}/bin/RELEASERISCV64_VIRT.fd"        || exit 1
${SHELL} scripts/download-clean.sh boot/qemu/RELEASEX64_OVMF.fd         "${url_base}/bin/RELEASEX64_OVMF.fd"            || exit 1

exit 0
