#ifndef CBE16F75_0279_489A_A726_4EE92C6FEFF2
#define CBE16F75_0279_489A_A726_4EE92C6FEFF2
#pragma once

#include <arch/paging.h>
#include <mm/vprot.h>
#include <stdint.h>

struct pagemap {
    pmentry_t* virt;
    uintptr_t phys;
};

extern struct pagemap sys_pagemap;

extern struct pagemap* vmm_create(void) __nodiscard;
extern struct pagemap* vmm_fork(const struct pagemap* restrict stem) __nodiscard;
extern struct pagemap* vmm_clone(const struct pagemap* restrict stem) __nodiscard;
extern void vmm_destroy(struct pagemap* restrict vm);
extern void vmm_switch(const struct pagemap* restrict vm);

extern int vmm_map(struct pagemap* restrict vm, uintptr_t virt, uintptr_t phys, unsigned int vprot);
extern int vmm_patch(struct pagemap* restrict vm, uintptr_t virt, unsigned int vprot);
extern int vmm_unmap(struct pagemap* restrict vm, uintptr_t virt);

extern uintptr_t vmm_resolve(const struct pagemap* restrict vm, uintptr_t virt);

#endif /* CBE16F75_0279_489A_A726_4EE92C6FEFF2 */
