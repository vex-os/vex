#include <vex/assert.h>
#include <vex/errno.h>
#include <vex/event.h>
#include <vex/hhdm.h>
#include <vex/initcall.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/pframe.h>
#include <vex/pmap.h>
#include <vex/pslab.h>
#include <vex/rmap.h>
#include <vex/sched.h>
#include <vex/slab.h>
#include <vex/spinlock.h>
#include <vex/string.h>
#include <vex/tlb.h>
#include <vex/vms.h>
#include <vex/vnode.h>

#define FREE_BATCH 64

static spinlock_type stale_lock = SPINLOCK_INIT;
static struct vm_region* stale_regions = nullptr;
static struct event stale_wakeup;

static_assert(sizeof(struct vm_region) <= SLAB_MAX_SIZE);
static_assert(sizeof(struct vmspace) <= SLAB_MAX_SIZE);

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
    upper->node = region->node;
    upper->pgoff = region->pgoff + (virt - region->start) / PAGE_SIZE;
    upper->shared = region->shared;
    upper->may_write = region->may_write;
    upper->next = region->next;

    if(upper->node) {
        vnode_incref(upper->node);
    }

    region->end = virt;
    region->next = upper;

    return 0;
}

static bool range_covered(struct vmspace* restrict vms, uintptr_t start, uintptr_t end)
{
    uintptr_t covered = start;

    for(struct vm_region* it = find_region(vms, start); it && it->start <= covered && covered < end; it = it->next) {
        covered = it->end;
    }

    return covered >= end;
}

static bool range_valid(struct vmspace* restrict vms, uintptr_t start, uintptr_t end)
{
    if(page_offset(start) || page_offset(end)) {
        return false;
    }

    return start < end && end <= pmap_user_end(vms->pmap);
}

static void put_frame(uintptr_t phys)
{
    if(pframe_decref(pframe_of(phys))) {
        pslab_free_phys(phys);
    }
}

static void free_region(struct vm_region* restrict region, bool deferred)
{
    if(region->node == nullptr) {
        slab_free(region);
        return;
    }

    if(!deferred) {
        vnode_decref(region->node);
        slab_free(region);
        return;
    }

    bool intr_state = spinlock_acquire_intr(&stale_lock);
    region->next = stale_regions;
    stale_regions = region;
    spinlock_release_intr(&stale_lock, intr_state);

    event_trigger(&stale_wakeup, true);
}

static int unshare_locked(struct vmspace* restrict vms, const struct vm_region* restrict region, uintptr_t page)
{
    unsigned vprot = region->vprot | VPROT_USER;
    bool sole;
    uintptr_t phys = rmap_pin(vms->pmap, page, &sole);

    if(phys == 0) {
        return -EAGAIN;
    }

    if(sole) {
        int retval = 0;

        if((pmap_resolve_vprot(vms->pmap, page) & VPROT_WRITE) != (vprot & VPROT_WRITE)) {
            retval = pmap_patch_noflush(vms->pmap, page, vprot);
        }

        put_frame(phys);

        return retval;
    }

    uintptr_t copy = pslab_alloc_phys();

    if(copy == 0) {
        put_frame(phys);
        return -ENOMEM;
    }

    memcpy(phys_to_virt(copy), phys_to_virt(phys), PAGE_SIZE);

    int retval = rmap_replace(vms->pmap, page, phys, copy, vprot);

    if(retval < 0) {
        pslab_free_phys(copy);
        put_frame(phys);
        return retval;
    }

    tlb_shootdown();

    put_frame(phys); // The old mapping's
    put_frame(phys); // The pin's

    return 0;
}

static int fault_locked(struct vmspace* restrict vms, uintptr_t page, unsigned access, bool exclusive)
{
    struct vm_region* region = find_region(vms, page);

    if(region == nullptr) {
        return -EFAULT;
    }

    if((region->vprot & access) != access) {
        return -EACCES;
    }

    for(;;) {
        bool mapped = pmap_resolve_address(vms->pmap, page);

        if(mapped && !exclusive) {
            return 0;
        }

        if(mapped && !region->shared) {
            int retval = unshare_locked(vms, region, page);

            if(retval != -EAGAIN) {
                return retval;
            }

            continue;
        }

        if(region->node == nullptr) {
            break;
        }

        // A private page starts out as the file's own, and a write copies it on the next pass;
        // a shared one only becomes writable through a write fault, which marks it dirty
        uint64_t index = region->pgoff + (page - region->start) / PAGE_SIZE;
        unsigned vprot = region->vprot | VPROT_USER;

        if(!region->shared || !exclusive) {
            vprot &= ~VPROT_WRITE;
        }

        int retval = vnode_map_page(region->node, index, vms->pmap, page, vprot);

        if(retval < 0 || region->shared) {
            return retval;
        }
    }

    uintptr_t phys = pslab_alloc_phys();

    if(phys == 0) {
        return -ENOMEM;
    }

    memset(phys_to_virt(phys), 0, PAGE_SIZE);

    int retval = rmap_map(vms->pmap, page, phys, region->vprot | VPROT_USER);

    if(retval < 0) {
        pslab_free_phys(phys);
        return retval;
    }

    return 0;
}

static uintptr_t pin_locked(struct vmspace* restrict vms, uintptr_t virt, unsigned access, bool exclusive)
{
    uintptr_t page = page_align(virt);
    uintptr_t phys = 0;

    while(phys == 0) {
        if(fault_locked(vms, page, access, exclusive) < 0) {
            return 0;
        }

        phys = rmap_pin(vms->pmap, page, nullptr);
    }

    return phys;
}

static void free_batch(uintptr_t* restrict batch, size_t* restrict count, bool shootdown)
{
    if(*count == 0) {
        return;
    }

    if(shootdown) {
        tlb_shootdown();
    }

    for(size_t i = 0; i < *count; ++i) {
        put_frame(batch[i]);
    }

    *count = 0;
}

static void release_pages(struct vmspace* restrict vms, const struct vm_region* restrict region, uintptr_t* restrict batch,
    size_t* restrict count, bool shootdown)
{
    for(uintptr_t page = region->start; page < region->end; page += PAGE_SIZE) {
        uintptr_t phys = rmap_unmap(vms->pmap, page);

        if(phys == 0) {
            continue;
        }

        batch[*count] = phys;
        *count += 1;

        if(*count == FREE_BATCH) {
            free_batch(batch, count, shootdown);
        }
    }
}

static void unmap_all_locked(struct vmspace* restrict vms, bool shootdown, bool deferred)
{
    uintptr_t batch[FREE_BATCH];
    size_t count = 0;

    while(vms->regions) {
        struct vm_region* region = vms->regions;
        vms->regions = region->next;

        release_pages(vms, region, batch, &count, shootdown);
        free_region(region, deferred);
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
        free_region(region, false);
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

static struct vm_region* alloc_region(uintptr_t start, uintptr_t end, unsigned vprot, const struct vm_backing* restrict backing)
{
    struct vm_region* region = slab_alloc(sizeof(struct vm_region));

    if(region == nullptr) {
        return nullptr;
    }

    region->start = start;
    region->end = end;
    region->vprot = vprot & ~VPROT_USER;
    region->node = nullptr;
    region->pgoff = 0;
    region->shared = false;
    region->may_write = true;

    if(backing && backing->node) {
        region->node = backing->node;
        region->pgoff = backing->pgoff;
        region->shared = backing->shared;
        region->may_write = backing->may_write;
        vnode_incref(region->node);
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
    unmap_all_locked(vms, false, true);
    pmap_release(vms->pmap);
    mutex_deinit(&vms->lock);
    slab_free(vms);
}

int vms_map(struct vmspace* restrict vms, uintptr_t start, uintptr_t end, unsigned vprot, const struct vm_backing* restrict backing)
{
    if(!range_valid(vms, start, end)) {
        return -EINVAL;
    }

    struct vm_region* region = alloc_region(start, end, vprot, backing);

    if(region == nullptr) {
        return -ENOMEM;
    }

    mutex_acquire(&vms->lock);

    if(!range_free(vms, start, end)) {
        mutex_release(&vms->lock);
        free_region(region, false);
        return -EEXIST;
    }

    insert_region(vms, region);

    mutex_release(&vms->lock);

    return 0;
}

int vms_map_replace(struct vmspace* restrict vms, uintptr_t start, uintptr_t end, unsigned vprot, const struct vm_backing* restrict backing)
{
    if(!range_valid(vms, start, end)) {
        return -EINVAL;
    }

    struct vm_region* region = alloc_region(start, end, vprot, backing);

    if(region == nullptr) {
        return -ENOMEM;
    }

    mutex_acquire(&vms->lock);

    int retval = unmap_locked(vms, start, end);

    if(retval < 0) {
        mutex_release(&vms->lock);
        free_region(region, false);
        return retval;
    }

    insert_region(vms, region);

    mutex_release(&vms->lock);

    return 0;
}

int vms_map_any(struct vmspace* restrict vms, size_t size, unsigned vprot, const struct vm_backing* restrict backing,
    uintptr_t* restrict out)
{
    if(size == 0 || page_offset(size)) {
        return -EINVAL;
    }

    struct vm_region* region = alloc_region(0, 0, vprot, backing);

    if(region == nullptr) {
        return -ENOMEM;
    }

    mutex_acquire(&vms->lock);

    if(!find_gap(vms, size, &region->start)) {
        mutex_release(&vms->lock);
        free_region(region, false);
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

    if(!range_covered(vms, start, end)) {
        mutex_release(&vms->lock);
        return -ENOMEM;
    }

    for(struct vm_region* it = find_region(vms, start); it && it->start < end; it = it->next) {
        if((vprot & VPROT_WRITE) && !it->may_write) {
            mutex_release(&vms->lock);
            return -EACCES;
        }
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
            uintptr_t phys = pmap_resolve_address(vms->pmap, page);

            if(phys == 0) {
                continue;
            }

            unsigned page_vprot = vprot | VPROT_USER;

            if(it->shared || pframe_shared(pframe_of(phys))) {
                page_vprot &= ~VPROT_WRITE;
            }

            if(0 == pmap_patch_noflush(vms->pmap, page, page_vprot)) {
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

int vms_sync(struct vmspace* restrict vms, uintptr_t start, uintptr_t end, bool wait)
{
    if(!range_valid(vms, start, end)) {
        return -EINVAL;
    }

    mutex_acquire(&vms->lock);
    bool covered = range_covered(vms, start, end);
    mutex_release(&vms->lock);

    if(!covered) {
        return -ENOMEM;
    }

    int retval = 0;
    uintptr_t cursor = start;

    // One file at a time, as fsync sleeps on the filesystem
    while(wait && cursor < end) {
        struct vnode* node = nullptr;

        mutex_acquire(&vms->lock);

        for(struct vm_region* it = vms->regions; it && it->start < end && node == nullptr; it = it->next) {
            if(it->end <= cursor) {
                continue;
            }

            cursor = it->end;

            if(it->shared) {
                node = it->node;
                vnode_incref(node);
            }
        }

        mutex_release(&vms->lock);

        if(node == nullptr) {
            break;
        }

        int node_retval = vnode_fsync(node);
        vnode_decref(node);

        if(retval == 0) {
            retval = node_retval;
        }
    }

    return retval;
}

int vms_unmap_all(struct vmspace* restrict vms)
{
    mutex_acquire(&vms->lock);
    unmap_all_locked(vms, true, false);
    mutex_release(&vms->lock);
    return 0;
}

int vms_clone(struct vmspace* restrict vms, struct vmspace* restrict stem)
{
    kassert(vms->regions == nullptr);

    mutex_acquire(&stem->lock);

    bool patched = false;
    int retval = 0;

    struct vm_region** tail = &vms->regions;

    for(const struct vm_region* it = stem->regions; it; it = it->next) {
        struct vm_backing backing = {
            .node = it->node,
            .pgoff = it->pgoff,
            .shared = it->shared,
            .may_write = it->may_write,
        };

        struct vm_region* region = alloc_region(it->start, it->end, it->vprot, &backing);

        if(region == nullptr) {
            retval = -ENOMEM;
            break;
        }

        region->next = nullptr;
        *tail = region;
        tail = &region->next;

        unsigned vprot = (it->vprot & ~VPROT_WRITE) | VPROT_USER;

        for(uintptr_t page = it->start; page < it->end && retval == 0; page += PAGE_SIZE) {
            uintptr_t phys = rmap_pin(stem->pmap, page, nullptr);

            if(phys == 0) {
                continue;
            }

            retval = rmap_map(vms->pmap, page, phys, vprot);

            if(retval < 0) {
                put_frame(phys);
                break;
            }

            // Fails only when a walker took the page meanwhile
            if((it->vprot & VPROT_WRITE) && !it->shared && 0 == pmap_patch_noflush(stem->pmap, page, vprot)) {
                patched = true;
            }
        }

        if(retval < 0) {
            break;
        }
    }

    if(patched) {
        tlb_shootdown();
    }

    mutex_release(&stem->lock);

    if(retval < 0) {
        vms_unmap_all(vms);
    }

    return retval;
}

int vms_fault(struct vmspace* restrict vms, uintptr_t virt, unsigned access)
{
    if(virt >= pmap_user_end(vms->pmap)) {
        return -EFAULT;
    }

    mutex_acquire(&vms->lock);
    access &= ~VPROT_USER;
    int retval = fault_locked(vms, page_align(virt), access, 0 != (access & VPROT_WRITE));
    mutex_release(&vms->lock);

    return retval;
}

uintptr_t vms_touch(struct vmspace* restrict vms, uintptr_t virt)
{
    if(virt >= pmap_user_end(vms->pmap)) {
        return 0;
    }

    mutex_acquire(&vms->lock);

    uintptr_t phys = pin_locked(vms, virt, VPROT_NONE, true);

    if(phys) {
        put_frame(phys);
        phys += page_offset(virt);
    }

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
        uintptr_t src_phys = pin_locked(vms, src, VPROT_READ, false);

        if(src_phys == 0) {
            mutex_release(&vms->lock);
            return -EFAULT;
        }

        size_t chunk = PAGE_SIZE - page_offset(src);

        if(chunk > sz) {
            chunk = sz;
        }

        memcpy(ptr, (const uint8_t*)(phys_to_virt(src_phys)) + page_offset(src), chunk);
        put_frame(src_phys);

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
            src_phys = pin_locked(vms, src, VPROT_READ, false);
        }

        if(src_phys == 0) {
            mutex_release(&vms->lock);
            return -EFAULT;
        }

        size_t chunk = PAGE_SIZE - page_offset(src);

        if(chunk > remaining) {
            chunk = remaining;
        }

        const char* page = (const char*)(phys_to_virt(src_phys)) + page_offset(src);
        const char* null_pos = memchr(page, 0x00, chunk);

        if(null_pos) {
            size_t len = null_pos - page + 1;

            memcpy(ptr, page, len);
            put_frame(src_phys);

            mutex_release(&vms->lock);

            return ptr - dst + len - 1;
        }

        memcpy(ptr, page, chunk);
        put_frame(src_phys);

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
        uintptr_t dst_phys = pin_locked(vms, dst, VPROT_WRITE, true);

        if(dst_phys == 0) {
            mutex_release(&vms->lock);
            return -EFAULT;
        }

        size_t chunk = PAGE_SIZE - page_offset(dst);

        if(chunk > sz) {
            chunk = sz;
        }

        memcpy((uint8_t*)(phys_to_virt(dst_phys)) + page_offset(dst), ptr, chunk);
        put_frame(dst_phys);

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

static void __noreturn release_thread(void* restrict arg)
{
    for(;;) {
        struct event* wakeup = &stale_wakeup;
        event_await(&wakeup, 1);

        bool intr_state = spinlock_acquire_intr(&stale_lock);
        struct vm_region* regions = stale_regions;
        stale_regions = nullptr;
        spinlock_release_intr(&stale_lock, intr_state);

        while(regions) {
            struct vm_region* next = regions->next;
            free_region(regions, false);
            regions = next;
        }
    }
}

static void init_vms(void)
{
    event_init(&stale_wakeup);

    if(create_thread_kern_enq(&release_thread, nullptr) == nullptr) {
        panic("vms: out of memory");
    }
}

initcall_bsp_vfs(init_vms, INIT_FIRST);
