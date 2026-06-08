#include <limine.h>
#include <string.h>
#include <sys.arch/vmm.h>
#include <sys/assert.h>
#include <sys/errno.h>
#include <sys/hhdm.h>
#include <sys/page.h>
#include <sys/panic.h>
#include <sys/pslab.h>

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
    vm->amd64_cr3_phys = pslab_alloc();

    if(vm->amd64_cr3_phys == 0) {
        return -ENOMEM;
    }

    vm->amd64_cr3_virt = phys_to_virt(vm->amd64_cr3_phys);

    return 0;
}

void pagemap_free(struct pagemap* restrict vm)
{
    pslab_free_virt(vm->amd64_cr3_virt);

    vm->amd64_cr3_phys = 0;
    vm->amd64_cr3_virt = nullptr;
}

void pagemap_zero(struct pagemap* restrict vm)
{
    memset(vm->amd64_cr3_virt, 0, PAGE_SIZE);
}

void pagemap_kcopy(struct pagemap* restrict dest, const struct pagemap* restrict stem)
{
    for(size_t i = AMD64_PAGEMAP_KERN; i < PAGEMAP_SIZE; ++i) {
        dest->amd64_cr3_virt[i] = stem->amd64_cr3_virt[i];
    }
}
