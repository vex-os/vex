#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

typedef struct pmap_s pmap_t;
typedef struct pframe_s pframe_t;

typedef struct rmap_entry_s rmap_entry_t;

struct rmap_entry_s {
    rmap_entry_t* next;
    pmap_t* pmap;
    uintptr_t virt;
};

int rmap_map(pmap_t* restrict pmap, uintptr_t virt, uintptr_t phys, unsigned vprot);
uintptr_t rmap_unmap(pmap_t* restrict pmap, uintptr_t virt) __nodiscard;
int rmap_replace(pmap_t* restrict pmap, uintptr_t virt, uintptr_t old_phys, uintptr_t new_phys, unsigned vprot);
int rmap_patch(pmap_t* restrict pmap, uintptr_t virt, uintptr_t phys, unsigned vprot);
uintptr_t rmap_pin(pmap_t* restrict pmap, uintptr_t virt, bool* restrict sole) __nodiscard;
size_t rmap_unmap_all(pframe_t* restrict pframe) __nodiscard;
size_t rmap_protect_all(pframe_t* restrict pframe) __nodiscard;
