/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <machine/boot.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/iecprefix.h>
#include <sys/init.h>
#include <sys/klog.h>
#include <sys/panic.h>
#include <sys/pmm.h>
#include <sys/version.h>

volatile struct limine_5_level_paging_request five_level_paging_request = {
    .id = LIMINE_5_LEVEL_PAGING_REQUEST,
    .revision = 0,
    .response = NULL,
};

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

uintptr_t hhdm_offset = 0;
uintptr_t kernel_base_phys = 0;
uintptr_t kernel_base_virt = 0;

#define ZONE_DMA_END MiB(16)
#define ZONE_DMA32_END GiB(4)

static void parse_memmap(const struct limine_memmap_entry *restrict entry)
{
    size_t split_length;
    struct limine_memmap_entry dummy = { 0 };

    dummy.base = entry->base;
    dummy.length = entry->length;

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

    pmm_add_memblock(dummy.base, get_page_count(dummy.length), PMM_ZONE_NORMAL_BIT);
}

void __used __noreturn kstart(void)
{
    size_t i;

    klog(LOG_INFO, "kstart: starting version %s+%s", version, git_revision);

    kassertf(hhdm_request.response, "kstart: hhdm not available");
    kassertf(memmap_request.response, "kstart: memmap not available");
    kassertf(kernel_address_request.response, "kstart: kernel address not available");

    hhdm_offset = hhdm_request.response->offset;
    kernel_base_phys = kernel_address_request.response->physical_base;
    kernel_base_virt = kernel_address_request.response->virtual_base;

    for(i = 0; i < memmap_request.response->entry_count; ++i) {
        if(memmap_request.response->entries[i]->type != LIMINE_MEMMAP_USABLE)
            continue;
        parse_memmap(memmap_request.response->entries[i]);
    }

    /* UNDONE: should SMP and sched use initcalls? */
    for(i = 0; initcalls[i]; initcalls[i++]());

    panic("kstart: nothing else to do!");
    UNREACHABLE();
}