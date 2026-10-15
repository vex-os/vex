# Protocol header for the kernel and prebuilt
# bootloader binaries for the boot image
limine_includes = include_directories('include')

limine_efi_names = {
    'amd64': 'BOOTX64.EFI',
    'aarch64': 'BOOTAA64.EFI',
    'riscv64': 'BOOTRISCV64.EFI',
}

limine_efi = files(limine_efi_names[arch])
