#include <limine.h>
#include <vex.arch/paging.h>
#include <vex/assert.h>
#include <vex/errno.h>
#include <vex/hhdm.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/pslab.h>
#include <vex/string.h>

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

int pagemap_alloc(struct pagemap* restrict vm)
{
    vm->phys = pslab_alloc_phys();

    if(vm->phys == 0) {
        return -ENOMEM;
    }

    vm->virt = phys_to_virt(vm->phys);

    pagemap_zero(vm);

    return 0;
}

void pagemap_free(struct pagemap* restrict vm)
{
    pslab_free_virt(vm->virt);

    vm->phys = 0;
    vm->virt = nullptr;
}

void pagemap_zero(struct pagemap* restrict vm)
{
    memset(vm->virt, 0, PAGE_SIZE);
}

void pagemap_kcopy(struct pagemap* restrict dest, const struct pagemap* restrict stem)
{
    for(size_t i = AMD64_PAGEMAP_KERN; i < PAGEMAP_SIZE; ++i) {
        dest->virt[i] = stem->virt[i];
    }
}
