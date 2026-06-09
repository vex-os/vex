#ifndef CBE16F75_0279_489A_A726_4EE92C6FEFF2
#define CBE16F75_0279_489A_A726_4EE92C6FEFF2
#pragma once

#include <vex.arch/vmm.h>
#include <vex/limits.h>
#include <vex/types.h>
#include <vex/vprot.h>

static_assert((PAGEMAP_SIZE * sizeof(pmentry_type)) == PAGE_SIZE);

struct vmm_fault_info {
    uintptr_t fault;
    unsigned vprot;
    bool present;
};

extern struct pagemap sys_pagemap;

struct pagemap* vmm_create(void) __nodiscard;
struct pagemap* vmm_fork(struct pagemap* restrict stem) __nodiscard;
struct pagemap* vmm_clone(const struct pagemap* restrict stem) __nodiscard;
void vmm_release(struct pagemap* restrict vm);

int vmm_map(struct pagemap* restrict vm, uintptr_t virt, uintptr_t phys, unsigned vprot);
int vmm_patch(struct pagemap* restrict vm, uintptr_t virt, unsigned vprot);
int vmm_unmap(struct pagemap* restrict vm, uintptr_t virt);

uintptr_t vmm_resolve(const struct pagemap* restrict vm, uintptr_t virt);

struct pagemap* vmm_handle_fault(struct pagemap* restrict vm, const struct vmm_fault_info* restrict info);

#endif /* CBE16F75_0279_489A_A726_4EE92C6FEFF2 */
