/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <bitmap.h>
#include <kan/errno.h>
#include <machine/boot.h>
#include <machine/page.h>
#include <string.h>
#include <sys/debug.h>
#include <sys/pmem.h>

typedef struct pmem_block_s {
    struct pmem_block_s *pm_next;
    bitmap_t pm_bitmap;
    uintptr_t pm_start_phys;
    uintptr_t pm_end_phys;
    size_t pm_last_free_page;
} pmem_block_t;

static pmem_block_t *pmem_blocks = NULL;
static size_t pmem_total_memory = 0;
static size_t pmem_used_memory = 0;

int pmem_add_memblock(uintptr_t address, size_t n)
{
    size_t npages = get_page_count(n);
    size_t blocksize;
    pmem_block_t *block;

    block = (pmem_block_t *)(address + hhdm_offset);
    block->pm_bitmap.bm_data = NULL;
    block->pm_start_phys = address;
    block->pm_end_phys = address + (npages * PAGE_SIZE);

    bitmap_init(&block->pm_bitmap, npages);

    // For potentially better performance the bitmap must
    // be aligned to a boundary of sorts and uint64_t seems
    // like a good idea since bitmap_t manages bits in chunks
    // of that specific size, so we find the first place within
    // the memory block that allows for that bitmap placement.
    blocksize = __align_ceil(sizeof(pmem_block_t), sizeof(uint64_t));
    block->pm_bitmap.bm_data = (uint64_t *)((uintptr_t)block + blocksize);

    // Memory block must be able to fit the header plus the bitmap
    kassert((npages * PAGE_SIZE) >= (blocksize + block->pm_bitmap.bm_size));

    // Last page known to be free comes right after the bitmap's page
    block->pm_last_free_page = get_page_count(blocksize + block->pm_bitmap.bm_size);

    // bitmap_init() has possibly aligned npages value to be a multiple
    // of 64 (since bitmap cannot hold less than 64 bits by design), so
    // we first assume all pages are non-present or occupied and then mark
    // actually present and free pages as free to use.
    bitmap_clear_range(&block->pm_bitmap, 0, block->pm_bitmap.bm_nbits - 1);
    bitmap_set_range(&block->pm_bitmap, block->pm_last_free_page, npages - 1);

    pmem_total_memory += (npages * PAGE_SIZE);
    pmem_used_memory += (blocksize + block->pm_bitmap.bm_size);

    block->pm_next = pmem_blocks;
    pmem_blocks = block;

    return 0;
}

size_t pmem_get_total_memory(void)
{
    return pmem_total_memory;
}

size_t pmem_get_used_memory(void)
{
    return pmem_used_memory;
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

uintptr_t pmem_alloc(size_t npages)
{
    size_t i;
    pmem_block_t *block;

    for(block = pmem_blocks; block; block = block->pm_next) {
        for(i = block->pm_last_free_page; i < block->pm_bitmap.bm_nbits; i++) {
            if(try_occupy_range(&block->pm_bitmap, i, i + npages - 1)) {
                block->pm_last_free_page = i + 1;
                pmem_used_memory += npages * PAGE_SIZE;
                return block->pm_start_phys + i * PAGE_SIZE;
            }
        }

        for(i = 0; i < block->pm_last_free_page; i++) {
            if(try_occupy_range(&block->pm_bitmap, i, i + npages - 1)) {
                block->pm_last_free_page = i + 1;
                pmem_used_memory += npages * PAGE_SIZE;
                return block->pm_start_phys + i * PAGE_SIZE;
            }
        }
    }

    return 0;
}

void *pmem_alloc_virt(size_t npages)
{
    uintptr_t address = pmem_alloc(npages);
    if(address)
        return (void *)(address + hhdm_offset);
    return NULL;
}

void pmem_free(uintptr_t address, size_t npages)
{
    size_t page;
    pmem_block_t *block;

    if(address) {
        address = page_align_address(address);

        for(block = pmem_blocks; block; block = block->pm_next) {
            if(address >= block->pm_start_phys && address < block->pm_end_phys) {
                page = (address - block->pm_start_phys) / PAGE_SIZE;
                bitmap_clear_range(&block->pm_bitmap, page, page + npages - 1);
                block->pm_last_free_page = page;
                pmem_used_memory -= npages * PAGE_SIZE;
                return;
            }
        }
    }
}

void pmem_free_virt(void *restrict ptr, size_t npages)
{
    if(ptr == NULL)
        return;
    pmem_free(((uintptr_t)ptr - hhdm_offset), npages);
}

// iterate_memmap callback
static void pmem_iterate_memmap(uintptr_t address, size_t n, unsigned short type, void *restrict arg)
{
    if(type == MEMMAP_USABLE) {
        // FIXME: reclaim memory in the same way
        pmem_add_memblock(address, n);
    }
}

static void init_pmem(void)
{
    iterate_memmap(&pmem_iterate_memmap, NULL);
}
early_initcall(pmem, init_pmem);
initcall_dependency(pmem, boot);
