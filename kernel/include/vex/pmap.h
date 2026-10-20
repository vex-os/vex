#pragma once

#include <arch/pmap.h>
#include <vex/limits.h>
#include <vex/types.h>
#include <vex/vprot.h>

static_assert(PAGE_SIZE == (PAGEMAP_SIZE * sizeof(pmentry_t)));

extern pmap_t sys_pmap;

pmap_t* pmap_create(void) __nodiscard;
void pmap_release(pmap_t* restrict pmap);

int pmap_map(pmap_t* restrict pmap, uintptr_t virt, uintptr_t phys, unsigned vprot);
int pmap_map_range(pmap_t* restrict pmap, uintptr_t virt, uintptr_t phys, size_t size, unsigned vprot);

// The noflush variants leave the old translation in TLBs: the caller runs tlb_shootdown
// before that translation could matter, or knows that no hart ever used it
int pmap_patch(pmap_t* restrict pmap, uintptr_t virt, unsigned vprot);
int pmap_patch_noflush(pmap_t* restrict pmap, uintptr_t virt, unsigned vprot);
int pmap_unmap(pmap_t* restrict pmap, uintptr_t virt);
int pmap_unmap_noflush(pmap_t* restrict pmap, uintptr_t virt);
int pmap_unmap_range_noflush(pmap_t* restrict pmap, uintptr_t virt, size_t size);

uintptr_t pmap_resolve_address(pmap_t* restrict pmap, uintptr_t virt);
unsigned pmap_resolve_vprot(pmap_t* restrict pmap, uintptr_t virt);
