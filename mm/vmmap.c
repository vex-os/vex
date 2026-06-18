#include <vex/errno.h>
#include <vex/hhdm.h>
#include <vex/mmu.h>
#include <vex/page.h>
#include <vex/pslab.h>
#include <vex/slab.h>
#include <vex/spinlock.h>
#include <vex/string.h>
#include <vex/vmmap.h>

static_assert(sizeof(struct vmmap_entry) <= (PAGE_SIZE / 2));
static_assert(sizeof(struct vmmap) <= (PAGE_SIZE / 2));

static __always_inline size_t vmmap_bucket(uintptr_t virt)
{
    return (virt >> PAGE_SHIFT) % VMMAP_BUCKETS;
}

struct vmmap* vmmap_create(void)
{
    struct mmu_ctx* ctx = mmu_create_ctx();

    if(ctx == nullptr) {
        return nullptr;
    }

    struct vmmap* map = vmmap_create_with_ctx(ctx);

    if(map == nullptr) {
        mmu_release_ctx(ctx);
        return nullptr;
    }

    return map;
}

struct vmmap* vmmap_create_with_ctx(struct mmu_ctx* restrict ctx)
{
    if(ctx == nullptr) {
        return nullptr;
    }

    struct vmmap* map = slab_alloc(sizeof(struct vmmap));

    if(map == nullptr) {
        return nullptr;
    }

    for(size_t i = 0; i < VMMAP_BUCKETS; ++i) {
        map->buckets[i] = nullptr;
    }

    map->ctx = ctx;
    map->lock = SPINLOCK_INIT;

    return map;
}

void vmmap_release(struct vmmap* restrict map)
{
    vmmap_unmap_all(map);
    mmu_release_ctx(map->ctx);
    slab_free(map);
}

int vmmap_anon(struct vmmap* restrict map, uintptr_t virt, unsigned vprot)
{
    struct vmmap_entry* entry = slab_alloc(sizeof(struct vmmap_entry));

    if(entry == nullptr) {
        return -ENOMEM;
    }

    uintptr_t phys = pslab_alloc_phys();

    if(phys == 0) {
        slab_free(entry);
        return -ENOMEM;
    }

    memset(phys_to_virt(phys), 0, PAGE_SIZE);

    int retval = mmu_map(map->ctx, virt, phys, vprot);

    if(retval < 0) {
        pslab_free_phys(phys);
        slab_free(entry);
        return retval;
    }

    entry->virt = virt;
    entry->phys = phys;
    entry->vprot = vprot;

    size_t bucket = vmmap_bucket(virt);

    bool intr_state = spinlock_acquire_intr(&map->lock);

    entry->next = map->buckets[bucket];
    map->buckets[bucket] = entry;

    spinlock_release_intr(&map->lock, intr_state);

    return 0;
}

int vmmap_patch(struct vmmap* restrict map, uintptr_t virt, unsigned vprot)
{
    size_t bucket = vmmap_bucket(virt);

    bool intr_state = spinlock_acquire_intr(&map->lock);

    struct vmmap_entry* entry = map->buckets[bucket];

    while(entry) {
        if(entry->virt == virt) {
            break;
        }

        entry = entry->next;
    }

    if(entry == nullptr) {
        spinlock_release_intr(&map->lock, intr_state);
        return -ENOENT;
    }

    entry->vprot = vprot;

    spinlock_release_intr(&map->lock, intr_state);

    return mmu_patch(map->ctx, virt, vprot);
}

int vmmap_unmap(struct vmmap* restrict map, uintptr_t virt)
{
    size_t bucket = vmmap_bucket(virt);

    bool intr_state = spinlock_acquire_intr(&map->lock);

    struct vmmap_entry* prev = nullptr;
    struct vmmap_entry* entry = map->buckets[bucket];

    while(entry) {
        if(entry->virt == virt) {
            if(prev) {
                prev->next = entry->next;
            }
            else {
                map->buckets[bucket] = entry->next;
            }

            break;
        }

        prev = entry;
        entry = entry->next;
    }

    spinlock_release_intr(&map->lock, intr_state);

    if(entry == nullptr) {
        return -ENOENT;
    }

    mmu_unmap(map->ctx, virt);
    pslab_free_phys(entry->phys);
    slab_free(entry);

    return 0;
}

int vmmap_unmap_all(struct vmmap* restrict map)
{
    bool intr_state = spinlock_acquire_intr(&map->lock);

    for(size_t i = 0; i < VMMAP_BUCKETS; ++i) {
        struct vmmap_entry* entry = map->buckets[i];

        while(entry) {
            struct vmmap_entry* next = entry->next;

            mmu_unmap(map->ctx, entry->virt);
            pslab_free_phys(entry->phys);
            slab_free(entry);

            entry = next;
        }

        map->buckets[i] = nullptr;
    }

    spinlock_release_intr(&map->lock, intr_state);

    return 0;
}

int vmmap_clone(struct vmmap* restrict map, const struct vmmap* restrict stem)
{
    bool intr_state = spinlock_acquire_intr(&((struct vmmap*)stem)->lock);

    for(size_t i = 0; i < VMMAP_BUCKETS; ++i) {
        for(struct vmmap_entry* entry = stem->buckets[i]; entry; entry = entry->next) {
            struct vmmap_entry* copy = slab_alloc(sizeof(struct vmmap_entry));

            if(copy == nullptr) {
                spinlock_release_intr(&((struct vmmap*)stem)->lock, intr_state);
                return -ENOMEM;
            }

            uintptr_t phys = pslab_alloc_phys();

            if(phys == 0) {
                slab_free(copy);
                spinlock_release_intr(&((struct vmmap*)stem)->lock, intr_state);
                return -ENOMEM;
            }

            memcpy(phys_to_virt(phys), phys_to_virt(entry->phys), PAGE_SIZE);

            int retval = mmu_map(map->ctx, entry->virt, phys, entry->vprot);

            if(retval < 0) {
                pslab_free_phys(phys);
                slab_free(copy);
                spinlock_release_intr(&((struct vmmap*)stem)->lock, intr_state);
                return retval;
            }

            copy->virt = entry->virt;
            copy->phys = phys;
            copy->vprot = entry->vprot;

            size_t bucket = vmmap_bucket(entry->virt);
            copy->next = map->buckets[bucket];
            map->buckets[bucket] = copy;
        }
    }

    spinlock_release_intr(&((struct vmmap*)stem)->lock, intr_state);

    return 0;
}
