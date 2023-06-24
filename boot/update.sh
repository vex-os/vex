#!/bin/sh

download() { wget -q --show-progress -O "${2}/${3}" "${1}/${3}"; }

ovmf_url="https://retrage.github.io/edk2-nightly/bin"
download ${ovmf_url} boot/riscv64 RELEASERISCV64_VIRT.fd
download ${ovmf_url} boot/x86_64 RELEASEX64_OVMF.fd

limine_url="https://raw.githubusercontent.com/limine-bootloader/limine/v5.x-branch-binary"
download ${limine_url} boot/riscv64 BOOTRISCV64.EFI
download ${limine_url} boot/x86_64 BOOTX64.EFI
download ${limine_url} include limine.h
