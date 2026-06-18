#ifndef D4C8D15A_CB51_45C1_9C8C_8B495E540FE1
#define D4C8D15A_CB51_45C1_9C8C_8B495E540FE1
#pragma once

#include <vex/spinlock.h>

#define VMMAP_BUCKETS 64U

struct mmu_ctx;

struct vmmap_entry {
    struct vmmap_entry* next;
    uintptr_t virt;
    uintptr_t phys;
    unsigned vprot;
};

struct vmmap {
    struct vmmap_entry* buckets[VMMAP_BUCKETS];
    struct mmu_ctx* ctx;
    spinlock_type lock;
};

struct vmmap* vmmap_create(void);
struct vmmap* vmmap_create_with_ctx(struct mmu_ctx* restrict ctx);
void vmmap_release(struct vmmap* restrict map);

int vmmap_anon(struct vmmap* restrict map, uintptr_t virt, unsigned vprot);
int vmmap_patch(struct vmmap* restrict map, uintptr_t virt, unsigned vprot);
int vmmap_unmap(struct vmmap* restrict map, uintptr_t virt);
int vmmap_unmap_all(struct vmmap* restrict map);

int vmmap_clone(struct vmmap* restrict map, const struct vmmap* restrict stem);

#endif /* D4C8D15A_CB51_45C1_9C8C_8B495E540FE1 */
