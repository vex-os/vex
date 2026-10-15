#include <vex/bitmap.h>
#include <vex/errno.h>
#include <vex/initcall.h>
#include <vex/kstack.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/pmap.h>
#include <vex/pslab.h>
#include <vex/spinlock.h>

// Each slot is a guard page followed by the stack itself; the guard page
// is never mapped, so running off the bottom of a stack faults right away
#define KSTACK_SLOT_SIZE ((KSTACK_PAGES + 1U) * PAGE_SIZE)
#define KSTACK_NUM_SLOTS 16384U

static_assert(KSTACK_NUM_SLOTS % BITMAP_CHUNK_BITS == 0);

static bitmap_type slots[KSTACK_NUM_SLOTS / BITMAP_CHUNK_BITS]; // A set bit means the slot is free
static size_t next_slot = 0U;
static spinlock_type slots_lock = SPINLOCK_INIT;

static __always_inline uintptr_t slot_base(size_t slot)
{
    return KSTACK_REGION_BASE + slot * KSTACK_SLOT_SIZE;
}

static size_t claim_slot(void)
{
    bool intr_state = spinlock_acquire_intr(&slots_lock);

    for(size_t i = 0U; i < KSTACK_NUM_SLOTS; i += 1U) {
        size_t slot = (next_slot + i) % KSTACK_NUM_SLOTS;

        if(bitmap_isset(slots, slot)) {
            bitmap_clear(slots, slot);
            next_slot = slot + 1U;
            spinlock_release_intr(&slots_lock, intr_state);
            return slot;
        }
    }

    spinlock_release_intr(&slots_lock, intr_state);

    return SIZE_MAX;
}

static void release_slot(size_t slot)
{
    bool intr_state = spinlock_acquire_intr(&slots_lock);
    bitmap_set(slots, slot);
    spinlock_release_intr(&slots_lock, intr_state);
}

static void unmap_pages(uintptr_t stack_bottom, size_t count)
{
    for(size_t i = 0U; i < count; i += 1U) {
        uintptr_t virt = stack_bottom + i * PAGE_SIZE;
        uintptr_t phys = pmap_resolve_address(&sys_pmap, virt);

        pmap_unmap(&sys_pmap, virt);
        pslab_free_phys(page_align(phys));
    }
}

void* kstack_alloc(void)
{
    size_t slot = claim_slot();

    if(slot == SIZE_MAX) {
        return nullptr;
    }

    uintptr_t stack_bottom = slot_base(slot) + PAGE_SIZE;

    for(size_t i = 0U; i < KSTACK_PAGES; i += 1U) {
        uintptr_t phys = pslab_alloc_phys();

        if(phys == 0U || pmap_map(&sys_pmap, stack_bottom + i * PAGE_SIZE, phys, VPROT_RW) < 0) {
            pslab_free_phys(phys);
            unmap_pages(stack_bottom, i);
            release_slot(slot);
            return nullptr;
        }
    }

    return (void*)(stack_bottom + KSTACK_SIZE);
}

void kstack_free(void* restrict top)
{
    if(top == nullptr) {
        return;
    }

    uintptr_t stack_top = (uintptr_t)(top);
    uintptr_t stack_bottom = stack_top - KSTACK_SIZE;
    size_t slot = (stack_bottom - PAGE_SIZE - KSTACK_REGION_BASE) / KSTACK_SLOT_SIZE;

    if(stack_top < KSTACK_REGION_BASE || slot >= KSTACK_NUM_SLOTS || slot_base(slot) + KSTACK_SLOT_SIZE != stack_top) {
        panic("kstack: freeing invalid kernel stack %p", top);
    }

    unmap_pages(stack_bottom, KSTACK_PAGES);
    release_slot(slot);
}

bool kstack_is_guard(uintptr_t address)
{
    if(address < KSTACK_REGION_BASE) {
        return false;
    }

    size_t offset = address - KSTACK_REGION_BASE;

    if(offset >= KSTACK_NUM_SLOTS * KSTACK_SLOT_SIZE) {
        return false;
    }

    return (offset % KSTACK_SLOT_SIZE) < PAGE_SIZE;
}

static void init_kstack(void)
{
    bitmap_range_set(slots, 0U, KSTACK_NUM_SLOTS - 1U);

    // Address spaces share kernel-half page tables by copying the top-level
    // entries on creation; mapping a page here once creates the region's entry
    uintptr_t phys = pslab_alloc_phys();

    if(phys == 0U || pmap_map(&sys_pmap, KSTACK_REGION_BASE, phys, VPROT_RW) < 0) {
        panic("kstack: out of memory");
    }

    pmap_unmap(&sys_pmap, KSTACK_REGION_BASE);
    pslab_free_phys(phys);
}

initcall_bsp_heap(init_kstack, INIT_MIDDLE);
