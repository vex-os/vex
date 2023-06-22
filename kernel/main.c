/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <kernel/boot.h>
#include <kernel/init.h>
#include <kernel/pmm.h>
#include <kernel/systm.h>

uintptr_t highmem = 0;
uintptr_t kbase_phys = 0;
uintptr_t kbase_virt = 0;

volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
    .response = NULL,
};

volatile struct limine_kernel_address_request kernel_address_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0,
    .response = NULL,
};

volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
    .response = NULL,
};

void __noreturn __used main(void)
{
    size_t i;

    kprintf("%s %s %s", sysname, release, version);

    kassertf(hhdm_request.response, "main: hhdm not available");
    kassertf(memmap_request.response, "main: memmap not available");
    kassertf(kernel_address_request.response, "main: kernel address not available");

    highmem = hhdm_request.response->offset;
    kbase_phys = kernel_address_request.response->physical_base;
    kbase_virt = kernel_address_request.response->virtual_base;

    /* UNDONE: should SMP and sched use initcalls? */
    for(i = 0; initcalls[i]; initcalls[i++]());

    panic("main: nothing else to do!");
    unreachable();
}
