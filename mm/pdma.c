#include <arch/limits.h>
#include <core/kprintf.h>
#include <core/panic.h>
#include <core/spinlock.h>
#include <init/initcall.h>
#include <lib/bitmap.h>
#include <lib/hrsize.h>
#include <mm/hhdm.h>
#include <mm/memmap.h>
#include <mm/page.h>
#include <mm/pdma.h>

static bitmap_t* pdma_bitmap = nullptr;
static size_t pdma_numpages = 0U;
static size_t pdma_numused = 0U;
static size_t pdma_lastpage = 0U;
static spinlock_type pdma_lock = SPINLOCK_INIT;

struct memmap_info {
    uintptr_t start;
    uintptr_t end;
    size_t length;
    size_t npages;
};

static void get_memmap_info(const struct limine_memmap_entry* restrict entry, struct memmap_info* restrict info)
{
    info->start = align_ceil(entry->base, PAGE_SIZE);
    info->length = align_floor(entry->length, PAGE_SIZE);

    if(info->start == 0U) {
        info->start += PAGE_SIZE;
        info->length -= PAGE_SIZE;
    }

    info->end = info->start + info->length - 1U;
    info->npages = page_count(info->length);
}

static void add_region(uintptr_t start, size_t length)
{
    size_t page = start / PAGE_SIZE;
    size_t npages = page_count(length);

    bitmap_range_set(pdma_bitmap, page, page + npages - 1U);

    pr_verbose("pdma: adding region: start=%p end=%p", (void*)(start), (void*)(start + length - 1U));
}

uintptr_t pdma_alloc(size_t numpages)
{
    size_t page;
    size_t maxpage;

    spinlock_acquire(&pdma_lock);

    maxpage = pdma_numpages - numpages;
    for(page = pdma_lastpage; page < maxpage; page += 1U) {
        if(bitmap_range_isset(pdma_bitmap, page, page + numpages - 1U)) {
            bitmap_range_clear(pdma_bitmap, page, page + numpages - 1U);
            pdma_lastpage = page + numpages;
            pdma_numused += numpages;

            spinlock_release(&pdma_lock);

            return page * PAGE_SIZE;
        }
    }

    maxpage = pdma_lastpage - numpages;
    for(page = 0U; page < maxpage; page += 1U) {
        if(bitmap_range_isset(pdma_bitmap, page, page + numpages - 1U)) {
            bitmap_range_clear(pdma_bitmap, page, page + numpages - 1U);
            pdma_lastpage = page + numpages;
            pdma_numused += numpages;

            spinlock_release(&pdma_lock);

            return page * PAGE_SIZE;
        }
    }

    spinlock_release(&pdma_lock);

    return 0U;
}

void* pdma_alloc_virt(size_t numpages)
{
    uintptr_t address = pdma_alloc(numpages);
    if(address == 0U)
        return nullptr;
    return phys_to_virt(address);
}

void pdma_free(uintptr_t physaddr, size_t numpages)
{
    size_t page = physaddr / PAGE_SIZE;

    spinlock_acquire(&pdma_lock);

    if(physaddr) {
        bitmap_range_set(pdma_bitmap, page, page + numpages - 1U);
        pdma_numused -= numpages;
        pdma_lastpage = page;
    }

    spinlock_release(&pdma_lock);
}

void pdma_free_virt(void* restrict virtaddr, size_t numpages)
{
    if(virtaddr) {
        pdma_free(virt_to_phys(virtaddr), numpages);
    }
}

void pdma_get_stats(struct pdma_stats* stats)
{
    stats->total_bytes = pdma_numpages * PAGE_SIZE;
    stats->total_pages = pdma_numpages;
    stats->used_bytes = pdma_numused * PAGE_SIZE;
    stats->used_pages = pdma_numused;
}

define_initcall(pdma, INITCALL_STAGE_BSP_PMM, INITCALL_ORDER_FIRST)
{
    size_t bitmap_npages = 0U;

    for(size_t i = 0U; i < memmap->entry_count; ++i) {
        const struct limine_memmap_entry* entry = memmap->entries[i];

        if(entry->type == LIMINE_MEMMAP_USABLE) {
            struct memmap_info info;
            get_memmap_info(entry, &info);

            if(info.start < PDMA_APPROX_END) {
                if(info.end >= PDMA_APPROX_END) {
                    info.end = PDMA_APPROX_END;
                    info.length = info.end - info.start + 1U;
                    info.npages = page_count(info.length);
                }

                if((info.end / PAGE_SIZE + 1U) > bitmap_npages) {
                    bitmap_npages = info.end / PAGE_SIZE + 1U;
                }
            }
        }
    }

    pdma_numpages = align_ceil(bitmap_npages, BITMAP_CHUNK_BITS);

    size_t bitmap_size = bitmap_bytecount(pdma_numpages);
    bitmap_npages = page_count(bitmap_size);
    bitmap_size = bitmap_npages * PAGE_SIZE;

    uintptr_t bitmap_phys = 0U;

    for(size_t i = 0U; i < memmap->entry_count; ++i) {
        const struct limine_memmap_entry* entry = memmap->entries[i];

        if(entry->type == LIMINE_MEMMAP_USABLE) {
            struct memmap_info info;
            get_memmap_info(entry, &info);

            if(info.npages >= bitmap_npages && info.start + bitmap_size <= PDMA_APPROX_END) {
                bitmap_phys = info.start;
                pdma_bitmap = phys_to_virt(bitmap_phys);
                add_region(bitmap_phys, bitmap_size);
                break;
            }
        }
    }

    if(pdma_bitmap == nullptr) {
        panic("pdma: out of memory");
    }

    bitmap_range_clear(pdma_bitmap, 0U, pdma_numpages - 1U);

    for(size_t i = 0U; i < memmap->entry_count; ++i) {
        const struct limine_memmap_entry* entry = memmap->entries[i];

        if(entry->type == LIMINE_MEMMAP_USABLE) {
            struct memmap_info info;
            get_memmap_info(entry, &info);

            if(info.start >= PDMA_APPROX_END)
                continue;

            if(info.end >= PDMA_APPROX_END) {
                info.end = PDMA_APPROX_END;
                info.length = info.end - info.start + 1U;
            }

            if(info.start == bitmap_phys) {
                info.start += bitmap_size;
                info.length -= bitmap_size;
            }

            if(info.length > 0U) {
                add_region(info.start, info.length);
            }
        }
    }

    struct pdma_stats stats;
    pdma_get_stats(&stats);

    char hrsize_buffer[128];
    hrsize_bin(hrsize_buffer, sizeof(hrsize_buffer), stats.total_bytes);
    pr_verbose("pdma: tracking %s in %zu pages", hrsize_buffer, stats.total_pages);
}
