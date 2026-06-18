#ifndef CBE16F75_0279_489A_A726_4EE92C6FEFF2
#define CBE16F75_0279_489A_A726_4EE92C6FEFF2
#pragma once

#include <arch/mmu.h>
#include <vex/limits.h>
#include <vex/types.h>
#include <vex/vprot.h>

static_assert((MMU_CTX_SIZE * sizeof(mmu_entry_type)) == PAGE_SIZE);

struct page_fault_info {
    uintptr_t fault;
    unsigned vprot;
    bool present;
};

extern struct mmu_ctx sys_mmu_ctx;

struct mmu_ctx* mmu_create_ctx(void) __nodiscard;
void mmu_release_ctx(struct mmu_ctx* restrict ctx);

int mmu_map(struct mmu_ctx* restrict ctx, uintptr_t virt, uintptr_t phys, unsigned vprot);
int mmu_patch(struct mmu_ctx* restrict ctx, uintptr_t virt, unsigned vprot);
int mmu_unmap(struct mmu_ctx* restrict ctx, uintptr_t virt);

uintptr_t mmu_resolve_address(struct mmu_ctx* restrict ctx, uintptr_t virt);
unsigned mmu_resolve_vprot(struct mmu_ctx* restrict ctx, uintptr_t virt);

struct mmu_ctx* mmu_page_fault(struct mmu_ctx* restrict ctx, const struct page_fault_info* restrict info);

size_t mmu_memcpy_to_ctx(struct mmu_ctx* restrict ctx, uintptr_t v_dst, const void* restrict s_src, size_t sz);
size_t mmu_memcpy_from_ctx(void* restrict s_dst, struct mmu_ctx* restrict ctx, uintptr_t v_src, size_t sz);

#endif /* CBE16F75_0279_489A_A726_4EE92C6FEFF2 */
