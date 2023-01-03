/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, Kirill GPRB */
#include <bitmap.h>
#include <kan/debug.h>
#include <kan/kprintf.h>
#include <kan/pmem.h>
#include <limine.h>
#include <stdbool.h>
#include <string.h>

static uintptr_t hhdm_offset = 0;
static uintptr_t phys_limit = 0;
static bitmap_t bitmap = { 0 };
static size_t last_page = 0;

static volatile __used struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
    .response = NULL,
};

static volatile __used struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
    .response = NULL,
};

static __force_inline size_t get_page_bit(uintptr_t address)
{
    return page_align_address(address) / PAGE_SIZE;
}

uintptr_t pmalloc(size_t n)
{
    size_t i;

    for(i = last_page; i < bitmap.num_bits; i++) {
        if(!bitmap_check_range(&bitmap, i, i + n - 1))
            continue;
        goto found;
    }

    for(i = 0; i < last_page; i++) {
        if(!bitmap_check_range(&bitmap, i, i + n - 1))
            continue;
        goto found;
    }

    /* FIXME: can we just return NULL? User-space
     * applications won't benefit from the kernel
     * straight up shitting itself when they go
     * "please sir I want some more memory!"... */
    panic("pmem: not enough memory (can't allocate %zu pages)", n);
    unreachable();
    return 0;

found:
    last_page = i + 1;
    bitmap_clear_range(&bitmap, i, i + n - 1);
    return i * PAGE_SIZE;
}

void pmfree(uintptr_t p, size_t n)
{
    size_t page = get_page_bit(p);
    bitmap_set_range(&bitmap, page, page + n - 1);
    last_page = page;
}

static int init_pmem(void)
{
    size_t a;
    size_t b;
    uint64_t i;
    uintptr_t entry_end;
    const char *entry_name;
    const struct limine_memmap_entry *entry;

    kassertf(hhdm_request.response, "pmem: no limine_hhdm_response");
    kassertf(memmap_request.response, "pmem: no limine_memmap_response");

    hhdm_offset = hhdm_request.response->offset;
    phys_limit = 0;
    bitmap.data = NULL;
    bitmap.num_bits = 0;
    bitmap.num_bytes = 0;
    last_page = 0;

    /* Determine physical memory limit */
    for(i = 0; i < memmap_request.response->entry_count; i++) {
        entry = memmap_request.response->entries[i];
        entry_end = entry->base + entry->length - 1;
        switch(entry->type) {
            case LIMINE_MEMMAP_USABLE:
                entry_name = "usable";
                break;
            case LIMINE_MEMMAP_RESERVED:
                entry_name = "reserved";
                break;
            case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
                entry_name = "ACPI data";
                break;
            case LIMINE_MEMMAP_ACPI_NVS:
                entry_name = "ACPI NVS";
                break;
            case LIMINE_MEMMAP_BAD_MEMORY:
                entry_name = "bad memory";
                break;
            case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
                entry_name = "bootloader";
                break;
            case LIMINE_MEMMAP_KERNEL_AND_MODULES:
                entry_name = "kernel";
                break;
            case LIMINE_MEMMAP_FRAMEBUFFER:
                entry_name = "framebuffer";
                break;
            default:
                entry_name = "unknown";
                break;
        }

        pr_debug("pmem: [%p..%p] %s", (void *)entry->base, (void *)entry_end, entry_name);

        switch(entry->type) {
            case LIMINE_MEMMAP_USABLE:
            case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
            case LIMINE_MEMMAP_ACPI_NVS:
            case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
            case LIMINE_MEMMAP_KERNEL_AND_MODULES:
                if(phys_limit < entry_end)
                    phys_limit = entry_end;
                break;
        }
    }

    bitmap_init(&bitmap, get_page_count(phys_limit));

    /* Determine a suitable place for the bitmap */
    for(i = 0; i < memmap_request.response->entry_count; i++) {
        entry = memmap_request.response->entries[i];
        if(entry->type == LIMINE_MEMMAP_USABLE && entry->length >= bitmap.num_bytes) {
            bitmap.data = (uint8_t *)(entry->base + hhdm_offset);
            memset(bitmap.data, 0, bitmap.num_bytes);
            break;
        }
    }

    kassertf(bitmap.data, "pmem: not enough memory to initialize");

    /* Mark USABLE pages as free memory */
    for(i = 0; i < memmap_request.response->entry_count; i++) {
        entry = memmap_request.response->entries[i];
        if(entry->type == LIMINE_MEMMAP_USABLE) {
            a = get_page_bit(entry->base);
            b = get_page_bit(entry->base + entry->length - 1);
            bitmap_set_range(&bitmap, a, b);
        }
    }

    /* Mark bitmap as non-free memory */
    a = get_page_bit((uintptr_t)bitmap.data - hhdm_offset);
    b = get_page_bit((uintptr_t)bitmap.data - hhdm_offset + bitmap.num_bytes - 1);
    bitmap_clear_range(&bitmap, a, b);

    /* Ensure pmalloc seeks at the first available page */
    pmfree(pmalloc(1), 1);

    pr_debug("pmem: bitmap.data=%p", (void *)bitmap.data);
    pr_debug("pmem: bitmap.num_bits=%zu", bitmap.num_bits);
    pr_debug("pmem: bitmap.num_bytes=%zu", bitmap.num_bytes);
    pr_debug("pmem: phys_limit=%p (%zu MiB)", (void *)phys_limit, (phys_limit + 1) / 0x100000);

    return 0;
}
initcall_tier_0(pmem, init_pmem);
