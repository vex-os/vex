#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

struct pagemap;
struct pframe;

struct rmap_entry {
    struct rmap_entry* next;
    struct pagemap* pmap;
    uintptr_t virt;
};

int rmap_map(struct pagemap* restrict pmap, uintptr_t virt, uintptr_t phys, unsigned vprot);
uintptr_t rmap_unmap(struct pagemap* restrict pmap, uintptr_t virt) __nodiscard;
int rmap_replace(struct pagemap* restrict pmap, uintptr_t virt, uintptr_t old_phys, uintptr_t new_phys, unsigned vprot);
int rmap_patch(struct pagemap* restrict pmap, uintptr_t virt, uintptr_t phys, unsigned vprot);
uintptr_t rmap_pin(struct pagemap* restrict pmap, uintptr_t virt, bool* restrict sole) __nodiscard;
size_t rmap_unmap_all(struct pframe* restrict pf) __nodiscard;
size_t rmap_protect_all(struct pframe* restrict pf) __nodiscard;
