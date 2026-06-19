#ifndef CBE16F75_0279_489A_A726_4EE92C6FEFF2
#define CBE16F75_0279_489A_A726_4EE92C6FEFF2
#pragma once

#include <arch/pmap.h>
#include <vex/limits.h>
#include <vex/types.h>
#include <vex/vprot.h>

static_assert((PAGEMAP_SIZE * sizeof(pmentry_type)) == PAGE_SIZE);

struct page_fault_info {
    uintptr_t fault;
    unsigned vprot;
    bool present;
};

extern struct pagemap sys_pmap;

struct pagemap* pmap_create(void) __nodiscard;
void pmap_release(struct pagemap* restrict pmap);

int pmap_map(struct pagemap* restrict pmap, uintptr_t virt, uintptr_t phys, unsigned vprot);
int pmap_patch(struct pagemap* restrict pmap, uintptr_t virt, unsigned vprot);
int pmap_unmap(struct pagemap* restrict pmap, uintptr_t virt);

uintptr_t pmap_resolve_address(struct pagemap* restrict pmap, uintptr_t virt);
unsigned pmap_resolve_vprot(struct pagemap* restrict pmap, uintptr_t virt);

struct pagemap* handle_page_fault(struct pagemap* restrict pmap, const struct page_fault_info* restrict info);

size_t copy_to_pmap(struct pagemap* restrict pmap, uintptr_t v_dst, const void* restrict s_src, size_t sz);
size_t copy_from_pmap(void* restrict s_dst, struct pagemap* restrict pmap, uintptr_t v_src, size_t sz);

#endif /* CBE16F75_0279_489A_A726_4EE92C6FEFF2 */
