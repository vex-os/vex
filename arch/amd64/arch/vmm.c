#include <arch/vmm.h>
#include <core/assert.h>
#include <core/errno.h>
#include <core/panic.h>
#include <lib/string.h>
#include <limine.h>
#include <mm/hhdm.h>
#include <mm/page.h>
#include <mm/pslab.h>

static volatile __used __section(".limine_requests") struct limine_paging_mode_request paging_mode = {
    .id = LIMINE_PAGING_MODE_REQUEST_ID,
    .mode = LIMINE_PAGING_MODE_X86_64_5LVL,
    .revision = 2,
};

unsigned vmm_detect_levels(void)
{
    if(paging_mode.response == nullptr) {
        panic("vmm: bootloader did not negotiate paging mode");
    }

    if(paging_mode.response->mode >= LIMINE_PAGING_MODE_X86_64_5LVL) {
        return 5;
    }

    if(paging_mode.response->mode >= LIMINE_PAGING_MODE_X86_64_4LVL) {
        return 4;
    }

    return 3;
}

int vmm_pagemap_alloc(struct vmm_pagemap* restrict vm)
{
    vm->amd64.cr3_phys = pslab_alloc();

    if(vm->amd64.cr3_phys == 0) {
        return -ENOMEM;
    }

    vm->amd64.cr3_virt = phys_to_virt(vm->amd64.cr3_phys);

    return 0;
}

void vmm_pagemap_free(struct vmm_pagemap* restrict vm)
{
    pslab_free_virt(vm->amd64.cr3_virt);

    vm->amd64.cr3_phys = 0;
    vm->amd64.cr3_virt = nullptr;
}

void vmm_pagemap_zero(struct vmm_pagemap* restrict vm)
{
    memset(vm->amd64.cr3_virt, 0, PAGE_SIZE);
}

void vmm_pagemap_copy_kernel(struct vmm_pagemap* restrict dest, const struct vmm_pagemap* restrict stem)
{
    size_t i;

    for(i = AMD64_VMM_TABLE_KERN; i < VMM_TABLE_SIZE; ++i) {
        dest->amd64.cr3_virt[i] = stem->amd64.cr3_virt[i];
    }
}
