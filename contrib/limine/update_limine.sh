#!/bin/sh

export LANG=C.UTF-8
export LANGUAGE=C.UTF-8
export LC_ALL=C.UTF-8

cd $(realpath $(dirname ${0}))

rm -fv BOOTAA64.EFI
rm -fv BOOTRISCV64.EFI
rm -fv BOOTX64.EFI
rm -fv include/limine.h

url_base="https://raw.githubusercontent.com/limine-bootloader/limine/v7.x-binary"
wget -q --show-progress -P ${PWD} "${url_base}/BOOTAA64.EFI"
wget -q --show-progress -P ${PWD} "${url_base}/BOOTRISCV64.EFI"
wget -q --show-progress -P ${PWD} "${url_base}/BOOTX64.EFI"
wget -q --show-progress -P ${PWD}/include "${url_base}/limine.h"
