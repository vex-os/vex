#include <vex/assert.h>
#include <vex/hhdm.h>
#include <vex/hrsize.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/memmap.h>
#include <vex/page.h>
#include <vex/pdma.h>
#include <vex/pframe.h>
#include <vex/pslab.h>
#include <vex/spinlock.h>

static void** pslab_list = nullptr;
static size_t pslab_numpages = 0U;
static size_t pslab_numused = 0U;
static spinlock_type pslab_lock = SPINLOCK_INIT;

static void pslab_add_region(uintptr_t start, size_t length)
{
    void** head_ptr;
    uintptr_t offset;

    for(offset = 0U; offset < length; offset += PAGE_SIZE) {
        struct pframe* pf = pframe_of(start + offset);

        pf->flags |= PFRAME_PSLAB;

        head_ptr = phys_to_virt(start + offset);
        head_ptr[0] = pslab_list;
        pslab_list = head_ptr;
        pslab_numpages += 1U;
    }

    pr_verbose("pslab: adding region: start=%p end=%p", (void*)(start), (void*)(start + length - 1U));
}

static void pslab_add_usable(uintptr_t start, size_t length)
{
    uintptr_t reserved_start;
    size_t reserved_length;
    pframe_storage(&reserved_start, &reserved_length);

    uintptr_t end = start + length;
    uintptr_t reserved_end = reserved_start + reserved_length;

    if(reserved_end <= start || reserved_start >= end) {
        pslab_add_region(start, length);
        return;
    }

    if(reserved_start > start) {
        pslab_add_region(start, reserved_start - start);
    }

    if(reserved_end < end) {
        pslab_add_region(reserved_end, end - reserved_end);
    }
}

uintptr_t pslab_alloc_phys(void)
{
    void* ptr = pslab_alloc_virt();
    if(ptr == nullptr)
        return 0U;
    return virt_to_phys(ptr);
}

void* pslab_alloc_virt(void)
{
    void** head_ptr;

    bool intr_state = spinlock_acquire_intr(&pslab_lock);

    if(pslab_list) {
        head_ptr = pslab_list;
        pslab_list = pslab_list[0];
        pslab_numused += 1U;

        struct pframe* pf = pframe_of(virt_to_phys(head_ptr));
        kassert(pf->refcount == 0);
        pf->refcount = 1;

        spinlock_release_intr(&pslab_lock, intr_state);

        return head_ptr;
    }

    spinlock_release_intr(&pslab_lock, intr_state);

    return nullptr;
}

void pslab_free_phys(uintptr_t physaddr)
{
    if(physaddr) {
        pslab_free_virt(phys_to_virt(physaddr));
    }
}

void pslab_free_virt(void* restrict virtaddr)
{
    void** head_ptr;

    bool intr_state = spinlock_acquire_intr(&pslab_lock);

    if(virtaddr) {
        struct pframe* pf = pframe_of(virt_to_phys(virtaddr));
        kassert(pf->flags & PFRAME_PSLAB);
        kassert(pf->refcount == 1);
        pf->refcount = 0;

        head_ptr = virtaddr;
        head_ptr[0] = pslab_list;
        pslab_list = head_ptr;
        pslab_numused -= 1U;
    }

    spinlock_release_intr(&pslab_lock, intr_state);
}

static void init_pslab(void)
{
    for(size_t i = 0U; i < memmap->entry_count; ++i) {
        const struct limine_memmap_entry* entry = memmap->entries[i];

        if(entry->type == LIMINE_MEMMAP_USABLE) {
            uintptr_t start = page_align_up(entry->base);
            size_t length = align_floor(entry->length, PAGE_SIZE);

            if(start == 0U) {
                start += PAGE_SIZE;
                length -= PAGE_SIZE;
            }

            if(start + length <= PDMA_APPROX_END) {
                continue;
            }

            if(start < PDMA_APPROX_END) {
                length -= PDMA_APPROX_END + 1U - start;
                start = PDMA_APPROX_END + 1U;
            }

            if(length > 0U) {
                pslab_add_usable(start, length);
            }
        }
    }

    char hrsize_buffer[128];
    hrsize_bin(hrsize_buffer, sizeof(hrsize_buffer), pslab_numpages * PAGE_SIZE);
    pr_verbose("pslab: tracking %s in %zu pages", hrsize_buffer, pslab_numpages);
}

initcall_bsp_pmm(init_pslab, INIT_MIDDLE);
