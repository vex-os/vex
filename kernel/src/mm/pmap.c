#include <vex/errno.h>
#include <vex/hart.h>
#include <vex/hhdm.h>
#include <vex/initcall.h>
#include <vex/ireq.h>
#include <vex/kbase.h>
#include <vex/kprintf.h>
#include <vex/linker.h>
#include <vex/memmap.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/pmap.h>
#include <vex/pslab.h>
#include <vex/slab.h>
#include <vex/spinlock.h>
#include <vex/string.h>

static_assert(sizeof(struct pagemap) <= (PAGE_SIZE / 2));

static unsigned flush_ipi_ireq;

struct pagemap sys_pmap;

static void flush_ipi_handler(unsigned number, const struct hart_context* restrict ctx, void* restrict ctxdata)
{
    pmap_flush_all();
}

static size_t pte_index(uintptr_t virt, uintptr_t shift)
{
    return (virt >> shift) & PMENTRY_LEVEL_MASK;
}

static pmentry_type* pmentry_get(pmentry_type* restrict table, size_t index, bool allocate)
{
    if(pmentry_valid(table[index])) {
        return phys_to_virt(pmentry_address(table[index]));
    }

    if(allocate) {
        uintptr_t phys = pslab_alloc_phys();

        if(phys) {
            pmentry_type* child = phys_to_virt(phys);
            memset(child, 0, PAGE_SIZE);
            table[index] = pmentry_setup(phys, VPROT_URWX);
            return child;
        }
    }

    return nullptr;
}

static pmentry_type* pmentry_walk(struct pagemap* restrict pmap, uintptr_t virt, bool allocate)
{
    pmentry_type* table = pmap_root(pmap, virt);

    if(pmap->num_levels >= 5) {
        size_t index = pte_index(virt, PMENTRY_LVL5_SHIFT);
        table = pmentry_get(table, index, allocate);

        if(table == nullptr) {
            return nullptr;
        }
    }

    if(pmap->num_levels >= 4) {
        size_t index = pte_index(virt, PMENTRY_LVL4_SHIFT);
        table = pmentry_get(table, index, allocate);

        if(table == nullptr) {
            return nullptr;
        }
    }

    if(pmap->num_levels >= 3) {
        size_t index = pte_index(virt, PMENTRY_LVL3_SHIFT);
        table = pmentry_get(table, index, allocate);

        if(table == nullptr) {
            return nullptr;
        }
    }

    size_t index = pte_index(virt, PMENTRY_LVL2_SHIFT);
    table = pmentry_get(table, index, allocate);

    if(table == nullptr) {
        return nullptr;
    }

    index = pte_index(virt, PMENTRY_LVL1_SHIFT);
    return &table[index];
}

static void pmap_table_free(pmentry_type* restrict table, size_t begin, size_t end, unsigned depth)
{
    if(depth) {
        for(size_t i = begin; i < end; ++i) {
            if(pmentry_valid(table[i])) {
                pmentry_type* child = phys_to_virt(pmentry_address(table[i]));
                pmap_table_free(child, 0, PAGEMAP_SIZE, depth - 1);
                pslab_free_phys(virt_to_phys(child));
            }
        }
    }
}

static uintptr_t pmap_resolve_address_nolock(struct pagemap* restrict pmap, uintptr_t virt)
{
    pmentry_type* entry = pmentry_walk(pmap, virt, false);

    if(entry && pmentry_valid(*entry)) {
        return pmentry_address(*entry) + (virt & (PAGE_SIZE - 1));
    }

    return 0;
}

struct pagemap* pmap_create(void)
{
    struct pagemap* pmap = slab_alloc(sizeof(struct pagemap));

    if(pmap) {
        pmap->lock = SPINLOCK_INIT;
        pmap->num_levels = sys_pmap.num_levels;

        if(arch_pmap_alloc(pmap) == 0) {
            arch_pmap_kcopy(pmap, &sys_pmap);
            return pmap;
        }

        slab_free(pmap);
    }

    return nullptr;
}

void pmap_release(struct pagemap* restrict pmap)
{
    pmap_table_free(pmap_root(pmap, 0), 0, PAGEMAP_USER_END, pmap->num_levels - 1);
    arch_pmap_free(pmap);
    slab_free(pmap);
}

int pmap_map(struct pagemap* restrict pmap, uintptr_t virt, uintptr_t phys, unsigned vprot)
{
    bool intr_state = spinlock_acquire_intr(&pmap->lock);

    pmentry_type* entry = pmentry_walk(pmap, virt, true);
    int retval;

    if(entry == nullptr) {
        retval = -ENOMEM;
    }
    else if(pmentry_valid(*entry)) {
        retval = -EEXIST;
    }
    else {
        *entry = pmentry_setup(page_align(phys), vprot);
        pmap_flush(virt);
        retval = 0;
    }

    spinlock_release_intr(&pmap->lock, intr_state);

    return retval;
}

int pmap_patch(struct pagemap* restrict pmap, uintptr_t virt, unsigned vprot)
{
    bool intr_state = spinlock_acquire_intr(&pmap->lock);

    pmentry_type* entry = pmentry_walk(pmap, virt, false);
    int retval;

    if(entry == nullptr) {
        retval = -ENOMEM;
    }
    else if(!pmentry_valid(*entry)) {
        retval = -EINVAL;
    }
    else {
        *entry = pmentry_setup(pmentry_address(*entry), vprot);
        hart_ipi(HART_IPI_EXCEPT_ME, flush_ipi_ireq);
        pmap_flush(virt);
        retval = 0;
    }

    spinlock_release_intr(&pmap->lock, intr_state);

    return retval;
}

int pmap_unmap(struct pagemap* restrict pmap, uintptr_t virt)
{
    bool intr_state = spinlock_acquire_intr(&pmap->lock);

    pmentry_type* entry = pmentry_walk(pmap, virt, false);

    if(entry == nullptr || !pmentry_valid(*entry)) {
        spinlock_release_intr(&pmap->lock, intr_state);
        return -ENOENT;
    }
    else {
        *entry = PMENTRY_NULL;

        hart_ipi(HART_IPI_EXCEPT_ME, flush_ipi_ireq);
        pmap_flush(virt);

        spinlock_release_intr(&pmap->lock, intr_state);

        return 0;
    }
}

uintptr_t pmap_resolve_address(struct pagemap* restrict pmap, uintptr_t virt)
{
    bool intr_state = spinlock_acquire_intr(&pmap->lock);
    uintptr_t result = pmap_resolve_address_nolock(pmap, virt);
    spinlock_release_intr(&pmap->lock, intr_state);
    return result;
}

unsigned pmap_resolve_vprot(struct pagemap* restrict pmap, uintptr_t virt)
{
    bool intr_state = spinlock_acquire_intr(&pmap->lock);

    pmentry_type* entry = pmentry_walk(pmap, virt, false);
    unsigned result = 0;

    if(entry && pmentry_valid(*entry)) {
        result = pmentry_vprot(*entry);
    }

    spinlock_release_intr(&pmap->lock, intr_state);

    return result;
}

struct pagemap* handle_page_fault(struct pagemap* restrict pmap, const struct page_fault_info* restrict info)
{
    return nullptr;
}

ssize_t pmap_copyin(void* restrict dst, struct pagemap* restrict pmap, uintptr_t src, size_t sz)
{
    uint8_t* ptr = dst;

    bool intr_state = spinlock_acquire_intr(&pmap->lock);

    while(sz) {
        uintptr_t src_phys = pmap_resolve_address_nolock(pmap, src);

        if(src_phys == 0) {
            spinlock_release_intr(&pmap->lock, intr_state);
            return -EFAULT;
        }

        size_t chunk = PAGE_SIZE - page_offset(src);

        if(chunk > sz) {
            chunk = sz;
        }

        memcpy(ptr, phys_to_virt(src_phys), chunk);

        src += chunk;
        ptr += chunk;
        sz -= chunk;
    }

    spinlock_release_intr(&pmap->lock, intr_state);

    return ptr - (uint8_t*)(dst);
}

ssize_t pmap_copyin_str(char* restrict dst, struct pagemap* restrict pmap, uintptr_t src, size_t max_sz)
{
    if(max_sz == 0) {
        return -EINVAL;
    }

    char* ptr = dst;
    size_t remaining = max_sz;

    bool intr_state = spinlock_acquire_intr(&pmap->lock);

    while(remaining) {
        uintptr_t src_phys = pmap_resolve_address_nolock(pmap, src);

        if(src_phys == 0) {
            spinlock_release_intr(&pmap->lock, intr_state);
            return -EFAULT;
        }

        size_t chunk = PAGE_SIZE - (src & (PAGE_SIZE - 1));

        if(chunk > remaining) {
            chunk = remaining;
        }

        const char* page = phys_to_virt(src_phys);
        const char* null_pos = memchr(page, 0x00, chunk);

        if(null_pos) {
            size_t len = null_pos - page + 1;

            memcpy(ptr, page, len);

            spinlock_release_intr(&pmap->lock, intr_state);

            return ptr - dst + len - 1;
        }

        memcpy(ptr, page, chunk);

        src += chunk;
        ptr += chunk;
        remaining -= chunk;
    }

    spinlock_release_intr(&pmap->lock, intr_state);

    dst[max_sz - 1] = 0x00;

    return -ENAMETOOLONG;
}

ssize_t pmap_copyout(struct pagemap* restrict pmap, uintptr_t dst, const void* restrict src, size_t sz)
{
    const uint8_t* ptr = src;

    bool intr_state = spinlock_acquire_intr(&pmap->lock);

    while(sz) {
        uintptr_t dst_phys = pmap_resolve_address_nolock(pmap, dst);

        if(dst_phys == 0) {
            spinlock_release_intr(&pmap->lock, intr_state);
            return -EFAULT;
        }

        size_t chunk = PAGE_SIZE - page_offset(dst);

        if(chunk > sz) {
            chunk = sz;
        }

        memcpy(phys_to_virt(dst_phys), ptr, chunk);

        dst += chunk;
        ptr += chunk;
        sz -= chunk;
    }

    spinlock_release_intr(&pmap->lock, intr_state);

    return ptr - (const uint8_t*)(src);
}

ssize_t pmap_copyout_str(struct pagemap* restrict pmap, uintptr_t dst, const char* restrict src, size_t max_sz)
{
    if(max_sz == 0) {
        return -EINVAL;
    }

    size_t len = strnlen(src, max_sz);

    if(len < max_sz) {
        return pmap_copyout(pmap, dst, src, len + 1);
    }

    return -ENAMETOOLONG;
}

static int map_kernel_section(const void* restrict start, const void* restrict end, unsigned vprot)
{
    uintptr_t virt = page_align((uintptr_t)start);
    uintptr_t virt_end = page_align_up((uintptr_t)end);
    uintptr_t phys = virt - kbase_virt + kbase_phys;

    while(virt < virt_end) {
        int retval = pmap_map(&sys_pmap, virt, phys, vprot);

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
        int retval = pmap_map(&sys_pmap, virt, phys, VPROT_RWX);

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

    sys_pmap.lock = SPINLOCK_INIT;
    sys_pmap.num_levels = pmap_detect_levels();

    int retval = arch_pmap_alloc(&sys_pmap);

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

    pmap_switch(&sys_pmap);
}

initcall_bsp_mmu(init_mmu, INIT_MIDDLE);
