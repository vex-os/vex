#include <core/assert.h>
#include <core/kprintf.h>
#include <core/spinlock.h>
#include <init/initcall.h>
#include <lib/hrsize.h>
#include <mm/hhdm.h>
#include <mm/memmap.h>
#include <mm/page.h>
#include <mm/pdma.h>
#include <mm/pslab.h>

static void** pslab_list = nullptr;
static size_t pslab_numpages = 0U;
static size_t pslab_numused = 0U;
static spinlock_type pslab_lock = SPINLOCK_INIT;

static void add_region(uintptr_t start, size_t length)
{
    void** head_ptr;
    uintptr_t offset;

    for(offset = 0U; offset < length; offset += PAGE_SIZE) {
        head_ptr = phys_to_virt(start + offset);
        head_ptr[0] = pslab_list;
        pslab_list = head_ptr;
        pslab_numpages += 1U;
    }

    pr_notice("pslab: adding region: [%p..%p]", (const void*)start, (const void*)(start + length - 1U));
}

uintptr_t pslab_alloc(void)
{
    void* ptr = pslab_alloc_virt();
    if(ptr == nullptr)
        return 0U;
    return virt_to_phys(ptr);
}

void* pslab_alloc_virt(void)
{
    void** head_ptr;

    spinlock_acquire(&pslab_lock);

    if(pslab_list) {
        head_ptr = pslab_list;
        pslab_list = pslab_list[0];
        pslab_numused += 1U;

        spinlock_release(&pslab_lock);

        return head_ptr;
    }

    spinlock_release(&pslab_lock);

    return nullptr;
}

void pslab_free(uintptr_t physaddr)
{
    if(physaddr) {
        pslab_free_virt(phys_to_virt(physaddr));
    }
}

void pslab_free_virt(void* restrict virtaddr)
{
    void** head_ptr;

    spinlock_acquire(&pslab_lock);

    if(virtaddr) {
        head_ptr = virtaddr;
        head_ptr[0] = pslab_list;
        pslab_list = head_ptr;
        pslab_numused -= 1U;
    }

    spinlock_release(&pslab_lock);
}

void pslab_get_stats(struct pslab_stats* stats)
{
    stats->total_bytes = pslab_numpages * PAGE_SIZE;
    stats->total_pages = pslab_numpages;
    stats->used_bytes = pslab_numused * PAGE_SIZE;
    stats->used_pages = pslab_numused;
}

define_initcall(pslab, INITCALL_STAGE_BSP_PMM, INITCALL_ORDER_EARLY)
{
    size_t i;
    const struct limine_memmap_entry* entry;
    struct pslab_stats stats;
    char hrsize_buffer[128];

    uintptr_t start;
    size_t length;

    for(i = 0U; i < memmap->entry_count; ++i) {
        entry = memmap->entries[i];

        if(entry->type == LIMINE_MEMMAP_USABLE) {
            start = page_align_up(entry->base);
            length = align_floor(entry->length, PAGE_SIZE);

            if(start == 0U) {
                start += PAGE_SIZE;
                length -= PAGE_SIZE;
            }

            if(start + length <= PDMA_APPROX_END) {
                continue; // pslab only manages memory above the DMA zone
            }

            if(start < PDMA_APPROX_END) {
                length -= PDMA_APPROX_END + 1U - start;
                start = PDMA_APPROX_END + 1U;
            }

            if(length > 0U) {
                add_region(start, length);
            }
        }
    }

    pslab_get_stats(&stats);

    hrsize_bin(hrsize_buffer, sizeof(hrsize_buffer), stats.total_bytes);
    pr_notice("pslab: tracking %s in %zu pages", hrsize_buffer, stats.total_pages);
}
