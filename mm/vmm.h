#ifndef CBE16F75_0279_489A_A726_4EE92C6FEFF2
#define CBE16F75_0279_489A_A726_4EE92C6FEFF2
#pragma once

#include <arch/limits.h>
#include <arch/vmm.h>
#include <core/types.h>
#include <mm/vprot.h>

static_assert((VMM_TABLE_SIZE * sizeof(vmm_entry_type)) == PAGE_SIZE);

extern struct vmm_pagemap sys_pagemap;

extern struct vmm_pagemap* vmm_create(void) __nodiscard;
extern struct vmm_pagemap* vmm_fork(const struct vmm_pagemap* restrict stem) __nodiscard;  // shallow copy-on-write clone
extern struct vmm_pagemap* vmm_clone(const struct vmm_pagemap* restrict stem) __nodiscard; // deep copy except kernel mappings
extern void vmm_destroy(struct vmm_pagemap* restrict vm);

extern int vmm_map(struct vmm_pagemap* restrict vm, uintptr_t virt, uintptr_t phys, unsigned vprot);
extern int vmm_patch(struct vmm_pagemap* restrict vm, uintptr_t virt, unsigned vprot);
extern int vmm_unmap(struct vmm_pagemap* restrict vm, uintptr_t virt);

extern uintptr_t vmm_resolve(const struct vmm_pagemap* restrict vm, uintptr_t virt);

#endif /* CBE16F75_0279_489A_A726_4EE92C6FEFF2 */
