#ifndef D4C8D15A_CB51_45C1_9C8C_8B495E540FE1
#define D4C8D15A_CB51_45C1_9C8C_8B495E540FE1
#pragma once

#include <vex/spinlock.h>

#define VMSPACE_BUCKETS 64U

struct pagemap;

struct vms_page {
    struct vms_page* next;
    uintptr_t virt;
    uintptr_t phys;
    unsigned vprot;
};

struct vmspace {
    struct vms_page* buckets[VMSPACE_BUCKETS];
    spinlock_type lock;
};

int vms_map_anon(struct vmspace* restrict vms, struct pagemap* restrict vm, uintptr_t virt, unsigned vprot);
int vms_patch(struct vmspace* restrict vms, struct pagemap* restrict vm, uintptr_t virt, unsigned vprot);

int vms_unmap_all(struct vmspace* restrict vms, struct pagemap* restrict vm);
int vms_unmap_page(struct vmspace* restrict vms, struct pagemap* restrict vm, uintptr_t virt);

int vms_clone(struct vmspace* restrict vms, struct pagemap* restrict vm, const struct vmspace* restrict stem);

#endif /* D4C8D15A_CB51_45C1_9C8C_8B495E540FE1 */
