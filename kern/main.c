/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <kern/boot.h>
#include <kern/init.h>
#include <kern/pmm.h>
#include <kern/systm.h>

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

static void parse_memmap(const struct limine_memmap_entry *restrict entry)
{
    size_t split_length;
    struct limine_memmap_entry dummy = { 0 };

    dummy.base = entry->base;
    dummy.length = entry->length;

#if HAS_ZONE_DMA
    if(dummy.base < ZONE_DMA_END) {
        if(dummy.base + dummy.length > ZONE_DMA_END) {
            split_length = ZONE_DMA_END - dummy.base;
            pmm_add_memblock(dummy.base, get_page_count(split_length), PMM_ZONE_NORMAL_BIT | PMM_ZONE_DMA_BIT | PMM_ZONE_DMA32_BIT);
            dummy.base += split_length;
            dummy.length -= split_length;
        }
        else {
            pmm_add_memblock(dummy.base, get_page_count(dummy.length), PMM_ZONE_NORMAL_BIT | PMM_ZONE_DMA_BIT | PMM_ZONE_DMA32_BIT);
            return;
        }
    }
#endif

#if HAS_ZONE_DMA32
    if(dummy.base < ZONE_DMA32_END) {
        if(dummy.base + dummy.length > ZONE_DMA32_END) {
            split_length = ZONE_DMA32_END - dummy.base;
            pmm_add_memblock(dummy.base, get_page_count(split_length), PMM_ZONE_NORMAL_BIT | PMM_ZONE_DMA32_BIT);
            dummy.base += split_length;
            dummy.length -= split_length;
        }
        else {
            pmm_add_memblock(dummy.base, get_page_count(dummy.length), PMM_ZONE_NORMAL_BIT | PMM_ZONE_DMA32_BIT);
            return;
        }
    }
#endif

    pmm_add_memblock(dummy.base, get_page_count(dummy.length), PMM_ZONE_NORMAL_BIT);
}

void __noreturn __used kmain(void)
{
    size_t i;

    kprintf("%s %s %s", sysname, release, version);

    kassertf(hhdm_request.response, "kmain: hhdm not available");
    kassertf(memmap_request.response, "kmain: memmap not available");
    kassertf(kernel_address_request.response, "kmain: kernel address not available");

    highmem = hhdm_request.response->offset;
    kbase_phys = kernel_address_request.response->physical_base;
    kbase_virt = kernel_address_request.response->virtual_base;

    for(i = 0; i < memmap_request.response->entry_count; ++i) {
        if(memmap_request.response->entries[i]->type != LIMINE_MEMMAP_USABLE)
            continue;
        parse_memmap(memmap_request.response->entries[i]);
    }

    /* UNDONE: should SMP and sched use initcalls? */
    for(i = 0; initcalls[i]; initcalls[i++]());

    panic("kmain: nothing else to do!");
    unreachable();
}
