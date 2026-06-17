#include <vex/errno.h>
#include <vex/hhdm.h>
#include <vex/page.h>
#include <vex/pslab.h>
#include <vex/slab.h>
#include <vex/spinlock.h>
#include <vex/string.h>
#include <vex/vmm.h>
#include <vex/vmsp.h>

static __always_inline size_t vms_bucket(uintptr_t virt)
{
    return (virt >> PAGE_SHIFT) % VMSPACE_BUCKETS;
}

int vms_map_anon(struct vmspace* restrict vms, struct pagemap* restrict vm, uintptr_t virt, unsigned vprot)
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

    int retval = vmm_map(vm, virt, phys, vprot);

    if(retval < 0) {
        pslab_free_phys(phys);
        slab_free(page);
        return retval;
    }

    page->virt = virt;
    page->phys = phys;
    page->vprot = vprot;

    size_t bucket = vms_bucket(virt);

    bool intr_state = spinlock_acquire_intr(&vms->lock);

    page->next = vms->buckets[bucket];
    vms->buckets[bucket] = page;

    spinlock_release_intr(&vms->lock, intr_state);

    return 0;
}

int vms_patch(struct vmspace* restrict vms, struct pagemap* restrict vm, uintptr_t virt, unsigned vprot)
{
    size_t bucket = vms_bucket(virt);

    bool intr_state = spinlock_acquire_intr(&vms->lock);

    struct vms_page* page = vms->buckets[bucket];

    while(page) {
        if(page->virt == virt) {
            break;
        }

        page = page->next;
    }

    spinlock_release_intr(&vms->lock, intr_state);

    if(page == nullptr) {
        return -ENOENT;
    }

    page->vprot = vprot;

    return vmm_patch(vm, virt, vprot);
}

int vms_unmap_page(struct vmspace* restrict vms, struct pagemap* restrict vm, uintptr_t virt)
{
    size_t bucket = vms_bucket(virt);

    bool intr_state = spinlock_acquire_intr(&vms->lock);

    struct vms_page* prev = nullptr;
    struct vms_page* page = vms->buckets[bucket];

    while(page) {
        if(page->virt == virt) {
            if(prev) {
                prev->next = page->next;
            }
            else {
                vms->buckets[bucket] = page->next;
            }

            break;
        }

        prev = page;
        page = page->next;
    }

    spinlock_release_intr(&vms->lock, intr_state);

    if(page == nullptr) {
        return -ENOENT;
    }

    vmm_unmap(vm, virt);
    pslab_free_phys(page->phys);
    slab_free(page);

    return 0;
}

int vms_unmap_all(struct vmspace* restrict vms, struct pagemap* restrict vm)
{
    bool intr_state = spinlock_acquire_intr(&vms->lock);

    for(size_t i = 0; i < VMSPACE_BUCKETS; ++i) {
        struct vms_page* page = vms->buckets[i];

        while(page) {
            struct vms_page* next = page->next;

            vmm_unmap(vm, page->virt);
            pslab_free_phys(page->phys);
            slab_free(page);

            page = next;
        }

        vms->buckets[i] = nullptr;
    }

    spinlock_release_intr(&vms->lock, intr_state);

    return 0;
}

int vms_clone(struct vmspace* restrict dst, struct pagemap* restrict dst_vm, const struct vmspace* restrict src)
{
    for(size_t i = 0; i < VMSPACE_BUCKETS; ++i) {
        for(struct vms_page* page = src->buckets[i]; page; page = page->next) {
            struct vms_page* copy = slab_alloc(sizeof(struct vms_page));

            if(copy == nullptr) {
                return -ENOMEM;
            }

            uintptr_t phys = pslab_alloc_phys();

            if(phys == 0) {
                slab_free(copy);
                return -ENOMEM;
            }

            memcpy(phys_to_virt(phys), phys_to_virt(page->phys), PAGE_SIZE);

            int retval = vmm_map(dst_vm, page->virt, phys, page->vprot);

            if(retval < 0) {
                pslab_free_phys(phys);
                slab_free(copy);
                return retval;
            }

            copy->virt = page->virt;
            copy->phys = phys;
            copy->vprot = page->vprot;

            size_t bucket = vms_bucket(page->virt);
            copy->next = dst->buckets[bucket];
            dst->buckets[bucket] = copy;
        }
    }

    return 0;
}
