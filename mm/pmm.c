/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <mm/align.h>
#include <mm/pmm.h>
#include <stdbool.h>
#include <string.h>
#include <sys/boot.h>
#include <sys/panic.h>
#include <sys/printf.h>

/* PMM separates memory into two areas:
 *
 * 1.   The DMA memory is anything that ends at
 *      about DMA_APPROX_END (64 MiB). This memory
 *      is managed by a simple bitmap to allow for
 *      allocating contiguous memory for DMA operations.
 * 
 * 2.   General-purpose memory is anything that comes
 *      after about DMA_APPROX_END. This memory is
 *      managed by an even simpler linked list, allowing
 *      for efficient allocation of single-page blocks.
 *      If this memory area is exhausted, PMM falls back
 *      to using DMA pages; same happens when there's less
 *      than 64 MiB of system memory installed.
 */

#define DMA_APPROX_END 0x3FFFFFF

static void **page_list = NULL;
static uintptr_t dma_end = 0;
static uint64_t *dma_bitmap = NULL;
static size_t dma_numpages = 0;
static size_t dma_lastpage = 0;

static void bitmap_mark_free(size_t a, size_t b)
{
    size_t i;
    size_t ax = __align_ceil(a, 64);
    size_t bx = __align_floor(b, 64);
    size_t axi = ax / 64;
    size_t bxi = bx / 64;
    for(i = a; i < ax; dma_bitmap[axi - 1] |= (UINT64_C(1) << (i++ % 64)));
    if(bxi >= axi)
        for(i = bx + 1; i <= b; dma_bitmap[bxi] |= (UINT64_C(1) << (i++ % 64)));
    for(i = axi; i < bxi; dma_bitmap[i++] = UINT64_C(0xFFFFFFFFFFFFFFFF));
}

static void bitmap_mark_used(size_t a, size_t b)
{
    size_t i;
    size_t ax = __align_ceil(a, 64);
    size_t bx = __align_floor(b, 64);
    size_t axi = ax / 64;
    size_t bxi = bx / 64;
    for(i = a; i < ax; dma_bitmap[axi - 1] &= ~(UINT64_C(1) << (i++ % 64)));
    if(bxi >= axi)
        for(i = bx + 1; i <= b; dma_bitmap[bxi] &= ~(UINT64_C(1) << (i++ % 64)));
    for(i = axi; i < bxi; dma_bitmap[i++] = UINT64_C(0x0000000000000000));
}

static bool bitmap_try_mark_used(size_t a, size_t b)
{
    size_t i;
    size_t chunk;
    uint64_t mask;

    for(i = a; i <= b; ++i) {
        mask = (UINT64_C(1) << (i % 64));
        chunk = (i / 64);

        if(dma_bitmap[chunk] & mask) {
            dma_bitmap[chunk] &= ~mask;
            continue;
        }

        if(i > a)
            bitmap_mark_free(a, i - 1);
        return false;
    }

    return true;
}

uintptr_t pmm_alloc(size_t npages)
{
    size_t page;
    size_t limit;
    uintptr_t address;

    if(npages == 1 && page_list) {
        address = ((uintptr_t)page_list - hhdm_offset);
        page_list = page_list[0];
        return address;
    }

    limit = dma_numpages - npages;
    for(page = dma_lastpage; page < limit; ++page) {
        if(bitmap_try_mark_used(page, page + npages - 1)) {
            dma_lastpage = page + npages;
            return page * PAGE_SIZE;
        }
    }

    limit = dma_lastpage - npages;
    for(page = 0; page < limit; ++page) {
        if(bitmap_try_mark_used(page, page + npages - 1)) {
            dma_lastpage = page + npages;
            return page * PAGE_SIZE;
        }
    }

    return 0;
}

void *pmm_alloc_hhdm(size_t npages)
{
    uintptr_t address;
    if((address = pmm_alloc(npages)) != 0)
        return (void *)(address + hhdm_offset);
    return 0;
}

void pmm_free(uintptr_t address, size_t npages)
{
    size_t page;
    void **head_ptr;

    if(address != 0) {
        if(npages == 1 && address >= dma_end) {
            head_ptr = (void **)(address + hhdm_offset);
            head_ptr[0] = page_list;
            page_list = head_ptr;
            return;
        }

        page = address / PAGE_SIZE;
        bitmap_mark_free(page, page + npages - 1);
        dma_lastpage = page;
    }
}

void pmm_free_hhdm(void *restrict ptr, size_t npages)
{
    if(ptr == NULL)
        return;
    pmm_free(((uintptr_t)ptr - hhdm_offset), npages);
}

static void init_pmm(void)
{
    size_t i;
    size_t page;
    size_t nb, npages;
    uintptr_t end_addr, off;
    void **head_ptr;
    struct limine_memmap_entry *entry;

    page_list = NULL;
    dma_end = DMA_APPROX_END;
    dma_bitmap = NULL;
    dma_numpages = 0;
    dma_lastpage = 0;

    /* Determine the actual dma_end */
    for(i = 0; i < memmap_request.response->entry_count; ++i) {
        entry = memmap_request.response->entries[i];

        if(entry->type == LIMINE_MEMMAP_USABLE && entry->base <= DMA_APPROX_END) {
            end_addr = entry->base + entry->length - 1;
            if(dma_end < end_addr)
                dma_end = end_addr;
            break;
        }
    }

    npages = get_page_count(dma_end + 1);
    dma_numpages = __align_ceil(npages, 64);
    nb = dma_numpages / 8;

    /* Bitmap's size in pages */
    npages = get_page_count(nb);

    /* Find a place for the bitmap */
    for(i = 0; i < memmap_request.response->entry_count; ++i) {
        entry = memmap_request.response->entries[i];

        if(entry->type == LIMINE_MEMMAP_USABLE && entry->length >= npages) {
            dma_bitmap = (uint64_t *)(entry->base + hhdm_offset);
            entry->base += npages * PAGE_SIZE;
            entry->length -= npages * PAGE_SIZE;
        }
    }

    kassertf(dma_bitmap, "pmm: insufficient memory");
    bitmap_mark_used(0, dma_numpages - 1);

    /* Mark blocks within DMA space as free memory */
    for(i = 0; i < memmap_request.response->entry_count; ++i) {
        entry = memmap_request.response->entries[i];

        if(entry->type == LIMINE_MEMMAP_USABLE && entry->base <= DMA_APPROX_END && entry->length != 0) {
            npages = get_page_count(entry->length);
            page = entry->base / PAGE_SIZE;
            bitmap_mark_free(page, page + npages - 1);
            break;
        }
    }

    /* Initialize anything else as a linked list */
    for(i = 0; i < memmap_request.response->entry_count; ++i) {
        entry = memmap_request.response->entries[i];

        if(entry->type == LIMINE_MEMMAP_USABLE && entry->base > dma_end) {
            for(off = 0; off < entry->length; off += PAGE_SIZE) {
                head_ptr = (void **)(entry->base + off + hhdm_offset);
                head_ptr[0] = page_list;
                page_list = head_ptr;
            }
        }
    }
}
core_initcall(pmm, init_pmm);
