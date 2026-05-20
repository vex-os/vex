#ifndef CBE16F75_0279_489A_A726_4EE92C6FEFF2
#define CBE16F75_0279_489A_A726_4EE92C6FEFF2
#pragma once

#include <arch/vm.h>
#include <mm/vprot.h>
#include <stdint.h>

struct vmm_pagemap {
    vm_entry_t* virt_kern;
    vm_entry_t* virt_user;
    uintptr_t phys_kern;
    uintptr_t phys_user;
    unsigned int levels;
};

extern struct vmm_pagemap sys_pagemap;

extern struct vmm_pagemap* vmm_create(void) __nodiscard;
extern struct vmm_pagemap* vmm_fork(const struct vmm_pagemap* restrict stem) __nodiscard;
extern struct vmm_pagemap* vmm_clone(const struct vmm_pagemap* restrict stem) __nodiscard;
extern void vmm_destroy(struct vmm_pagemap* restrict vm);
extern void vmm_switch(const struct vmm_pagemap* restrict vm);

extern int vmm_map(struct vmm_pagemap* restrict vm, uintptr_t virt, uintptr_t phys, unsigned int vprot);
extern int vmm_patch(struct vmm_pagemap* restrict vm, uintptr_t virt, unsigned int vprot);
extern int vmm_unmap(struct vmm_pagemap* restrict vm, uintptr_t virt);

extern uintptr_t vmm_resolve(const struct vmm_pagemap* restrict vm, uintptr_t virt);

#endif /* CBE16F75_0279_489A_A726_4EE92C6FEFF2 */
