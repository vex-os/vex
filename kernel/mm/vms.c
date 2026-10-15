#include <vex/errno.h>
#include <vex/hhdm.h>
#include <vex/page.h>
#include <vex/pmap.h>
#include <vex/pslab.h>
#include <vex/slab.h>
#include <vex/spinlock.h>
#include <vex/string.h>
#include <vex/tlb.h>
#include <vex/vms.h>

static_assert(sizeof(struct vms_page) <= (PAGE_SIZE / 2));
static_assert(sizeof(struct vmspace) <= (PAGE_SIZE / 2));

static __always_inline size_t vms_bucket(uintptr_t virt)
{
    return (virt >> PAGE_SHIFT) % VMS_BUCKETS;
}

static int unmap_all(struct vmspace* restrict vms, bool shootdown)
{
    struct vms_page* detached = nullptr;

    bool intr_state = spinlock_acquire_intr(&vms->lock);

    for(size_t i = 0; i < VMS_BUCKETS; ++i) {
        while(vms->buckets[i]) {
            struct vms_page* entry = vms->buckets[i];
            vms->buckets[i] = entry->next;
            entry->next = detached;
            detached = entry;
        }
    }

    spinlock_release_intr(&vms->lock, intr_state);

    for(struct vms_page* entry = detached; entry; entry = entry->next) {
        pmap_unmap_noflush(vms->pmap, entry->virt);
    }

    // The pages may only be reused once
    // no hart can reach them through a stale TLB entry
    if(shootdown && detached) {
        tlb_shootdown();
    }

    while(detached) {
        struct vms_page* next = detached->next;
        pslab_free_phys(detached->phys);
        slab_free(detached);
        detached = next;
    }

    return 0;
}

struct vmspace* vms_create(void)
{
    struct pagemap* pmap = pmap_create();

    if(pmap == nullptr) {
        return nullptr;
    }

    struct vmspace* vms = vms_create_pmap(pmap);

    if(vms == nullptr) {
        pmap_release(pmap);
        return nullptr;
    }

    return vms;
}

struct vmspace* vms_create_pmap(struct pagemap* restrict pmap)
{
    if(pmap == nullptr) {
        return nullptr;
    }

    struct vmspace* vms = slab_alloc(sizeof(struct vmspace));

    if(vms == nullptr) {
        return nullptr;
    }

    for(size_t i = 0; i < VMS_BUCKETS; ++i) {
        vms->buckets[i] = nullptr;
    }

    vms->pmap = pmap;
    vms->lock = SPINLOCK_INIT;

    return vms;
}

void vms_release(struct vmspace* restrict vms)
{
    // Released only once no hart has these
    // page tables loaded, so no TLB holds their entries
    unmap_all(vms, false);
    pmap_release(vms->pmap);
    slab_free(vms);
}

int vms_anon(struct vmspace* restrict vms, uintptr_t virt, unsigned vprot)
{
    struct vms_page* page = slab_alloc(sizeof(struct vms_page));

    if(page == nullptr) {
        return -ENOMEM;
    }

    uintptr_t phys = pslab_alloc_phys();

    if(phys == 0) {
        slab_free(page);
        return -ENOMEM;
    }

    memset(phys_to_virt(phys), 0, PAGE_SIZE);

    size_t bucket = vms_bucket(virt);

    bool intr_state = spinlock_acquire_intr(&vms->lock);

    for(struct vms_page* e = vms->buckets[bucket]; e; e = e->next) {
        if(e->virt == virt) {
            spinlock_release_intr(&vms->lock, intr_state);
            pslab_free_phys(phys);
            slab_free(page);
            return -EEXIST;
        }
    }

    int retval = pmap_map(vms->pmap, virt, phys, vprot);

    if(retval < 0) {
        spinlock_release_intr(&vms->lock, intr_state);
        pslab_free_phys(phys);
        slab_free(page);
        return retval;
    }

    page->virt = virt;
    page->phys = phys;
    page->vprot = vprot;
    page->next = vms->buckets[bucket];
    vms->buckets[bucket] = page;

    spinlock_release_intr(&vms->lock, intr_state);

    return 0;
}

int vms_patch(struct vmspace* restrict vms, uintptr_t virt, unsigned vprot)
{
    size_t bucket = vms_bucket(virt);

    bool intr_state = spinlock_acquire_intr(&vms->lock);

    struct vms_page* entry = vms->buckets[bucket];

    while(entry) {
        if(entry->virt == virt) {
            break;
        }

        entry = entry->next;
    }

    if(entry == nullptr) {
        spinlock_release_intr(&vms->lock, intr_state);
        return -ENOENT;
    }

    entry->vprot = vprot;

    spinlock_release_intr(&vms->lock, intr_state);

    return pmap_patch(vms->pmap, virt, vprot);
}

int vms_unmap(struct vmspace* restrict vms, uintptr_t virt)
{
    size_t bucket = vms_bucket(virt);

    bool intr_state = spinlock_acquire_intr(&vms->lock);

    struct vms_page* prev = nullptr;
    struct vms_page* entry = vms->buckets[bucket];

    while(entry) {
        if(entry->virt == virt) {
            if(prev) {
                prev->next = entry->next;
            }
            else {
                vms->buckets[bucket] = entry->next;
            }

            break;
        }

        prev = entry;
        entry = entry->next;
    }

    spinlock_release_intr(&vms->lock, intr_state);

    if(entry == nullptr) {
        return -ENOENT;
    }

    pmap_unmap(vms->pmap, virt);
    pslab_free_phys(entry->phys);
    slab_free(entry);

    return 0;
}

int vms_unmap_all(struct vmspace* restrict vms)
{
    return unmap_all(vms, true);
}

int vms_clone(struct vmspace* restrict vms, struct vmspace* restrict stem)
{
    bool intr_state = spinlock_acquire_intr(&stem->lock);

    for(size_t i = 0; i < VMS_BUCKETS; ++i) {
        for(struct vms_page* entry = stem->buckets[i]; entry; entry = entry->next) {
            struct vms_page* copy = slab_alloc(sizeof(struct vms_page));

            if(copy == nullptr) {
                spinlock_release_intr(&stem->lock, intr_state);
                vms_unmap_all(vms);
                return -ENOMEM;
            }

            uintptr_t phys = pslab_alloc_phys();

            if(phys == 0) {
                slab_free(copy);
                spinlock_release_intr(&stem->lock, intr_state);
                vms_unmap_all(vms);
                return -ENOMEM;
            }

            memcpy(phys_to_virt(phys), phys_to_virt(entry->phys), PAGE_SIZE);

            int retval = pmap_map(vms->pmap, entry->virt, phys, entry->vprot);

            if(retval < 0) {
                pslab_free_phys(phys);
                slab_free(copy);
                spinlock_release_intr(&stem->lock, intr_state);
                vms_unmap_all(vms);
                return retval;
            }

            copy->virt = entry->virt;
            copy->phys = phys;
            copy->vprot = entry->vprot;

            size_t bucket = vms_bucket(entry->virt);
            copy->next = vms->buckets[bucket];
            vms->buckets[bucket] = copy;
        }
    }

    spinlock_release_intr(&stem->lock, intr_state);

    return 0;
}
