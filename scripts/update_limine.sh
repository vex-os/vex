#!/bin/sh

# Make sure the script is ran from the
# source tree root (we use absolute paths)
cd $(dirname $(realpath $(dirname ${0})))

download() { wget -q --show-progress -O "${PWD}/${2}/${3}" "${1}/${3}"; }

limine_url="https://raw.githubusercontent.com/limine-bootloader/limine/v5.x-branch-binary"
download ${limine_url} boot/riscv64 BOOTRISCV64.EFI
download ${limine_url} boot/x86_64 BOOTX64.EFI
download ${limine_url} include limine.h
