#!/bin/sh

export LANG=C.UTF-8
export LANGUAGE=C.UTF-8
export LC_ALL=C.UTF-8

cd $(dirname $(realpath $(dirname ${0})))

rm -fv boot/aarch64/BOOTAA64.EFI
rm -fv boot/riscv64/BOOTRISCV64.EFI
rm -fv boot/x86_64/BOOTX64.EFI
rm -fv include/limine.h

url_base="https://raw.githubusercontent.com/limine-bootloader/limine/v7.x-binary"
wget -q --show-progress -P boot/aarch64 "${url_base}/BOOTAA64.EFI"
wget -q --show-progress -P boot/riscv64 "${url_base}/BOOTRISCV64.EFI"
wget -q --show-progress -P boot/x86_64  "${url_base}/BOOTX64.EFI"
wget -q --show-progress -P include      "${url_base}/limine.h"
