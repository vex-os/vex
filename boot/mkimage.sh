#!/bin/sh
# Usage: mkimage.sh <output> <limine-efi> <limine.conf> <kernel>
# Creates a GPT disk image with a FAT16 ESP containing
# the bootloader, its configuration and the kernel binary

export LANG=C.UTF-8
export LANGUAGE=C.UTF-8
export LC_ALL=C.UTF-8

if test $# -ne 4; then
    printf "usage: %s <output> <limine-efi> <limine.conf> <kernel>\n" "${0}" >&2
    exit 1
fi

output="${1}"
limine_efi="${2}"
limine_conf="${3}"
kernel="${4}"

temp_file="${output}.tmp"

trap 'rm -f "${temp_file}"' EXIT

dd if=/dev/zero of="${temp_file}" bs=1M count=32 status=none || exit 1

sfdisk --quiet --force "${temp_file}" > /dev/null << SFDISK || exit 1
label: gpt
start=2048,size=8192,type=C12A7328-F81F-11D2-BA4B-00A0C93EC93B,name=ESP
type=EBD0A0A2-B9E5-4433-87C0-68B6B72699C7,name=DATA
SFDISK

mkfs.fat -F16 --offset 2048 "${temp_file}" > /dev/null || exit 1

if command -v echfs-utils > /dev/null 2>&1; then
    echfs-utils -g -p 1 "${temp_file}" quick-format 512 > /dev/null || exit 1
fi

esp="${temp_file}@@2048s"

mmd -i "${esp}" ::/EFI ::/EFI/BOOT || exit 1
mcopy -i "${esp}" "${limine_efi}" "::/EFI/BOOT/$(basename "${limine_efi}")" || exit 1
mcopy -i "${esp}" "${limine_conf}" ::/limine.conf || exit 1
mcopy -i "${esp}" "${kernel}" ::/vexsys.elf || exit 1

mv -f "${temp_file}" "${output}" || exit 1

exit 0
