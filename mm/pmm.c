/* SPDX-License-Identifier: Zlib */
#include <bitmap.h>
#include <kern/assert.h>
#include <kern/panic.h>
#include <kern/printf.h>
#include <mm/hhdm.h>
#include <mm/memmap.h>
#include <mm/page.h>
#include <mm/pmm.h>
#include <string.h>
#include <strings.h>

static void **page_list = NULL;
static uintptr_t dma_end_addr = 0;
static bitmap_t *dma_bitmap = NULL;
static size_t dma_numpages = 0;
static size_t dma_lastpage = 0;

uintptr_t dma_alloc(size_t npages)
{
    size_t page;
    size_t maxpage;

    maxpage = dma_numpages - npages;
    for(page = dma_lastpage; page < maxpage; ++page) {
        if(bitmap_range_isset(dma_bitmap, page, page + npages - 1)) {
            bitmap_range_clear(dma_bitmap, page, page + npages - 1);
            dma_lastpage = page + npages;
            return page * PAGE_SIZE;
        }
    }

    maxpage = dma_lastpage - npages;
    for(page = 0; page < maxpage; ++page) {
        if(bitmap_range_isset(dma_bitmap, page, page + npages - 1)) {
            bitmap_range_clear(dma_bitmap, page, page + npages - 1);
            dma_lastpage = page + npages;
            return page * PAGE_SIZE;
        }
    }

    return 0;
}

void *dma_alloc_hhdm(size_t npages)
{
    uintptr_t address;
    if((address = dma_alloc(npages)) != 0)
        return phys_to_hhdm(address);
    return NULL;
}

void dma_free(uintptr_t address, size_t npages)
{
    size_t page = address / PAGE_SIZE;

    kassert((address) < dma_end_addr);
    kassert((page + npages) < dma_numpages);

    if(address != 0) {
        bitmap_range_set(dma_bitmap, page, page + npages - 1);
        dma_lastpage = page;
    }
}

void dma_free_hhdm(void *restrict ptr, size_t npages)
{
    if(ptr == NULL)
        return;
    dma_free(hhdm_to_phys(ptr), npages);
}

uintptr_t pmm_alloc(void)
{
    uintptr_t address;

    if(page_list) {
        address = hhdm_to_phys(page_list);
        page_list = page_list[0];
        return address;
    }

    /* Fall back to the bitmap allocator in case the linked
     * list allocator runs out or if there was not enough
     * memory to initialize it in the first place */
    return dma_alloc(1);
}

void *pmm_alloc_hhdm(void)
{
    uintptr_t address;
    if((address = pmm_alloc()) != 0)
        return phys_to_hhdm(address);
    return NULL;
}

void pmm_free(uintptr_t address)
{
    void **headptr;

    if(address >= dma_end_addr) {
        headptr = phys_to_hhdm(address);
        headptr[0] = page_list;
        page_list = headptr;
        return;
    }

    dma_free(address, 1);
}

void pmm_free_hhdm(void *restrict ptr)
{
    if(ptr == NULL)
        return;
    pmm_free(hhdm_to_phys(ptr));
}

void init_pmm(void)
{
    size_t i;
    size_t page, npages;
    size_t bitmap_size;
    size_t list_numpages;
    uintptr_t address;
    uintptr_t bitmap_phys;
    void **head_ptr;
    struct limine_memmap_entry *entry;

    /* Determine the actual end of the DMA space */
    for(i = 0; i < memmap.response->entry_count; ++i) {
        entry = memmap.response->entries[i];

        if((entry->type == LIMINE_MEMMAP_USABLE) && (entry->base <= DMA_APPROX_END)) {
            address = entry->base + entry->length - 1;

            if(dma_end_addr >= address)
                continue;
            dma_end_addr = address;
        }
    }

    npages = page_count(dma_end_addr + 1);
    dma_numpages = align_ceil(npages, BITMAP_CHUNK_BITS);
    bitmap_size = bitmap_bytecount(dma_numpages);
    npages = page_count(bitmap_size);

    dma_bitmap = NULL;
    bitmap_phys = 0;

    /* Figure out where to put the bitmap */
    for(i = 0; i < memmap.response->entry_count; ++i) {
        entry = memmap.response->entries[i];

        if((entry->type == LIMINE_MEMMAP_USABLE) && (entry->length >= npages)) {
            dma_bitmap = phys_to_hhdm(entry->base);
            bitmap_phys = entry->base;
            break;
        }
    }

    if(!dma_bitmap) {
        panic("pmm: out of memory");
        unreachable();
    }

    /* FIXME: use memset/bzero instead for faster
     * initiailzation? Filling out the linked list takes
     * much more time in cases it even exists though */
    bitmap_range_clear(dma_bitmap, 0, dma_numpages - 1);

    /* Figure out what chunks of DMA space are usable */
    for(i = 0; i < memmap.response->entry_count; ++i) {
        entry = memmap.response->entries[i];

        if(entry->type == LIMINE_MEMMAP_USABLE) {
            if((entry->base <= DMA_APPROX_END) && (entry->length != 0)) {
                npages = page_count(entry->length);
                page = entry->base / PAGE_SIZE;
                bitmap_range_set(dma_bitmap, page, page + npages - 1);
                continue;
            }
        }
    }

    /* Account for cases when the bitmap resides
     * in the very same memory region it tracks. */
    if(bitmap_phys <= dma_end_addr) {
        page = bitmap_phys / PAGE_SIZE;
        npages = page_count(bitmap_size);
        bitmap_range_clear(dma_bitmap, page, page + npages - 1);
    }

    list_numpages = 0;

    /* Figure out what pages belong to the linked list */
    for(i = 0; i < memmap.response->entry_count; ++i) {
        entry = memmap.response->entries[i];

        if((entry->type == LIMINE_MEMMAP_USABLE) && (entry->base > dma_end_addr)) {
            for(address = 0; address < entry->length; address += PAGE_SIZE) {
                head_ptr = phys_to_hhdm(entry->base + address);
                head_ptr[0] = page_list;
                page_list = head_ptr;
                list_numpages += 1;
            }
        }
    }

    kprintf(KP_DEBUG, "pmm: dma_end_addr=%p", (void *)dma_end_addr);
    kprintf(KP_DEBUG, "pmm: dma_bitmap=%p bitmap_size=%zu", (void *)dma_bitmap, bitmap_size);
    kprintf(KP_INFORM, "pmm: dma_bitmap: tracking %zu pages", dma_numpages);
    kprintf(KP_INFORM, "pmm: page_list: tracking %zu pages", list_numpages);
}
