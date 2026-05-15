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

url_base_h="https://raw.githubusercontent.com/Limine-Bootloader/limine-protocol/refs/heads/trunk"
url_base="https://raw.githubusercontent.com/Limine-Bootloader/limine/refs/heads/v11.x-binary"

rm -fv lib/limine.h || true
rm -fv boot/uefi/BOOTAA64.EFI || true
rm -fv boot/uefi/BOOTRISCV64.EFI || true
rm -fv boot/uefi/BOOTX64.EFI || true

${downloader} lib/limine.h "${url_base_h}/include/limine.h"
${downloader} boot/uefi/BOOTAA64.EFI "${url_base}/BOOTAA64.EFI"
${downloader} boot/uefi/BOOTRISCV64.EFI "${url_base}/BOOTRISCV64.EFI"
${downloader} boot/uefi/BOOTX64.EFI "${url_base}/BOOTX64.EFI"

exit 0
