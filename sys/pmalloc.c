/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <bitmap.h>
#include <string.h>
#include <sys/boot.h>
#include <sys/debug.h>
#include <sys/pmalloc.h>
#include <sys/kprintf.h>

typedef struct memblock_s {
    struct memblock_s *next;
    bitmap_t bitmap;
    uintptr_t phys_start;
    uintptr_t phys_end;
    size_t lastfree;
} memblock_t;

static memblock_t *blocks = NULL;
static size_t total_memory = 0;
static size_t used_memory = 0;

void add_memory_block(uintptr_t address, size_t length)
{
    size_t blocksize;
    memblock_t *block;

    block = (memblock_t *)(address + hhdm_offset);
    block->bitmap.data = NULL;
    block->phys_start = address;
    block->phys_end = address + length;

    bitmap_init(&block->bitmap, get_page_count(length));

    // For better performance, bitmap needs to be aligned
    // and we can't be sure that the header size is a multiple
    // of uint64_t (I have zero understanding of C struct rules)
    // so it's just better to have the header size aligned.
    blocksize = __align_ceil(sizeof(memblock_t), sizeof(uint64_t));
    block->bitmap.data = (void *)((uintptr_t)block + blocksize);

    // Memory blocks that are only one page in size do not make any sense
    // engineering-wise (hardwiring a chunk of RAM to be just one page,
    // what the fuck) so it's kind of okay for kernel to shit itself if they exist.
    // FIXME: just ignore those blocks instead, shitting itself isn't okay.
    kassert(length >= (blocksize + block->bitmap.size + PAGE_SIZE));

    // Last known to be free page comes right after metadata
    block->lastfree = get_page_count(blocksize + block->bitmap.size);

    memset(block->bitmap.data, 0, block->bitmap.size);
    bitmap_set_range(&block->bitmap, block->lastfree, block->bitmap.nbits - 1);

    total_memory += length;
    used_memory += blocksize + block->bitmap.size;

    block->next = blocks;
    blocks = block;
}

size_t get_total_memory(void)
{
    return total_memory;
}

size_t get_used_memory(void)
{
    return used_memory;
}

static bool try_occupy_range(bitmap_t *restrict bitmap, size_t a, size_t b)
{
    size_t i;
    for(i = a; i <= b; i++) {
        if(!bitmap_read(bitmap, i)) {
            if(i > a)
                bitmap_set_range(bitmap, a, i - 1);
            return false;
        }

        bitmap_clear(bitmap, i);
    }

    return true;
}

uintptr_t pmalloc(size_t npages)
{
    size_t i;
    memblock_t *block;

    for(block = blocks; block; block = block->next) {
        for(i = block->lastfree; i < block->bitmap.nbits; i++) {
            if(try_occupy_range(&block->bitmap, i, i + npages - 1)) {
                block->lastfree = i + 1;
                used_memory += npages * PAGE_SIZE;
                return block->phys_start + i * PAGE_SIZE;
            }
        }

        for(i = 0; i < block->lastfree; i++) {
            if(try_occupy_range(&block->bitmap, i, i + npages - 1)) {
                block->lastfree = i + 1;
                used_memory += npages * PAGE_SIZE;
                return block->phys_start + i * PAGE_SIZE;
            }
        }
    }

    return 0;
}

void *pmalloc_virt(size_t npages)
{
    uintptr_t address = pmalloc(npages);
    if(address)
        return (void *)(address + hhdm_offset);
    return NULL;
}

void pmfree(uintptr_t address, size_t npages)
{
    size_t page;
    memblock_t *block;

    if(address) {
        address = page_align_address(address);

        for(block = blocks; block; block = block->next) {
            if(address >= block->phys_start && address < block->phys_end) {
                page = (address - block->phys_start) / PAGE_SIZE;
                bitmap_clear_range(&block->bitmap, page, page + npages - 1);
                block->lastfree = page;
                used_memory -= npages * PAGE_SIZE;
                return;
            }
        }
    }
}

void pmfree_virt(void *restrict ptr, size_t npages)
{
    if(ptr == NULL)
        return;
    pmfree((uintptr_t)ptr - hhdm_offset, npages);
}
