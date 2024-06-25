/* SPDX-License-Identifier: Zlib */
#include <core/compiler.h>
#include <core/limine.h>
#include <stddef.h>

uintptr_t hhdm_offset = 0x0000000000000000;
uintptr_t kernel_base_phys = 0x0000000000000000;
uintptr_t kernel_base_virt = 0x0000000000000000;

volatile struct limine_hhdm_request __used hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
    .response = NULL,
};

volatile struct limine_kernel_address_request __used kernel_address_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0,
    .response = NULL,
};

volatile struct limine_paging_mode_request __used paging_mode_request = {
    .id = LIMINE_PAGING_MODE_REQUEST,
    .revision = 0,
    .response = NULL,
    .mode = LIMINE_PAGING_MODE_MAX,
    .flags = 0,
};

volatile struct limine_framebuffer_request __used framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
    .response = NULL,
};

volatile struct limine_memmap_request __used memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
    .response = NULL,
};

volatile struct limine_module_request __used module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 0,
    .response = NULL,
};
