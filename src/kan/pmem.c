/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <bitmap.h>
#include <kan/bootinfo.h>
#include <kan/debug.h>
#include <kan/errno.h>
#include <kan/kprintf.h>
#include <kan/pmem.h>
#include <kan/symbol.h>
#include <string.h>

static uintptr_t phys_limit = 0;
static bitmap_t bitmap = { 0 };
static size_t lastbit = 0;

static __force_inline size_t get_page_bit(uintptr_t address)
{
    return page_align_address(address) / PAGE_SIZE;
}

static __force_inline bool try_occupy_range(size_t a, size_t b)
{
    size_t i;
    for(i = a; i <= b; i++) {
        if(!read_bitmap(&bitmap, i)) {
            set_bitmap_range(&bitmap, a, i);
            return false;
        }

        clear_bitmap(&bitmap, i);
    }

    return true;
}

uintptr_t pmalloc(size_t psize)
{
    size_t i;

    for(i = lastbit; i < bitmap.nbits; i++) {
        if(!try_occupy_range(i, i + psize - 1))
            continue;
        goto found;
    }

    for(i = 0; i < lastbit; i++) {
        if(!try_occupy_range(i, i + psize - 1))
            continue;
        goto found;
    }

    /* FIXME: can we just return NULL? User-space
     * applications won't benefit from the kernel
     * straight up shitting itself when they go
     * "please sir I want some more memory!"... */
    panic("pmem: not enough memory (can't allocate %zu pages)", psize);
    unreachable();
    return 0;

found:
    lastbit = i + 1;
    return i * PAGE_SIZE;
}
EXPORT_SYMBOL(pmalloc);

void *pmalloc_hhdm(size_t psize)
{
    uintptr_t pptr = pmalloc(psize);
    if(pptr)
        return (void *)(pptr + get_hhdm_offset());
    return NULL;
}
EXPORT_SYMBOL(pmalloc_hhdm);

void pmfree(uintptr_t pptr, size_t psize)
{
    size_t page = get_page_bit(pptr);
    set_bitmap_range(&bitmap, page, page + psize - 1);
    lastbit = page;
}
EXPORT_SYMBOL(pmfree);

void pmfree_hhdm(void *restrict ptr, size_t psize)
{
    pmfree((uintptr_t)ptr - get_hhdm_offset(), psize);
}
EXPORT_SYMBOL(pmfree_hhdm);

static int init_pmem(void)
{
    size_t a;
    size_t b;
    uint64_t i;
    uintptr_t entry_end;
    uintptr_t hhdm_offset;
    const char *entry_name;
    const struct limine_memmap_response *memmap;
    const struct limine_memmap_entry *entry;

    phys_limit = 0;
    bitmap.data = NULL;
    bitmap.nbits = 0;
    bitmap.size = 0;
    lastbit = 0;
    hhdm_offset = get_hhdm_offset();
    memmap = get_memmap();

    /* Determine physical memory limit */
    for(i = 0; i < memmap->entry_count; i++) {
        entry = memmap->entries[i];
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
                entry_name = "bootloader reclaimable";
                break;
            case LIMINE_MEMMAP_KERNEL_AND_MODULES:
                entry_name = "kernel and modules";
                break;
            case LIMINE_MEMMAP_FRAMEBUFFER:
                entry_name = "framebuffer";
                break;
            default:
                entry_name = "unknown";
                break;
        }

        pr_debug("pmem: %p-%p %s", (void *)entry->base, (void *)entry_end, entry_name);

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
    for(i = 0; i < memmap->entry_count; i++) {
        entry = memmap->entries[i];
        if(entry->type == LIMINE_MEMMAP_USABLE && entry->length >= bitmap.size) {
            bitmap.data = (uint32_t *)(entry->base + hhdm_offset);
            memset(bitmap.data, 0, bitmap.size);
            break;
        }
    }

    panic_if(!bitmap.data, "pmem: not enough memory to initialize");

    /* Mark USABLE pages as free memory */
    for(i = 0; i < memmap->entry_count; i++) {
        entry = memmap->entries[i];
        if(entry->type == LIMINE_MEMMAP_USABLE) {
            a = get_page_bit(entry->base);
            b = get_page_bit(entry->base + entry->length - 1);
            set_bitmap_range(&bitmap, a, b);
        }
    }

    /* Mark bitmap as non-free memory */
    a = get_page_bit((uintptr_t)bitmap.data - hhdm_offset);
    b = get_page_bit((uintptr_t)bitmap.data - hhdm_offset + bitmap.size - 1);
    clear_bitmap_range(&bitmap, a, b);

    /* Set lastbit */
    pmfree(pmalloc(1), 1);

    pr_debug("pmem: bitmap.data=%p", (void *)bitmap.data);
    pr_debug("pmem: bitmap.nbits=%zu", bitmap.nbits);
    pr_debug("pmem: bitmap.size=%zu (%zu KiB)", bitmap.size, bitmap.size / 0x400);
    pr_debug("pmem: phys_limit=%p (%zu MiB)", (void *)phys_limit, (phys_limit + 1) / 0x100000);

    return EOK;
}
initcall_tier_0(pmem, init_pmem);
initcall_depend(pmem, bootinfo);
