/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <kan/boot.h>
#include <stddef.h>

volatile struct limine_bootloader_info_request boot_info = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST,
    .response = NULL,
    .revision = 0,
};

volatile struct limine_hhdm_request boot_hhdm = {
    .id = LIMINE_HHDM_REQUEST,
    .response = NULL,
    .revision = 0,
};

volatile struct limine_kernel_address_request boot_kernel_address = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .response = NULL,
    .revision = 0,
};

volatile struct limine_memmap_request boot_memmap = {
    .id = LIMINE_MEMMAP_REQUEST,
    .response = NULL,
    .revision = 0,
};
