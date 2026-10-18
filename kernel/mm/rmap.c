#include <vex/assert.h>
#include <vex/errno.h>
#include <vex/page.h>
#include <vex/pframe.h>
#include <vex/pmap.h>
#include <vex/rmap.h>
#include <vex/slab.h>
#include <vex/spinlock.h>

// Mappings of one page table only change under its vmspace lock, so a frame
// found by a lookup still carries the entry unless a walker has taken it since

#define NUM_LOCKS 256

static_assert(sizeof(struct rmap_entry) <= SLAB_MAX_SIZE);

// Taken with interrupts off, as the scheduler's interrupt releases address spaces
static spinlock_type locks[NUM_LOCKS];

static spinlock_type* frame_lock(uintptr_t phys)
{
    return &locks[(phys / PAGE_SIZE) % NUM_LOCKS];
}

static struct rmap_entry* unlink_entry(struct pframe* restrict pf, struct pagemap* restrict pmap, uintptr_t virt)
{
    for(struct rmap_entry** prev = &pf->mappings; *prev; prev = &(*prev)->next) {
        struct rmap_entry* entry = *prev;

        if(entry->pmap == pmap && entry->virt == virt) {
            *prev = entry->next;
            return entry;
        }
    }

    return nullptr;
}

int rmap_map(struct pagemap* restrict pmap, uintptr_t virt, uintptr_t phys, unsigned vprot)
{
    struct rmap_entry* entry = slab_alloc(sizeof(struct rmap_entry));

    if(entry == nullptr) {
        return -ENOMEM;
    }

    entry->pmap = pmap;
    entry->virt = virt;

    struct pframe* pf = pframe_of(phys);
    spinlock_type* lock = frame_lock(phys);

    bool intr_state = spinlock_acquire_intr(lock);

    int retval = pmap_map(pmap, virt, phys, vprot);

    if(retval == 0) {
        entry->next = pf->mappings;
        pf->mappings = entry;
    }

    spinlock_release_intr(lock, intr_state);

    if(retval < 0) {
        slab_free(entry);
    }

    return retval;
}

uintptr_t rmap_unmap(struct pagemap* restrict pmap, uintptr_t virt)
{
    uintptr_t phys = page_align(pmap_resolve_address(pmap, virt));

    if(phys == 0) {
        return 0;
    }

    struct pframe* pf = pframe_of(phys);
    spinlock_type* lock = frame_lock(phys);

    bool intr_state = spinlock_acquire_intr(lock);

    struct rmap_entry* entry = unlink_entry(pf, pmap, virt);

    if(entry) {
        int retval = pmap_unmap_noflush(pmap, virt);
        kassert(retval == 0);
    }

    spinlock_release_intr(lock, intr_state);

    if(entry == nullptr) {
        return 0;
    }

    slab_free(entry);

    return phys;
}

int rmap_replace(struct pagemap* restrict pmap, uintptr_t virt, uintptr_t old_phys, uintptr_t new_phys, unsigned vprot)
{
    struct pframe* old_pf = pframe_of(old_phys);
    struct pframe* new_pf = pframe_of(new_phys);
    spinlock_type* lock = frame_lock(old_phys);

    bool intr_state = spinlock_acquire_intr(lock);

    struct rmap_entry* entry = unlink_entry(old_pf, pmap, virt);

    if(entry) {
        int retval = pmap_unmap_noflush(pmap, virt);
        kassert(retval == 0);

        retval = pmap_map(pmap, virt, new_phys, vprot);
        kassert(retval == 0); // The page table that held the old entry is still there

        entry->next = new_pf->mappings;
        new_pf->mappings = entry;
    }

    spinlock_release_intr(lock, intr_state);

    if(entry == nullptr) {
        return -EAGAIN;
    }

    return 0;
}

int rmap_patch(struct pagemap* restrict pmap, uintptr_t virt, uintptr_t phys, unsigned vprot)
{
    spinlock_type* lock = frame_lock(phys);
    bool intr_state = spinlock_acquire_intr(lock);

    int retval = -EAGAIN;

    if(phys == page_align(pmap_resolve_address(pmap, virt))) {
        retval = pmap_patch_noflush(pmap, virt, vprot);
    }

    spinlock_release_intr(lock, intr_state);

    return retval;
}

uintptr_t rmap_pin(struct pagemap* restrict pmap, uintptr_t virt, bool* restrict sole)
{
    uintptr_t phys = page_align(pmap_resolve_address(pmap, virt));

    if(phys == 0) {
        return 0;
    }

    struct pframe* pf = pframe_of(phys);
    spinlock_type* lock = frame_lock(phys);

    bool intr_state = spinlock_acquire_intr(lock);

    bool mapped = phys == page_align(pmap_resolve_address(pmap, virt));

    if(mapped) {
        if(sole) {
            *sole = !pframe_shared(pf);
        }

        pframe_incref(pf);
    }

    spinlock_release_intr(lock, intr_state);

    if(!mapped) {
        return 0;
    }

    return phys;
}

size_t rmap_unmap_all(struct pframe* restrict pf)
{
    spinlock_type* lock = frame_lock(pframe_phys(pf));

    bool intr_state = spinlock_acquire_intr(lock);

    struct rmap_entry* entries = pf->mappings;
    pf->mappings = nullptr;

    size_t count = 0;

    for(struct rmap_entry* it = entries; it; it = it->next) {
        int retval = pmap_unmap_noflush(it->pmap, it->virt);
        kassert(retval == 0);
        count += 1;
    }

    spinlock_release_intr(lock, intr_state);

    while(entries) {
        struct rmap_entry* next = entries->next;
        slab_free(entries);
        entries = next;
    }

    return count;
}

size_t rmap_protect_all(struct pframe* restrict pf)
{
    spinlock_type* lock = frame_lock(pframe_phys(pf));
    bool intr_state = spinlock_acquire_intr(lock);

    size_t count = 0;

    for(struct rmap_entry* it = pf->mappings; it; it = it->next) {
        unsigned vprot = pmap_resolve_vprot(it->pmap, it->virt);

        if(vprot & VPROT_WRITE) {
            int retval = pmap_patch_noflush(it->pmap, it->virt, vprot & ~VPROT_WRITE);
            kassert(retval == 0);
            count += 1;
        }
    }

    spinlock_release_intr(lock, intr_state);

    return count;
}
