/* SPDX-License-Identifier: Zlib */
#include <iserix/compiler.h>
#include <iserix/limine.h>
#include <stddef.h>

uintptr_t hhdm_offset = 0x0000000000000000;
uintptr_t kernel_base_phys = 0x0000000000000000;
uintptr_t kernel_base_virt = 0x0000000000000000;

volatile struct limine_hhdm_request USED hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
    .response = NULL,
};

volatile struct limine_kernel_address_request USED kernel_address_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0,
    .response = NULL,
};

volatile struct limine_paging_mode_request USED paging_mode_request = {
    .id = LIMINE_PAGING_MODE_REQUEST,
    .revision = 0,
    .response = NULL,
    .mode = LIMINE_PAGING_MODE_MAX,
    .flags = 0,
};

volatile struct limine_framebuffer_request USED framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
    .response = NULL,
};

volatile struct limine_memmap_request USED memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
    .response = NULL,
};

volatile struct limine_module_request USED module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 0,
    .response = NULL,
};
