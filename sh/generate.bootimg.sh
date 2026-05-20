#!/bin/sh

export LANG=C.UTF-8
export LANGUAGE=C.UTF-8
export LC_ALL=C.UTF-8

cd $(dirname $(realpath $(dirname ${0})))

input_kernel=${1}
input_efi_arch=${2}
output_image=${3}

rootfs_dir=build/root
esp_size_mib=4
min_size_mib=64

if test -d "${PWD}/${rootfs_dir}"; then
    rootfs_size_mib=$(du -sm "${PWD}/${rootfs_dir}" | awk '{print $1}')
    data_size_mib=$(( rootfs_size_mib + rootfs_size_mib / 5 + 1 ))
else
    data_size_mib=0
fi

image_size_mib=$(( esp_size_mib + data_size_mib + 5 ))

if test ${image_size_mib} -lt ${min_size_mib}; then
    image_size_mib=${min_size_mib}
fi

esp_offset=2048
data_offset=$(( esp_offset + esp_size_mib * 2048 ))

dd if=/dev/zero of=${output_image} bs=1M count=${image_size_mib}

{
    printf "label: gpt\n"
    printf "start=%d,size=%d,type=C12A7328-F81F-11D2-BA4B-00A0C93EC93B,name=ESP\n" ${esp_offset} $(( esp_size_mib * 2048 ))
    printf "type=EBD0A0A2-B9E5-4433-87C0-68B6B72699C7,name=DATA\n"
} | sfdisk ${output_image}

mkfs.fat -F16 --offset ${esp_offset} ${output_image}
mkfs.fat -F32 --offset ${data_offset} ${output_image}

mmd -i ${output_image}@@${esp_offset}s ::/EFI
mmd -i ${output_image}@@${esp_offset}s ::/EFI/BOOT
mcopy -i ${output_image}@@${esp_offset}s boot/uefi/BOOT${input_efi_arch}.EFI ::/EFI/BOOT/BOOT${input_efi_arch}.EFI
mcopy -i ${output_image}@@${esp_offset}s ${input_kernel} ::/vexsys.elf
mcopy -i ${output_image}@@${esp_offset}s boot/limine.conf ::/limine.conf

if test -d "${PWD}/${rootfs_dir}"; then
    mcopy -i ${output_image}@@${data_offset}b -s ${rootfs_dir}/. ::/
fi
