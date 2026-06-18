#include <arch/mmu.h>
#include <limine.h>
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

unsigned mmu_detect_levels(void)
{
    if(paging_mode.response == nullptr) {
        panic("mmu: bootloader did not negotiate paging mode");
    }

    if(paging_mode.response->mode >= LIMINE_PAGING_MODE_X86_64_5LVL) {
        return 5;
    }

    if(paging_mode.response->mode >= LIMINE_PAGING_MODE_X86_64_4LVL) {
        return 4;
    }

    return 3;
}

int arch_mmu_alloc(struct mmu_ctx* restrict ctx)
{
    ctx->phys = pslab_alloc_phys();

    if(ctx->phys == 0) {
        return -ENOMEM;
    }

    ctx->virt = phys_to_virt(ctx->phys);

    arch_mmu_zero(ctx);

    return 0;
}

void arch_mmu_free(struct mmu_ctx* restrict ctx)
{
    pslab_free_virt(ctx->virt);

    ctx->phys = 0;
    ctx->virt = nullptr;
}

void arch_mmu_zero(struct mmu_ctx* restrict ctx)
{
    memset(ctx->virt, 0, PAGE_SIZE);
}

void arch_mmu_kcopy(struct mmu_ctx* restrict dest, const struct mmu_ctx* restrict stem)
{
    for(size_t i = AMD64_MMU_START_KERN; i < MMU_CTX_SIZE; ++i) {
        dest->virt[i] = stem->virt[i];
    }
}
