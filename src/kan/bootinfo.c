/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <kan/bootinfo.h>
#include <kan/debug.h>
#include <kan/kprintf.h>
#include <kan/symbol.h>
#include <stddef.h>

static volatile struct limine_bootloader_info_request info_request = {
    .id = LIMINE_BOOTLOADER_INFO_REQUEST,
    .response = NULL,
    .revision = 0,
};

static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .response = NULL,
    .revision = 0,
};

static volatile struct limine_kernel_address_request kernel_address_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .response = NULL,
    .revision = 0,
};

static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .response = NULL,
    .revision = 0,
};

static uintptr_t hhdm_offset = 0;
static uintptr_t kernel_address_phys = 0;
static uintptr_t kernel_address_virt = 0;

uintptr_t get_hhdm_offset(void)
{
    return hhdm_offset;
}
EXPORT_SYMBOL(get_hhdm_offset);

uintptr_t get_kernel_address_phys(void)
{
    return kernel_address_phys;
}
EXPORT_SYMBOL(get_kernel_address_phys);

uintptr_t get_kernel_address_virt(void)
{
    return kernel_address_virt;
}
EXPORT_SYMBOL(get_kernel_address_virt);

const struct limine_memmap_response *get_memmap(void)
{
    return memmap_request.response;
}

static int init_bootinfo(void)
{
    const char *name;
    const char *version;

    if(info_request.response) {
        name = info_request.response->name;
        version = info_request.response->version;
    }
    else {
        name = "unknown";
        version = "unknown";
    }

    pr_inform("bootinfo: %s %s", name, version);

    panic_if(!hhdm_request.response, "bootinfo: no hhdm_response");
    panic_if(!kernel_address_request.response, "bootinfo: no kernel_address_response");
    panic_if(!memmap_request.response, "bootinfo: no memmap_response");

    hhdm_offset = hhdm_request.response->offset;
    kernel_address_phys = kernel_address_request.response->physical_base;
    kernel_address_virt = kernel_address_request.response->virtual_base;

    return 0;
}
initcall_tier_0(bootinfo, init_bootinfo);
