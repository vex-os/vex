/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, Kirill GPRB */
#include <bitmap.h>
#include <kan/boot.h>
#include <kan/debug.h>
#include <kan/kprintf.h>
#include <kan/pmem.h>
#include <kan/symbol.h>
#include <string.h>

/* for this bitmap, bitweight=PAGE_SIZE */
static uintptr_t phys_limit = 0;
static bitmap_t bitmap = { 0 };
static size_t last_bit = 0;

static __force_inline size_t get_page_bit(uintptr_t address)
{
    return page_align_address(address) / PAGE_SIZE;
}

uintptr_t pmalloc(size_t n)
{
    size_t i;

    for(i = last_bit; i < bitmap.nbits; i++) {
        if(!read_bitmap_range(&bitmap, i, i + n - 1))
            continue;
        goto found;
    }

    for(i = 0; i < last_bit; i++) {
        if(!read_bitmap_range(&bitmap, i, i + n - 1))
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
    last_bit = i + 1;
    clear_bitmap_range(&bitmap, i, i + n - 1);
    return i * PAGE_SIZE;
}
EXPORT_SYMBOL(pmalloc);

void pmfree(uintptr_t pptr, size_t n)
{
    size_t page = get_page_bit(pptr);
    set_bitmap_range(&bitmap, page, page + n - 1);
    last_bit = page;
}
EXPORT_SYMBOL(pmfree);

static int init_pmem(void)
{
    size_t a;
    size_t b;
    uint64_t i;
    uintptr_t entry_end;
    const char *entry_name;
    const struct limine_memmap_entry *entry;

    panic_if(!boot_hhdm.response, "pmem: no boot_hhdm");
    panic_if(!boot_memmap.response, "pmem: no boot_memmap");

    phys_limit = 0;
    bitmap.data = NULL;
    bitmap.nbits = 0;
    bitmap.size = 0;
    last_bit = 0;

    /* Determine physical memory limit */
    for(i = 0; i < boot_memmap.response->entry_count; i++) {
        entry = boot_memmap.response->entries[i];
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

        pr_debug("pmem: [%p - %p] %s", (void *)entry->base, (void *)entry_end, entry_name);

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

    init_bitmap(&bitmap, get_page_count(phys_limit));

    /* Determine a suitable place for the bitmap */
    for(i = 0; i < boot_memmap.response->entry_count; i++) {
        entry = boot_memmap.response->entries[i];
        if(entry->type == LIMINE_MEMMAP_USABLE && entry->length >= bitmap.size) {
            bitmap.data = (uint32_t *)(entry->base + boot_hhdm.response->offset);
            memset(bitmap.data, 0, bitmap.size);
            break;
        }
    }

    panic_if(!bitmap.data, "pmem: not enough memory to initialize");

    /* Mark USABLE pages as free memory */
    for(i = 0; i < boot_memmap.response->entry_count; i++) {
        entry = boot_memmap.response->entries[i];
        if(entry->type == LIMINE_MEMMAP_USABLE) {
            a = get_page_bit(entry->base);
            b = get_page_bit(entry->base + entry->length - 1);
            set_bitmap_range(&bitmap, a, b);
        }
    }

    /* Mark bitmap as non-free memory */
    a = get_page_bit((uintptr_t)bitmap.data - boot_hhdm.response->offset);
    b = get_page_bit((uintptr_t)bitmap.data - boot_hhdm.response->offset + bitmap.size - 1);
    clear_bitmap_range(&bitmap, a, b);

    /* Set last_bit */
    pmfree(pmalloc(1), 1);

    pr_debug("pmem: bitmap.data=%p", (void *)bitmap.data);
    pr_debug("pmem: bitmap.nbits=%zu", bitmap.nbits);
    pr_debug("pmem: bitmap.size=%zu (%zu MiB)", bitmap.size, bitmap.size / 0x100000);
    pr_debug("pmem: phys_limit=%p (%zu MiB)", (void *)phys_limit, (phys_limit + 1) / 0x100000);

    return 0;
}
initcall_tier_0(pmem, init_pmem);