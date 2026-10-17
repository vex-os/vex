#!/bin/sh

export LANG=C.UTF-8
export LANGUAGE=C.UTF-8
export LC_ALL=C.UTF-8

cd "$(dirname "$(realpath "${0}")")" || exit 1


url_base_bin="https://raw.githubusercontent.com/Limine-Bootloader/limine/refs/heads/v11.x-binary"
url_base_hdr="https://raw.githubusercontent.com/Limine-Bootloader/limine-protocol/refs/heads/trunk"

mkdir -p include || exit 1

sh ../download.sh include/limine.h              "${url_base_hdr}/include/limine.h"              || exit 1
sh ../download.sh BOOTAA64.EFI                  "${url_base_bin}/BOOTAA64.EFI"                  || exit 1
sh ../download.sh BOOTRISCV64.EFI               "${url_base_bin}/BOOTRISCV64.EFI"               || exit 1
sh ../download.sh BOOTX64.EFI                   "${url_base_bin}/BOOTX64.EFI"                   || exit 1

exit 0
