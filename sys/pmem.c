/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <stdbool.h>
#include <sys/page.h>
#include <sys/pmem.h>
#include <sys/systm.h>

typedef struct memblock_s {
    struct memblock_s *next;
    unsigned short type;
    uintptr_t start;
    uintptr_t end;
    size_t last_free;
    size_t bitmap_bits;
    size_t bitmap_size;
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

void pmem_add_memblock(uintptr_t address, size_t npages)
{
    size_t blocksize;
    memblock_t *block;

    block = (memblock_t *)(address + highmem_offset);
    block->start = address;
    block->end = address + npages * PAGE_SIZE;
    block->bitmap_bits = __align_ceil(npages, 64);
    block->bitmap_size = block->bitmap_bits / 8;

    blocksize = __align_ceil(sizeof(memblock_t), sizeof(uint64_t));
    block->bitmap = (uint64_t *)(address + highmem_offset + blocksize);
    block->last_free = get_page_count(blocksize + block->bitmap_size);

    kassert((npages * PAGE_SIZE) >= (blocksize + block->bitmap_size));

    /* Pages in a bitmap may not be present physically since the
     * bitmap's floor of size is 64 (it uses 64-bit chunks), so
     * when initializing, all the pages are guilty until proven innocent. */
    clear_range(block, 0, block->bitmap_bits - 1);
    set_range(block, block->last_free, npages - 1);

    total_memory += npages * PAGE_SIZE;
    used_memory += blocksize + block->bitmap_size;

    block->next = memblocks;
    memblocks = block;
}

size_t pmem_get_total_memory(void)
{
    return total_memory;
}

size_t pmem_get_used_memory(void)
{
    return used_memory;
}

uintptr_t pmem_alloc(size_t npages)
{
    size_t i;
    memblock_t *block;

    for(block = memblocks; block; block = block->next) {
        for(i = block->last_free; i < block->bitmap_bits; i++) {
            if(try_occupy_range(block, i, i + npages - 1)) {
                block->last_free = i + 1;
                used_memory += npages * PAGE_SIZE;
                return block->start + i * PAGE_SIZE;
            }
        }

        for(i = 0; i < block->last_free; i++) {
            if(try_occupy_range(block, i, i + npages - 1)) {
                block->last_free = i + 1;
                used_memory += npages * PAGE_SIZE;
                return block->start + i * PAGE_SIZE;
            }
        }
    }

    return 0;
}

void *pmem_alloc_highmem(size_t npages)
{
    uintptr_t address = pmem_alloc(npages);
    if(address)
        return (void *)(address + highmem_offset);
    return NULL;
}

void pmem_free(uintptr_t address, size_t npages)
{
    size_t page;
    memblock_t *block;

    if(address) {
        address = page_align_address(address);

        for(block = memblocks; block; block = block->next) {
            if(address >= block->start && address < block->end) {
                page = (address - block->start) / PAGE_SIZE;
                clear_range(block, page, page + npages - 1);
                block->last_free = page;
                used_memory -= npages * PAGE_SIZE;
                return;
            }
        }
    }
}

void pmem_free_highmem(void *restrict ptr, size_t npages)
{
    if(ptr == NULL)
        return;
    pmem_free(((uintptr_t)ptr - highmem_offset), npages);
}
