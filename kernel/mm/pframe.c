#include <vex/assert.h>
#include <vex/atomic.h>
#include <vex/hhdm.h>
#include <vex/hrsize.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/memmap.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/pdma.h>
#include <vex/pframe.h>
#include <vex/string.h>

// Indexed by frame number up
// to the highest usable frame
static struct pframe* pframes = nullptr;
static size_t num_pframes = 0U;

static uintptr_t storage_start = 0U;
static size_t storage_length = 0U;

struct pframe* pframe_of(uintptr_t phys)
{
    size_t index = phys / PAGE_SIZE;
    kassert(index < num_pframes);
    return &pframes[index];
}

uintptr_t pframe_phys(const struct pframe* restrict pf)
{
    return (uintptr_t)(pf - pframes) * PAGE_SIZE;
}

void pframe_incref(struct pframe* restrict pf)
{
    atomic_fetch_add(&pf->refcount, 1, ATOMIC_RELAXED);
}

bool pframe_decref(struct pframe* restrict pf)
{
    uint32_t refcount = atomic_load(&pf->refcount, ATOMIC_ACQUIRE);

    while(refcount > 1) {
        if(atomic_cmpxchg_weak(&pf->refcount, &refcount, refcount - 1, ATOMIC_ACQ_REL, ATOMIC_ACQUIRE)) {
            return false;
        }
    }

    return true;
}

bool pframe_shared(const struct pframe* restrict pf)
{
    return atomic_load(&pf->refcount, ATOMIC_ACQUIRE) > 1;
}

void pframe_storage(uintptr_t* restrict start, size_t* restrict length)
{
    *start = storage_start;
    *length = storage_length;
}

static void init_pframe(void)
{
    uintptr_t usable_end = 0U;

    for(size_t i = 0U; i < memmap->entry_count; ++i) {
        const struct limine_memmap_entry* entry = memmap->entries[i];
        uintptr_t end = page_align(entry->base + entry->length);

        if(entry->type == LIMINE_MEMMAP_USABLE && end > usable_end) {
            usable_end = end;
        }
    }

    num_pframes = usable_end / PAGE_SIZE;
    storage_length = page_align_up(num_pframes * sizeof(struct pframe));

    // Taken from above the DMA zone, so pdma keeps all of it
    for(size_t i = 0U; i < memmap->entry_count; ++i) {
        const struct limine_memmap_entry* entry = memmap->entries[i];

        if(entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }

        uintptr_t start = page_align_up(entry->base);
        uintptr_t end = page_align(entry->base + entry->length);

        if(start <= PDMA_APPROX_END) {
            start = PDMA_APPROX_END + 1U;
        }

        if(end > start && end - start >= storage_length) {
            storage_start = start;
            break;
        }
    }

    if(storage_start == 0U) {
        panic("pframe: no room for %zu frames", num_pframes);
        unreachable();
    }

    pframes = phys_to_virt(storage_start);
    memset(pframes, 0, storage_length);

    char hrsize_buffer[128];
    hrsize_bin(hrsize_buffer, sizeof(hrsize_buffer), storage_length);
    pr_verbose("pframe: %zu frames described in %s at %p", num_pframes, hrsize_buffer, (void*)(storage_start));
}

initcall_bsp_pmm(init_pframe, INIT_EARLY);
