#pragma once

#include <arch/pmap.h>
#include <vex/limits.h>
#include <vex/types.h>
#include <vex/vprot.h>

static_assert(PAGE_SIZE == (PAGEMAP_SIZE * sizeof(pmentry_type)));

extern struct pagemap sys_pmap;

struct pagemap* pmap_create(void) __nodiscard;
void pmap_release(struct pagemap* restrict pmap);

int pmap_map(struct pagemap* restrict pmap, uintptr_t virt, uintptr_t phys, unsigned vprot);
int pmap_map_range(struct pagemap* restrict pmap, uintptr_t virt, uintptr_t phys, size_t size, unsigned vprot);
int pmap_patch(struct pagemap* restrict pmap, uintptr_t virt, unsigned vprot);
int pmap_patch_noflush(struct pagemap* restrict pmap, uintptr_t virt, unsigned vprot);
int pmap_unmap(struct pagemap* restrict pmap, uintptr_t virt);
int pmap_unmap_noflush(struct pagemap* restrict pmap, uintptr_t virt);
int pmap_unmap_range_noflush(struct pagemap* restrict pmap, uintptr_t virt, size_t size);

uintptr_t pmap_resolve_address(struct pagemap* restrict pmap, uintptr_t virt);
unsigned pmap_resolve_vprot(struct pagemap* restrict pmap, uintptr_t virt);
