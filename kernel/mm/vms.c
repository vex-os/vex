#include <vex/errno.h>
#include <vex/hhdm.h>
#include <vex/page.h>
#include <vex/pmap.h>
#include <vex/pslab.h>
#include <vex/slab.h>
#include <vex/string.h>
#include <vex/tlb.h>
#include <vex/vms.h>

#define FREE_BATCH 64

static_assert(sizeof(struct vm_region) <= (PAGE_SIZE / 2));
static_assert(sizeof(struct vmspace) <= (PAGE_SIZE / 2));

static struct vm_region* find_region(struct vmspace* restrict vms, uintptr_t virt)
{
    for(struct vm_region* it = vms->regions; it && it->start <= virt; it = it->next) {
        if(virt < it->end) {
            return it;
        }
    }

    return nullptr;
}

static bool range_free(struct vmspace* restrict vms, uintptr_t start, uintptr_t end)
{
    for(struct vm_region* it = vms->regions; it && it->start < end; it = it->next) {
        if(it->end > start) {
            return false;
        }
    }

    return true;
}

static void insert_region(struct vmspace* restrict vms, struct vm_region* restrict region)
{
    struct vm_region** prev = &vms->regions;

    while(*prev && (*prev)->start < region->start) {
        prev = &(*prev)->next;
    }

    region->next = *prev;
    *prev = region;
}

// Makes virt a region boundary, so whole
// regions cover any range starting or ending there
static int split_region(struct vmspace* restrict vms, uintptr_t virt)
{
    struct vm_region* region = find_region(vms, virt);

    if(region == nullptr || region->start == virt) {
        return 0;
    }

    struct vm_region* upper = slab_alloc(sizeof(struct vm_region));

    if(upper == nullptr) {
        return -ENOMEM;
    }

    upper->start = virt;
    upper->end = region->end;
    upper->vprot = region->vprot;
    upper->next = region->next;

    region->end = virt;
    region->next = upper;

    return 0;
}

static bool range_valid(struct vmspace* restrict vms, uintptr_t start, uintptr_t end)
{
    if(page_offset(start) || page_offset(end)) {
        return false;
    }

    return start < end && end <= pmap_user_end(vms->pmap);
}

static int fault_locked(struct vmspace* restrict vms, uintptr_t page, unsigned access)
{
    struct vm_region* region = find_region(vms, page);

    if(region == nullptr || (region->vprot & access) != access) {
        return -EFAULT;
    }

    // Another thread faulted it in first
    // or a stale TLB entry was behind the fault
    if(pmap_resolve_address(vms->pmap, page)) {
        return 0;
    }

    uintptr_t phys = pslab_alloc_phys();

    if(phys == 0) {
        return -ENOMEM;
    }

    memset(phys_to_virt(phys), 0, PAGE_SIZE);

    int retval = pmap_map(vms->pmap, page, phys, region->vprot | VPROT_USER);

    if(retval < 0) {
        pslab_free_phys(phys);
        return retval;
    }

    return 0;
}

static uintptr_t resolve_locked(struct vmspace* restrict vms, uintptr_t virt, unsigned access)
{
    uintptr_t page = page_align(virt);

    if(fault_locked(vms, page, access) < 0) {
        return 0;
    }

    return pmap_resolve_address(vms->pmap, page) + page_offset(virt);
}

static void free_batch(uintptr_t* restrict batch, size_t* restrict count, bool shootdown)
{
    if(*count == 0) {
        return;
    }

    // The pages may only be reused once
    // no hart can reach them through a stale TLB entry
    if(shootdown) {
        tlb_shootdown();
    }

    for(size_t i = 0; i < *count; ++i) {
        pslab_free_phys(batch[i]);
    }

    *count = 0;
}

static void release_pages(struct vmspace* restrict vms, const struct vm_region* restrict region, uintptr_t* restrict batch,
    size_t* restrict count, bool shootdown)
{
    for(uintptr_t page = region->start; page < region->end; page += PAGE_SIZE) {
        uintptr_t phys = pmap_resolve_address(vms->pmap, page);

        if(phys == 0) {
            continue;
        }

        pmap_unmap_noflush(vms->pmap, page);

        batch[*count] = phys;
        *count += 1;

        if(*count == FREE_BATCH) {
            free_batch(batch, count, shootdown);
        }
    }
}

static void unmap_all_locked(struct vmspace* restrict vms, bool shootdown)
{
    uintptr_t batch[FREE_BATCH];
    size_t count = 0;

    while(vms->regions) {
        struct vm_region* region = vms->regions;
        vms->regions = region->next;

        release_pages(vms, region, batch, &count, shootdown);
        slab_free(region);
    }

    free_batch(batch, &count, shootdown);
}

// Holes in the range are fine, as with munmap
static int unmap_locked(struct vmspace* restrict vms, uintptr_t start, uintptr_t end)
{
    int retval = split_region(vms, start);

    if(retval == 0) {
        retval = split_region(vms, end);
    }

    if(retval < 0) {
        return retval;
    }

    uintptr_t batch[FREE_BATCH];
    size_t count = 0;

    struct vm_region** prev = &vms->regions;

    while(*prev && (*prev)->start < end) {
        struct vm_region* region = *prev;

        if(region->start < start) {
            prev = &region->next;
            continue;
        }

        *prev = region->next;

        release_pages(vms, region, batch, &count, true);
        slab_free(region);
    }

    free_batch(batch, &count, true);

    return 0;
}

// Picks the highest gap below VMS_MMAP_TOP that fits
static bool find_gap(struct vmspace* restrict vms, size_t size, uintptr_t* restrict out)
{
    uintptr_t gap_start = VMS_MMAP_MIN;
    bool found = false;

    for(struct vm_region* it = vms->regions;; it = it->next) {
        uintptr_t gap_end = VMS_MMAP_TOP;

        if(it && it->start < VMS_MMAP_TOP) {
            gap_end = it->start;
        }

        if(gap_end > gap_start && gap_end - gap_start >= size) {
            *out = gap_end - size;
            found = true;
        }

        if(it == nullptr || it->start >= VMS_MMAP_TOP) {
            return found;
        }

        if(it->end > gap_start) {
            gap_start = it->end;
        }
    }
}

static struct vm_region* alloc_region(uintptr_t start, uintptr_t end, unsigned vprot)
{
    struct vm_region* region = slab_alloc(sizeof(struct vm_region));

    if(region) {
        region->start = start;
        region->end = end;
        region->vprot = vprot & ~VPROT_USER;
    }

    return region;
}

struct vmspace* vms_create(void)
{
    struct vmspace* vms = slab_alloc(sizeof(struct vmspace));

    if(vms == nullptr) {
        return nullptr;
    }

    vms->pmap = pmap_create();

    if(vms->pmap == nullptr) {
        slab_free(vms);
        return nullptr;
    }

    vms->regions = nullptr;
    mutex_init(&vms->lock);

    return vms;
}

void vms_release(struct vmspace* restrict vms)
{
    // Released only once no hart has these
    // page tables loaded, so no TLB holds their entries
    unmap_all_locked(vms, false);
    pmap_release(vms->pmap);
    mutex_deinit(&vms->lock);
    slab_free(vms);
}

int vms_map_anon(struct vmspace* restrict vms, uintptr_t start, uintptr_t end, unsigned vprot)
{
    if(!range_valid(vms, start, end)) {
        return -EINVAL;
    }

    struct vm_region* region = alloc_region(start, end, vprot);

    if(region == nullptr) {
        return -ENOMEM;
    }

    mutex_acquire(&vms->lock);

    if(!range_free(vms, start, end)) {
        mutex_release(&vms->lock);
        slab_free(region);
        return -EEXIST;
    }

    insert_region(vms, region);

    mutex_release(&vms->lock);

    return 0;
}

int vms_map_anon_replace(struct vmspace* restrict vms, uintptr_t start, uintptr_t end, unsigned vprot)
{
    if(!range_valid(vms, start, end)) {
        return -EINVAL;
    }

    struct vm_region* region = alloc_region(start, end, vprot);

    if(region == nullptr) {
        return -ENOMEM;
    }

    mutex_acquire(&vms->lock);

    int retval = unmap_locked(vms, start, end);

    if(retval < 0) {
        mutex_release(&vms->lock);
        slab_free(region);
        return retval;
    }

    insert_region(vms, region);

    mutex_release(&vms->lock);

    return 0;
}

int vms_map_anon_any(struct vmspace* restrict vms, size_t size, unsigned vprot, uintptr_t* restrict out)
{
    if(size == 0 || page_offset(size)) {
        return -EINVAL;
    }

    struct vm_region* region = alloc_region(0, 0, vprot);

    if(region == nullptr) {
        return -ENOMEM;
    }

    mutex_acquire(&vms->lock);

    if(!find_gap(vms, size, &region->start)) {
        mutex_release(&vms->lock);
        slab_free(region);
        return -ENOMEM;
    }

    region->end = region->start + size;
    insert_region(vms, region);

    *out = region->start;

    mutex_release(&vms->lock);

    return 0;
}

int vms_unmap(struct vmspace* restrict vms, uintptr_t start, uintptr_t end)
{
    if(!range_valid(vms, start, end)) {
        return -EINVAL;
    }

    mutex_acquire(&vms->lock);
    int retval = unmap_locked(vms, start, end);
    mutex_release(&vms->lock);

    return retval;
}

int vms_protect(struct vmspace* restrict vms, uintptr_t start, uintptr_t end, unsigned vprot)
{
    if(!range_valid(vms, start, end)) {
        return -EINVAL;
    }

    vprot &= ~VPROT_USER;

    mutex_acquire(&vms->lock);

    // Every page of the range has to be mapped
    uintptr_t covered = start;

    for(struct vm_region* it = find_region(vms, start); it && it->start <= covered && covered < end; it = it->next) {
        covered = it->end;
    }

    if(covered < end) {
        mutex_release(&vms->lock);
        return -ENOMEM;
    }

    int retval = split_region(vms, start);

    if(retval == 0) {
        retval = split_region(vms, end);
    }

    if(retval < 0) {
        mutex_release(&vms->lock);
        return retval;
    }

    bool patched = false;

    for(struct vm_region* it = find_region(vms, start); it && it->start < end; it = it->next) {
        it->vprot = vprot;

        for(uintptr_t page = it->start; page < it->end; page += PAGE_SIZE) {
            if(0 == pmap_patch_noflush(vms->pmap, page, vprot | VPROT_USER)) {
                patched = true;
            }
        }
    }

    mutex_release(&vms->lock);

    if(patched) {
        tlb_shootdown();
    }

    return 0;
}

int vms_unmap_all(struct vmspace* restrict vms)
{
    mutex_acquire(&vms->lock);
    unmap_all_locked(vms, true);
    mutex_release(&vms->lock);
    return 0;
}

int vms_fault(struct vmspace* restrict vms, uintptr_t virt, unsigned access)
{
    if(virt >= pmap_user_end(vms->pmap)) {
        return -EFAULT;
    }

    mutex_acquire(&vms->lock);
    int retval = fault_locked(vms, page_align(virt), access & ~VPROT_USER);
    mutex_release(&vms->lock);

    return retval;
}

uintptr_t vms_touch(struct vmspace* restrict vms, uintptr_t virt)
{
    if(virt >= pmap_user_end(vms->pmap)) {
        return 0;
    }

    mutex_acquire(&vms->lock);
    uintptr_t phys = resolve_locked(vms, virt, VPROT_NONE);
    mutex_release(&vms->lock);

    return phys;
}

ssize_t vms_copyin(void* restrict dst, struct vmspace* restrict vms, uintptr_t src, size_t sz)
{
    uint8_t* ptr = dst;

    if(src >= pmap_user_end(vms->pmap) || sz > pmap_user_end(vms->pmap) - src) {
        return -EFAULT;
    }

    mutex_acquire(&vms->lock);

    while(sz) {
        uintptr_t src_phys = resolve_locked(vms, src, VPROT_READ);

        if(src_phys == 0) {
            mutex_release(&vms->lock);
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

    mutex_release(&vms->lock);

    return ptr - (uint8_t*)(dst);
}

ssize_t vms_copyin_str(char* restrict dst, struct vmspace* restrict vms, uintptr_t src, size_t max_sz)
{
    if(max_sz == 0) {
        return -EINVAL;
    }

    char* ptr = dst;
    size_t remaining = max_sz;

    mutex_acquire(&vms->lock);

    while(remaining) {
        uintptr_t src_phys = 0;

        if(src < pmap_user_end(vms->pmap)) {
            src_phys = resolve_locked(vms, src, VPROT_READ);
        }

        if(src_phys == 0) {
            mutex_release(&vms->lock);
            return -EFAULT;
        }

        size_t chunk = PAGE_SIZE - page_offset(src);

        if(chunk > remaining) {
            chunk = remaining;
        }

        const char* page = phys_to_virt(src_phys);
        const char* null_pos = memchr(page, 0x00, chunk);

        if(null_pos) {
            size_t len = null_pos - page + 1;

            memcpy(ptr, page, len);

            mutex_release(&vms->lock);

            return ptr - dst + len - 1;
        }

        memcpy(ptr, page, chunk);

        src += chunk;
        ptr += chunk;
        remaining -= chunk;
    }

    mutex_release(&vms->lock);

    dst[max_sz - 1] = 0x00;

    return -ENAMETOOLONG;
}

ssize_t vms_copyout(struct vmspace* restrict vms, uintptr_t dst, const void* restrict src, size_t sz)
{
    const uint8_t* ptr = src;

    if(dst >= pmap_user_end(vms->pmap) || sz > pmap_user_end(vms->pmap) - dst) {
        return -EFAULT;
    }

    mutex_acquire(&vms->lock);

    while(sz) {
        uintptr_t dst_phys = resolve_locked(vms, dst, VPROT_WRITE);

        if(dst_phys == 0) {
            mutex_release(&vms->lock);
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

    mutex_release(&vms->lock);

    return ptr - (const uint8_t*)(src);
}

ssize_t vms_copyout_str(struct vmspace* restrict vms, uintptr_t dst, const char* restrict src, size_t max_sz)
{
    if(max_sz == 0) {
        return -EINVAL;
    }

    size_t len = strnlen(src, max_sz);

    if(len < max_sz) {
        return vms_copyout(vms, dst, src, len + 1);
    }

    return -ENAMETOOLONG;
}
