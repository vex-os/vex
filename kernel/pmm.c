#include <vex/assert.h>
#include <vex/bitmap.h>
#include <vex/hhdm.h>
#include <vex/hrsize.h>
#include <vex/kprintf.h>
#include <vex/memmap.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/pmm.h>
#include <vex/sprintf.h>
#include <vex/string.h>
#include <vex/strings.h>

#ifndef CONF_PMM_DMA_SPACE_END
#define CONF_PMM_DMA_SPACE_END 0x3FFFFFF
#endif

static bitmap_t* dma_bitmap = NULL;
static size_t dma_numpages = 0U;
static size_t dma_numused = 0U;
static size_t dma_lastpage = 0U;

static void** pslab_list = NULL;
static size_t pslab_numpages = 0U;
static size_t pslab_numused = 0U;

struct memmap_info {
    uintptr_t start;
    uintptr_t end;
    size_t length;
    size_t npages;
};

static void get_memmap_info(const struct limine_memmap_entry* restrict entry, struct memmap_info* restrict info)
{
    info->start = page_align(entry->base);
    info->length = align_floor(entry->length, PAGE_SIZE);

    if(info->start == 0U) {
        info->start += PAGE_SIZE;
        info->length -= PAGE_SIZE;
    }

    info->end = info->start + info->length - 1U;
    info->npages = page_count(info->length);
}

static void add_region_dma(uintptr_t start, size_t length)
{
    size_t page = start / PAGE_SIZE;
    size_t npages = page_count(length);

    bitmap_range_set(dma_bitmap, page, page + npages - 1U);

    kprintf(KP_NOTICE, "pmm: adding dma region: [%p - %p]", (const void*)(start), (const void*)(start + length - 1U));
}

static void add_region_pslab(uintptr_t start, size_t length)
{
    void** head_ptr;
    uintptr_t address;

    for(address = 0U; address < length; address += PAGE_SIZE) {
        head_ptr = phys_to_hhdm(start + address);
        head_ptr[0] = pslab_list;
        pslab_list = head_ptr;
        pslab_numpages += 1U;
    }

    kprintf(KP_NOTICE, "pmm: adding pslab region: [%p - %p]", (const void*)(start), (const void*)(start + length - 1U));
}

uintptr_t __nodiscard pmm_alloc_dma(size_t num_pages)
{
    size_t page;
    size_t maxpage;

    maxpage = dma_numpages - num_pages;
    for(page = dma_lastpage; page < maxpage; page += 1U) {
        if(bitmap_range_isset(dma_bitmap, page, page + num_pages - 1U)) {
            bitmap_range_clear(dma_bitmap, page, page + num_pages - 1U);
            dma_lastpage = page + num_pages;
            dma_numused += num_pages;
            return page * PAGE_SIZE;
        }
    }

    maxpage = dma_lastpage - num_pages;
    for(page = 0U; page < maxpage; page += 1U) {
        if(bitmap_range_isset(dma_bitmap, page, page + num_pages - 1U)) {
            bitmap_range_clear(dma_bitmap, page, page + num_pages - 1U);
            dma_lastpage = page + num_pages;
            dma_numused += num_pages;
            return page * PAGE_SIZE;
        }
    }

    return 0U;
}

void* __nodiscard pmm_alloc_dma_hhdm(size_t num_pages)
{
    uintptr_t address = pmm_alloc_dma(num_pages);
    if(address == 0U)
        return NULL;
    return phys_to_hhdm(address);
}

void pmm_free_dma(uintptr_t address, size_t num_pages)
{
    size_t page = address / PAGE_SIZE;

    kassert(address < CONF_PMM_DMA_SPACE_END);
    kassert((page + num_pages) < dma_numpages);

    if(address) {
        bitmap_range_set(dma_bitmap, page, page + num_pages - 1);
        dma_numused -= num_pages;
        dma_lastpage = page;
    }
}

void pmm_free_dma_hhdm(void* restrict ptr, size_t num_pages)
{
    if(ptr) {
        pmm_free_dma(hhdm_to_phys(ptr), num_pages);
    }
}

void pmm_stats_dma(struct pmm_stats* restrict stats)
{
    stats->total_bytes = dma_numpages * PAGE_SIZE;
    stats->total_pages = dma_numpages;
    stats->used_bytes = dma_numused * PAGE_SIZE;
    stats->used_pages = dma_numused;
}

uintptr_t __nodiscard pmm_alloc_pslab(void)
{
    void* ptr = pmm_alloc_pslab_hhdm();
    if(ptr == NULL)
        return 0U;
    return hhdm_to_phys(ptr);
}

void* __nodiscard pmm_alloc_pslab_hhdm(void)
{
    void** head_ptr;

    if(pslab_list) {
        head_ptr = pslab_list;
        pslab_list = pslab_list[0];
        pslab_numused += 1U;
        return head_ptr;
    }

    return NULL;
}

void pmm_free_pslab(uintptr_t address)
{
    if(address) {
        pmm_free_pslab_hhdm(phys_to_hhdm(address));
    }
}

void pmm_free_pslab_hhdm(void* restrict ptr)
{
    void** head_ptr;

    if(ptr) {
        head_ptr = ptr;
        head_ptr[0] = pslab_list;
        pslab_list = head_ptr;
        pslab_numused -= 1U;
    }
}

void pmm_stats_pslab(struct pmm_stats* restrict stats)
{
    stats->total_bytes = pslab_numpages * PAGE_SIZE;
    stats->total_pages = pslab_numpages;
    stats->used_bytes = pslab_numused * PAGE_SIZE;
    stats->used_pages = pslab_numused;
}

uintptr_t __nodiscard pmm_alloc(void)
{
    uintptr_t address = pmm_alloc_pslab();

    if(address == 0U) {
        // Fallback to the DMA allocator; considering
        // it exclusively manages lower memory, this
        // would always happen on systems with less than
        // CONF_PMM_DMA_SPACE_END+1 bytes of RAM
        address = pmm_alloc_dma(1U);
    }

    return address;
}

void* __nodiscard pmm_alloc_hhdm(void)
{
    void* ptr = pmm_alloc_pslab_hhdm();

    if(ptr == NULL) {
        // Fallback to the DMA allocator; considering
        // it exclusively manages lower memory, this
        // would always happen on systems with less than
        // CONF_PMM_DMA_SPACE_END+1 bytes of RAM
        ptr = pmm_alloc_dma_hhdm(1U);
    }

    return ptr;
}

void pmm_free(uintptr_t address)
{
    if(address < CONF_PMM_DMA_SPACE_END) {
        pmm_free_dma(address, 1U);
    }
    else {
        pmm_free_pslab(address);
    }
}

void pmm_free_hhdm(void* restrict ptr)
{
    uintptr_t address = hhdm_to_phys(ptr);

    if(address < CONF_PMM_DMA_SPACE_END) {
        pmm_free_dma_hhdm(ptr, 1U);
    }
    else {
        pmm_free_pslab_hhdm(ptr);
    }
}

void pmm_stats(struct pmm_stats* restrict stats)
{
    struct pmm_stats dma_stats;
    struct pmm_stats pslab_stats;

    pmm_stats_dma(&dma_stats);
    pmm_stats_pslab(&pslab_stats);

    stats->total_bytes = dma_stats.total_bytes + pslab_stats.total_bytes;
    stats->total_pages = dma_stats.total_pages + pslab_stats.total_pages;
    stats->used_bytes = dma_stats.used_bytes + pslab_stats.used_bytes;
    stats->used_pages = dma_stats.used_pages + pslab_stats.used_pages;
}

void init_pmm(void)
{
    size_t i;
    const struct limine_memmap_entry* entry;
    struct memmap_info info;

    struct pmm_stats stats_dma;
    struct pmm_stats stats_pslab;
    struct pmm_stats stats;

    size_t bitmap_size;
    size_t bitmap_npages;

    char hrsize_buffer[128];

    bitmap_npages = 0U;

    for(i = 0U; i < memmap->entry_count; ++i) {
        entry = memmap->entries[i];

        if(entry->type == LIMINE_MEMMAP_USABLE) {
            get_memmap_info(entry, &info);

            if(info.start < CONF_PMM_DMA_SPACE_END) {
                if(info.end >= CONF_PMM_DMA_SPACE_END) {
                    info.end = CONF_PMM_DMA_SPACE_END;
                    info.length = info.end - info.start + 1U;
                    info.npages = page_count(info.length);
                }

                if(info.npages > bitmap_npages) {
                    bitmap_npages = info.npages;
                }
            }
        }
    }

    // Total pages to track in the bitmap; there can be
    // a little bit more so the count is aligned with the
    // actual integer element size (BITMAP_CHUNK_BITS/8 = sizeof(bitmap_t))
    dma_numpages = align_ceil(bitmap_npages, BITMAP_CHUNK_BITS);

    bitmap_size = bitmap_bytecount(dma_numpages);
    bitmap_npages = page_count(bitmap_size);
    bitmap_size = bitmap_npages * PAGE_SIZE;

    // Figure out where to put the bitmap; it has to fit in a free region
    // below the CONF_PMM_DMA_SPACE_END limit so that dma_alloc can manage it
    for(i = 0U; i < memmap->entry_count; ++i) {
        entry = memmap->entries[i];
        if(entry->type == LIMINE_MEMMAP_USABLE) {
            get_memmap_info(entry, &info);

            if(info.npages >= bitmap_npages && info.start + bitmap_size <= CONF_PMM_DMA_SPACE_END) {
                dma_bitmap = phys_to_hhdm(info.start);

                add_region_dma(info.start, bitmap_size);
                break;
            }
        }
    }

    if(dma_bitmap == NULL) {
        panic("pmm: out of memory");
        unreachable();
    }

    bitmap_range_clear(dma_bitmap, 0U, dma_numpages - 1U);

    // Distribute memory regions to respective allocators
    for(i = 0U; i < memmap->entry_count; ++i) {
        entry = memmap->entries[i];

        if(entry->type == LIMINE_MEMMAP_USABLE) {
            get_memmap_info(entry, &info);

            if(info.start > CONF_PMM_DMA_SPACE_END) {
                add_region_pslab(info.start, info.length);
                continue;
            }

            if(info.end >= CONF_PMM_DMA_SPACE_END && info.start <= CONF_PMM_DMA_SPACE_END) {
                size_t dma_length = CONF_PMM_DMA_SPACE_END - info.start + 1U;
                size_t pslab_length = info.end - CONF_PMM_DMA_SPACE_END;

                if(dma_length > 0) {
                    add_region_dma(info.start, dma_length);
                }
                if(pslab_length > 0) {
                    add_region_pslab(CONF_PMM_DMA_SPACE_END + 1U, pslab_length);
                }
                continue;
            }

            if(info.end <= CONF_PMM_DMA_SPACE_END) {
                add_region_dma(info.start, info.length);
            }
        }
    }

    pmm_stats_dma(&stats_dma);
    pmm_stats_pslab(&stats_pslab);

    stats.total_bytes = stats_dma.total_bytes + stats_pslab.total_bytes;
    stats.total_pages = stats_dma.total_pages + stats_pslab.total_pages;
    stats.used_bytes = stats_dma.used_bytes + stats_pslab.used_bytes;
    stats.used_pages = stats_dma.used_pages + stats_pslab.used_pages;

    hrsize_bin(hrsize_buffer, sizeof(hrsize_buffer), stats.total_bytes);
    kprintf(KP_NOTICE, "pmm: initialized with %s of physical memory in %zu pages", hrsize_buffer, stats.total_pages);

    hrsize_bin(hrsize_buffer, sizeof(hrsize_buffer), stats_dma.total_bytes);
    kprintf(KP_NOTICE, "pmm: dma bitmap tracks %s in %zu pages", hrsize_buffer, stats_dma.total_pages);

    hrsize_bin(hrsize_buffer, sizeof(hrsize_buffer), stats_pslab.total_bytes);
    kprintf(KP_NOTICE, "pmm: pslab list tracks %s in %zu pages", hrsize_buffer, stats_pslab.total_pages);
}
