/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <kernel/boot.h>
#include <kernel/page.h>
#include <kernel/pmm.h>
#include <kernel/systm.h>
#include <stdbool.h>
#include <string.h>

#define LOWMEM_SIZE_APPROX 0x4000000

static void **pagelist = NULL;
static uintptr_t lowmem_end = 0;
static uint64_t *bitmap = 0;
static size_t numpages = 0;
static size_t lastpage = 0;

static void *memmap_alloc(size_t n)
{
    size_t i;
    size_t npages = get_page_count(n);
    struct limine_memmap_entry *entry;
    uintptr_t address;

    for(i = 0; i < memmap_request.response->entry_count; ++i) {
        entry = memmap_request.response->entries[i];

        if(entry->type == LIMINE_MEMMAP_USABLE && entry->length >= (npages * PAGE_SIZE)) {
            address = entry->base;
            entry->base += npages * PAGE_SIZE;
            entry->length -= npages * PAGE_SIZE;
            return (void *)(address + highmem);
        }
    }

    return NULL;
}

static void bitmap_mark_free(size_t a, size_t b)
{
    size_t i;
    size_t ax = __align_ceil(a, 64);
    size_t bx = __align_floor(b, 64);
    size_t axi = ax / 64;
    size_t bxi = bx / 64;
    for(i = a; i < ax; bitmap[axi - 1] |= (UINT64_C(1) << (i++ % 64)));
    if(bxi >= axi)
        for(i = bx + 1; i <= b; bitmap[bxi] |= (UINT64_C(1) << (i++ % 64)));
    for(i = axi; i < bxi; bitmap[i++] = UINT64_C(0xFFFFFFFFFFFFFFFF));
}

static void bitmap_mark_used(size_t a, size_t b)
{
    size_t i;
    size_t ax = __align_ceil(a, 64);
    size_t bx = __align_floor(b, 64);
    size_t axi = ax / 64;
    size_t bxi = bx / 64;
    for(i = a; i < ax; bitmap[axi - 1] &= ~(UINT64_C(1) << (i++ % 64)));
    if(bxi >= axi)
        for(i = bx + 1; i <= b; bitmap[bxi] &= ~(UINT64_C(1) << (i++ % 64)));
    for(i = axi; i < bxi; bitmap[i++] = UINT64_C(0x0000000000000000));
}

static bool bitmap_probe(size_t a, size_t b)
{
    size_t i;
    size_t chunk;
    uint64_t mask;

    for(i = a; i <= b; ++i) {
        mask = (UINT64_C(1) << (i % 64));
        chunk = (i / 64);

        if(bitmap[chunk] & mask) {
            bitmap[chunk] &= ~mask;
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
    size_t i;

    uintptr_t address;

    if(npages == 1 && pagelist) {
        address = ((uintptr_t)pagelist - highmem);
        pagelist = pagelist[0];
        return address;
    }

    for(i = lastpage; i < numpages - npages; ++i) {
        if(bitmap_probe(i, i + npages - 1)) {
            lastpage = i + npages;
            return i * PAGE_SIZE;
        }
    }

    for(i = 0; i < lastpage - npages; ++i) {
        if(bitmap_probe(i, i + npages - 1)) {
            lastpage = i + npages;
            return i * PAGE_SIZE;
        }
    }

    return 0;
}

void *pmm_alloc_highmem(size_t npages)
{
    uintptr_t address;
    if((address = pmm_alloc(npages)) != 0)
        return (void *)(address + highmem);
    return 0;
}

void pmm_free(uintptr_t address, size_t npages)
{
    size_t page;
    void **head_ptr;

    if(address != 0) {
        if(npages == 1 && address >= lowmem_end) {
            head_ptr = (void **)(address + highmem);
            head_ptr[0] = pagelist;
            pagelist = head_ptr;
            return;
        }

        page = address / PAGE_SIZE;
        bitmap_mark_free(page, page + npages - 1);
        lastpage = page;
    }
}

void pmm_free_highmem(void *restrict ptr, size_t npages)
{
    if(ptr == NULL)
        return;
    pmm_free(((uintptr_t)ptr - highmem), npages);
}

static void init_pmm(void)
{
    size_t i;
    size_t page;
    size_t npages;
    size_t bmsize;
    uintptr_t endptr, j;
    struct limine_memmap_entry *entry;
    void **head_ptr;

    for(i = 0; i < memmap_request.response->entry_count; ++i) {
        entry = memmap_request.response->entries[i];

        if(entry->type == LIMINE_MEMMAP_USABLE) {
            endptr = entry->base + entry->length - 1;
            if(endptr > lowmem_end)
                lowmem_end = endptr;
            if(endptr < LOWMEM_SIZE_APPROX)
                continue;
            break;
        }
    }

    npages = get_page_count(lowmem_end + 1);
    numpages = __align_ceil(npages, 64);
    bmsize = numpages / 8;

    bitmap = memmap_alloc(bmsize);
    kassertf(bitmap, "pmm: insufficient memory");
    bitmap_mark_used(0, numpages - 1);
    lastpage = numpages - 32;

    for(i = 0; i < memmap_request.response->entry_count; ++i) {
        entry = memmap_request.response->entries[i];

        if(entry->type == LIMINE_MEMMAP_USABLE) {
            npages = get_page_count(entry->length);
            page = entry->base / PAGE_SIZE;
            bitmap_mark_free(page, page + npages - 1);

            endptr = entry->base + entry->length - 1;
            if(endptr < LOWMEM_SIZE_APPROX)
                continue;
            break;
        }
    }

    for(i = 0; i < memmap_request.response->entry_count; ++i) {
        entry = memmap_request.response->entries[i];

        if(entry->type == LIMINE_MEMMAP_USABLE && entry->base >= lowmem_end) {
            for(j = 0; j < entry->length; j += PAGE_SIZE) {
                head_ptr = (void **)(entry->base + j + highmem);
                head_ptr[0] = pagelist;
                pagelist = head_ptr;
            }
        }
    }
}
core_initcall(pmm, init_pmm);
