#!/bin/sh

export LANG=C.UTF-8
export LANGUAGE=C.UTF-8
export LC_ALL=C.UTF-8

# Make sure the script is ran from the
# source tree root (we use absolute paths)
cd $(dirname $(realpath $(dirname ${0})))

download() { wget -q --show-progress -O "${PWD}/${2}/${3}" "${1}/${3}"; }

limine_url="https://raw.githubusercontent.com/limine-bootloader/limine/v7.x-binary"
download ${limine_url} arch/riscv64/boot BOOTRISCV64.EFI
download ${limine_url} arch/x86_64/boot BOOTX64.EFI
download ${limine_url} include limine.h
