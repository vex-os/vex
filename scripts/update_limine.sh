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

url_base="https://codeberg.org/Limine/Limine/raw/branch/v10.x-binary"

rm -fv include/detail/limine.h
rm -fv boot/uefi/BOOTAA64.EFI
rm -fv boot/uefi/BOOTRISCV64.EFI
rm -fv boot/uefi/BOOTX64.EFI

${loader} include/detail/limine.h "${url_base}/limine.h"
${loader} boot/uefi/BOOTAA64.EFI "${url_base}/BOOTAA64.EFI"
${loader} boot/uefi/BOOTRISCV64.EFI "${url_base}/BOOTRISCV64.EFI"
${loader} boot/uefi/BOOTX64.EFI "${url_base}/BOOTX64.EFI"

exit 0
