#include <vex/errno.h>
#include <vex/hart.h>
#include <vex/hhdm.h>
#include <vex/initcall.h>
#include <vex/ireq.h>
#include <vex/kbase.h>
#include <vex/kprintf.h>
#include <vex/linker.h>
#include <vex/memmap.h>
#include <vex/mmu.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/pslab.h>
#include <vex/slab.h>
#include <vex/spinlock.h>
#include <vex/string.h>

static_assert(sizeof(struct mmu_ctx) <= (PAGE_SIZE / 2));

static unsigned flush_ipi_ireq;

struct mmu_ctx sys_mmu_ctx;

static void flush_ipi_handler(unsigned number, const struct hart_context* restrict ctx, void* restrict ctxdata)
{
    mmu_flush_all();
}

static size_t pte_index(uintptr_t virt, uintptr_t shift)
{
    return (virt >> shift) & MMU_ENTRY_LEVEL_MASK;
}

static mmu_entry_type* mmu_entry_get(mmu_entry_type* restrict table, size_t index, bool allocate)
{
    if(mmu_entry_valid(table[index])) {
        return phys_to_virt(mmu_entry_address(table[index]));
    }

    if(allocate) {
        uintptr_t phys = pslab_alloc_phys();

        if(phys) {
            mmu_entry_type* child = phys_to_virt(phys);
            memset(child, 0, PAGE_SIZE);
            table[index] = mmu_entry_setup(phys, VPROT_URWX);
            return child;
        }
    }

    return nullptr;
}

static mmu_entry_type* mmu_entry_walk(struct mmu_ctx* restrict ctx, uintptr_t virt, bool allocate)
{
    mmu_entry_type* table = mmu_root(ctx, virt);

    if(ctx->num_levels >= 5) {
        size_t index = pte_index(virt, MMU_ENTRY_LVL5_SHIFT);
        table = mmu_entry_get(table, index, allocate);

        if(table == nullptr) {
            return nullptr;
        }
    }

    if(ctx->num_levels >= 4) {
        size_t index = pte_index(virt, MMU_ENTRY_LVL4_SHIFT);
        table = mmu_entry_get(table, index, allocate);

        if(table == nullptr) {
            return nullptr;
        }
    }

    if(ctx->num_levels >= 3) {
        size_t index = pte_index(virt, MMU_ENTRY_LVL3_SHIFT);
        table = mmu_entry_get(table, index, allocate);

        if(table == nullptr) {
            return nullptr;
        }
    }

    size_t index = pte_index(virt, MMU_ENTRY_LVL2_SHIFT);
    table = mmu_entry_get(table, index, allocate);

    if(table == nullptr) {
        return nullptr;
    }

    index = pte_index(virt, MMU_ENTRY_LVL1_SHIFT);
    return &table[index];
}

static void mmu_table_free(mmu_entry_type* restrict table, size_t begin, size_t end, unsigned depth)
{
    if(depth) {
        for(size_t i = begin; i < end; ++i) {
            if(mmu_entry_valid(table[i])) {
                mmu_entry_type* child = phys_to_virt(mmu_entry_address(table[i]));
                mmu_table_free(child, 0, MMU_CTX_SIZE, depth - 1);
                pslab_free_phys(virt_to_phys(child));
            }
        }
    }
}

struct mmu_ctx* mmu_create_ctx(void)
{
    struct mmu_ctx* ctx = slab_alloc(sizeof(struct mmu_ctx));

    if(ctx) {
        ctx->lock = SPINLOCK_INIT;
        ctx->num_levels = sys_mmu_ctx.num_levels;

        if(arch_mmu_alloc(ctx) == 0) {
            arch_mmu_kcopy(ctx, &sys_mmu_ctx);
            return ctx;
        }

        slab_free(ctx);
    }

    return nullptr;
}

void mmu_release_ctx(struct mmu_ctx* restrict ctx)
{
    mmu_table_free(mmu_root(ctx, 0), 0, MMU_CTX_USER_END, ctx->num_levels - 1);
    arch_mmu_free(ctx);
    slab_free(ctx);
}

int mmu_map(struct mmu_ctx* restrict ctx, uintptr_t virt, uintptr_t phys, unsigned vprot)
{
    bool intr_state = spinlock_acquire_intr(&ctx->lock);

    mmu_entry_type* entry = mmu_entry_walk(ctx, virt, true);
    int retval;

    if(entry == nullptr) {
        retval = -ENOMEM;
    }
    else if(mmu_entry_valid(*entry)) {
        retval = -EEXIST;
    }
    else {
        *entry = mmu_entry_setup(page_align(phys), vprot);
        mmu_flush(virt);
        retval = 0;
    }

    spinlock_release_intr(&ctx->lock, intr_state);

    return retval;
}

int mmu_patch(struct mmu_ctx* restrict ctx, uintptr_t virt, unsigned vprot)
{
    bool intr_state = spinlock_acquire_intr(&ctx->lock);

    mmu_entry_type* entry = mmu_entry_walk(ctx, virt, false);
    int retval;

    if(entry == nullptr) {
        retval = -ENOMEM;
    }
    else if(!mmu_entry_valid(*entry)) {
        retval = -EINVAL;
    }
    else {
        *entry = mmu_entry_setup(mmu_entry_address(*entry), vprot);
        hart_ipi(HART_IPI_EXCEPT_ME, flush_ipi_ireq);
        mmu_flush(virt);
        retval = 0;
    }

    spinlock_release_intr(&ctx->lock, intr_state);

    return retval;
}

int mmu_unmap(struct mmu_ctx* restrict ctx, uintptr_t virt)
{
    bool intr_state = spinlock_acquire_intr(&ctx->lock);

    mmu_entry_type* entry = mmu_entry_walk(ctx, virt, false);
    int retval;

    if(entry == nullptr) {
        retval = -ENOMEM;
    }
    else if(!mmu_entry_valid(*entry)) {
        retval = -EINVAL;
    }
    else {
        *entry = MMU_ENTRY_NULL;
        hart_ipi(HART_IPI_EXCEPT_ME, flush_ipi_ireq);
        mmu_flush(virt);
        retval = 0;
    }

    spinlock_release_intr(&ctx->lock, intr_state);

    return retval;
}

uintptr_t mmu_resolve_address(struct mmu_ctx* restrict ctx, uintptr_t virt)
{
    bool intr_state = spinlock_acquire_intr(&ctx->lock);

    mmu_entry_type* entry = mmu_entry_walk(ctx, virt, false);
    uintptr_t result = 0;

    if(entry && mmu_entry_valid(*entry)) {
        result = mmu_entry_address(*entry) + (virt & (PAGE_SIZE - 1));
    }

    spinlock_release_intr(&ctx->lock, intr_state);

    return result;
}

unsigned mmu_resolve_vprot(struct mmu_ctx* restrict ctx, uintptr_t virt)
{
    bool intr_state = spinlock_acquire_intr(&ctx->lock);

    mmu_entry_type* entry = mmu_entry_walk(ctx, virt, false);
    unsigned result = 0;

    if(entry && mmu_entry_valid(*entry)) {
        result = mmu_entry_vprot(*entry);
    }

    spinlock_release_intr(&ctx->lock, intr_state);

    return result;
}

struct mmu_ctx* mmu_page_fault(struct mmu_ctx* restrict ctx, const struct page_fault_info* restrict info)
{
    return nullptr;
}

size_t mmu_memcpy_to_ctx(struct mmu_ctx* restrict ctx, uintptr_t v_dst, const void* restrict s_src, size_t sz)
{
    const uint8_t* p_src = s_src;

    while(sz > 0) {
        uintptr_t dst_phys = mmu_resolve_address(ctx, v_dst);

        if(dst_phys == 0) {
            return sz;
        }

        size_t chunk = PAGE_SIZE - (v_dst & (PAGE_SIZE - 1));

        if(chunk > sz) {
            chunk = sz;
        }

        memcpy(phys_to_virt(dst_phys), p_src, chunk);

        v_dst += chunk;
        p_src += chunk;
        sz -= chunk;
    }

    return 0;
}

size_t mmu_memcpy_from_ctx(void* restrict s_dst, struct mmu_ctx* restrict ctx, uintptr_t v_src, size_t sz)
{
    uint8_t* p_dst = s_dst;

    while(sz) {
        uintptr_t src_phys = mmu_resolve_address(ctx, v_src);

        if(src_phys == 0) {
            return sz;
        }

        size_t chunk = PAGE_SIZE - (v_src & (PAGE_SIZE - 1));

        if(chunk > sz) {
            chunk = sz;
        }

        memcpy(p_dst, phys_to_virt(src_phys), chunk);

        v_src += chunk;
        p_dst += chunk;
        sz -= chunk;
    }

    return 0;
}

static int map_kernel_section(const void* restrict start, const void* restrict end, unsigned vprot)
{
    uintptr_t virt = page_align((uintptr_t)start);
    uintptr_t virt_end = page_align_up((uintptr_t)end);
    uintptr_t phys = virt - kbase_virt + kbase_phys;

    while(virt < virt_end) {
        int retval = mmu_map(&sys_mmu_ctx, virt, phys, vprot);

        if(retval == 0) {
            virt += PAGE_SIZE;
            phys += PAGE_SIZE;
            continue;
        }

        return retval;
    }

    return 0;
}

static int map_memmap_entry(const struct limine_memmap_entry* restrict entry)
{
    uintptr_t phys = page_align(entry->base);
    uintptr_t virt = phys + hhdm_base;
    uintptr_t virt_end = page_align_up(virt + entry->length);

    while(virt < virt_end) {
        int retval = mmu_map(&sys_mmu_ctx, virt, phys, VPROT_RWX);

        if(retval == 0 || retval == -EEXIST) {
            virt += PAGE_SIZE;
            phys += PAGE_SIZE;
            continue;
        }

        return retval;
    }

    return 0;
}

static void init_mmu(void)
{
    flush_ipi_ireq = add_ireq_handler(UINT_MAX, &flush_ipi_handler, nullptr);

    if(flush_ipi_ireq == UINT_MAX) {
        panic("mmu: out of ireq numbers");
    }

    sys_mmu_ctx.lock = SPINLOCK_INIT;
    sys_mmu_ctx.num_levels = mmu_detect_levels();

    int retval = arch_mmu_alloc(&sys_mmu_ctx);

    if(retval < 0) {
        panic("mmu: out of memory");
    }

    retval = map_kernel_section(&text_start, &text_end, VPROT_RX);

    if(retval < 0) {
        panic("mmu: failed to map kernel.text");
    }

    retval = map_kernel_section(&rodata_start, &rodata_end, VPROT_READ);

    if(retval < 0) {
        panic("mmu: failed to map kernel.rodata");
    }

    retval = map_kernel_section(&data_start, &data_end, VPROT_RW);

    if(retval < 0) {
        panic("mmu: failed to map kernel.data");
    }

    for(size_t i = 0; i < memmap->entry_count; ++i) {
        retval = map_memmap_entry(memmap->entries[i]);

        if(retval < 0) {
            panic("mmu: failed to map memmap entry");
        }
    }

    mmu_switch_ctx(&sys_mmu_ctx);
}

initcall_bsp_mmu(init_mmu, INIT_MIDDLE);
