#pragma once

#include <vex/spinlock.h>

#define VMS_BUCKETS 128U

struct pagemap;

struct vms_page {
    struct vms_page* next;
    uintptr_t virt;
    uintptr_t phys;
    unsigned vprot;
};

struct vmspace {
    struct vms_page* buckets[VMS_BUCKETS];
    struct pagemap* pmap;
    spinlock_type lock;
};

struct vmspace* vms_create(void);
struct vmspace* vms_create_pmap(struct pagemap* restrict pmap);
void vms_release(struct vmspace* restrict vms);

int vms_anon(struct vmspace* restrict vms, uintptr_t virt, unsigned vprot);
int vms_patch(struct vmspace* restrict vms, uintptr_t virt, unsigned vprot);
int vms_unmap(struct vmspace* restrict vms, uintptr_t virt);
int vms_unmap_all(struct vmspace* restrict vms);

int vms_clone(struct vmspace* restrict vms, struct vmspace* restrict stem);
