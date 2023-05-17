/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <kan/errno.h>
#include <kan/pmem.h>
#include <kan/system.h>
#include <stdbool.h>

typedef struct memblock_s {
    struct memblock_s *next;
    unsigned short zone_bits;
    uintptr_t phys_start;
    uintptr_t phys_end;
    size_t free_page;
    size_t page_count;
    uint64_t *bitmap;
} memblock_t;

static memblock_t *memblocks = NULL;
static size_t total_memory = 0;
static size_t used_memory = 0;

static void clear_range(memblock_t *restrict block, size_t a, size_t b)
{
    size_t i;
    size_t ax = __align_ceil(a, 64);
    size_t bx = __align_floor(b, 64);
    size_t axi = ax / 64;
    size_t bxi = bx / 64;
    for(i = a; i < ax; block->bitmap[axi - 1] &= ~(1 << (i++ % 64)));
    if(bxi >= axi)
        for(i = bx + 1; i <= b; block->bitmap[bxi] &= ~(1 << (i++ % 64)));
    for(i = axi; i < bxi; block->bitmap[i++] = UINT64_C(0x0000000000000000));
}

static void set_range(memblock_t *restrict block, size_t a, size_t b)
{
    size_t i;
    size_t ax = __align_ceil(a, 64);
    size_t bx = __align_floor(b, 64);
    size_t axi = ax / 64;
    size_t bxi = bx / 64;
    for(i = a; i < ax; block->bitmap[axi - 1] |= (1 << (i++ % 64)));
    if(bxi >= axi)
        for(i = bx + 1; i <= b; block->bitmap[bxi] |= (1 << (i++ % 64)));
    for(i = axi; i < bxi; block->bitmap[i++] = UINT64_C(0xFFFFFFFFFFFFFFFF));
}

static bool try_occupy_range(memblock_t *restrict block, size_t a, size_t b)
{
    size_t i;
    size_t j;
    uint64_t mask;

    for(i = a; i <= b; ++i) {
        j = (i / 64);
        mask = (1 << (i % 64));

        if(block->bitmap[j] & mask) {
            block->bitmap[j] &= ~mask;
            continue;
        }

        if(i > a)
            set_range(block, a, i - 1);
        return false;
    }

    return true;
}

int add_memblock(uintptr_t address, size_t npages, unsigned int zone_bits)
{
    size_t blocksize;
    size_t bitmap_size;
    memblock_t *block;

    block = (memblock_t *)(address + hhdm_offset);
    block->zone_bits = zone_bits;
    block->phys_start = address;
    block->phys_end = address + npages * PAGE_SIZE;
    block->page_count = __align_ceil(npages, 64);
    bitmap_size = block->page_count / 8;

    blocksize = __align_ceil(sizeof(memblock_t), sizeof(uint64_t));
    block->bitmap = (uint64_t *)(address + hhdm_offset + blocksize);
    block->free_page = get_page_count(blocksize + bitmap_size);

    kassert((npages * PAGE_SIZE) >= (blocksize + bitmap_size));

    /* Pages in a bitmap may not be present physically since the
     * bitmap's floor of size is 64 (it uses 64-bit chunks), so when
     * initializing, all the pages are guilty until proven innocent. */
    clear_range(block, 0, block->page_count - 1);
    set_range(block, block->free_page, npages - 1);

    total_memory += npages * PAGE_SIZE;
    used_memory += blocksize + bitmap_size;

    block->next = memblocks;
    memblocks = block;

    return 0;
}

size_t get_total_memory(void)
{
    return total_memory;
}

size_t get_used_memory(void)
{
    return used_memory;
}

uintptr_t pmalloc(size_t npages, unsigned short zone)
{
    size_t i;
    memblock_t *block;

    for(block = memblocks; block; block = block->next) {
        if(block->zone_bits & ZONE_BIT(zone)) {
            for(i = block->free_page; i < block->page_count; i++) {
                if(try_occupy_range(block, i, i + npages - 1)) {
                    block->free_page = i + 1;
                    used_memory += npages * PAGE_SIZE;
                    return block->phys_start + i * PAGE_SIZE;
                }
            }

            for(i = 0; i < block->free_page; i++) {
                if(try_occupy_range(block, i, i + npages - 1)) {
                    block->free_page = i + 1;
                    used_memory += npages * PAGE_SIZE;
                    return block->phys_start + i * PAGE_SIZE;
                }
            }
        }
    }

    return 0;
}

void pmfree(uintptr_t address, size_t npages)
{
    size_t page;
    memblock_t *block;

    if(address) {
        address = page_align_address(address);

        for(block = memblocks; block; block = block->next) {
            if(address >= block->phys_start && address < block->phys_end) {
                page = (address - block->phys_start) / PAGE_SIZE;
                clear_range(block, page, page + npages - 1);
                block->free_page = page;
                used_memory -= npages * PAGE_SIZE;
                return;
            }
        }
    }
}

void *pmalloc_hhdm(size_t npages, unsigned short zone)
{
    uintptr_t address = pmalloc(npages, zone);
    if(address)
        return (void *)(address + hhdm_offset);
    return NULL;
}

void pmfree_hhdm(void *restrict ptr, size_t npages)
{
    if(ptr == NULL)
        return;
    pmfree(((uintptr_t)ptr - hhdm_offset), npages);
}
