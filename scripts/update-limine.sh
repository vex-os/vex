#!/bin/sh

export LANG=C.UTF-8
export LANGUAGE=C.UTF-8
export LC_ALL=C.UTF-8

cd $(dirname $(realpath $(dirname ${0})))

url_base_bin="https://raw.githubusercontent.com/Limine-Bootloader/limine/refs/heads/v11.x-binary"
url_base_hdr="https://raw.githubusercontent.com/Limine-Bootloader/limine-protocol/refs/heads/trunk"

mkdir -p boot/limine || exit 1
mkdir -p kernel/include || exit 1

${SHELL} scripts/download-clean.sh kernel/include/limine.h      "${url_base_hdr}/include/limine.h"  || exit 1
${SHELL} scripts/download-clean.sh boot/limine/BOOTAA64.EFI     "${url_base_bin}/BOOTAA64.EFI"      || exit 1
${SHELL} scripts/download-clean.sh boot/limine/BOOTRISCV64.EFI  "${url_base_bin}/BOOTRISCV64.EFI"   || exit 1
${SHELL} scripts/download-clean.sh boot/limine/BOOTX64.EFI      "${url_base_bin}/BOOTX64.EFI"       || exit 1

exit 0
