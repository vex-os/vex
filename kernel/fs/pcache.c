#include <vex/assert.h>
#include <vex/atomic.h>
#include <vex/blk_device.h>
#include <vex/errno.h>
#include <vex/event.h>
#include <vex/hart.h>
#include <vex/hhdm.h>
#include <vex/initcall.h>
#include <vex/mount.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/pcache.h>
#include <vex/pframe.h>
#include <vex/pslab.h>
#include <vex/sched.h>
#include <vex/spinlock.h>
#include <vex/string.h>
#include <vex/thread.h>
#include <vex/vnode.h>

#define GATHER_BATCH 16

static spinlock_type lru_lock = SPINLOCK_INIT;
static struct pframe* lru_head = nullptr;
static struct pframe* lru_tail = nullptr;
static size_t lru_length = 0;

static spinlock_type dirty_lock = SPINLOCK_INIT;
static struct vnode* dirty_head = nullptr;

static size_t dirty_pages = 0;
static size_t dirty_limit = 0;

static struct event writeback_wakeup;
static bool writeback_kicked = false;

static void lru_append(struct pframe* restrict pf)
{
    pf->lru_prev = lru_tail;
    pf->lru_next = nullptr;

    if(lru_tail) {
        lru_tail->lru_next = pf;
    }
    else {
        lru_head = pf;
    }

    lru_tail = pf;
    lru_length += 1;
}

static void lru_unlink(struct pframe* restrict pf)
{
    if(pf->lru_prev) {
        pf->lru_prev->lru_next = pf->lru_next;
    }
    else {
        lru_head = pf->lru_next;
    }

    if(pf->lru_next) {
        pf->lru_next->lru_prev = pf->lru_prev;
    }
    else {
        lru_tail = pf->lru_prev;
    }

    pf->lru_prev = nullptr;
    pf->lru_next = nullptr;
    lru_length -= 1;
}

static void* page_data(const struct pframe* restrict pf)
{
    return phys_to_virt(pframe_phys(pf));
}

static size_t valid_bytes(uint64_t index, off_t size)
{
    uint64_t start = index * PAGE_SIZE;
    uint64_t end = size;

    if(end <= start) {
        return 0;
    }

    if(end - start >= PAGE_SIZE) {
        return PAGE_SIZE;
    }

    return end - start;
}

static void queue_dirty(struct vnode* restrict node)
{
    struct pcache* cache = &node->pcache;

    spinlock_acquire(&dirty_lock);

    if(!cache->queued) {
        vnode_incref(node);
        cache->queued = true;
        cache->dirty_next = dirty_head;
        dirty_head = node;
    }

    spinlock_release(&dirty_lock);
}

static void mark_dirty(struct vnode* restrict node, struct pframe* restrict pf)
{
    struct pcache* cache = &node->pcache;

    if(pf->flags & PFRAME_DIRTY) {
        return;
    }

    pf->flags |= PFRAME_DIRTY;
    cache->num_dirty += 1;
    atomic_fetch_add(&dirty_pages, 1, ATOMIC_RELAXED);

    queue_dirty(node);
}

static void mark_clean(struct pcache* restrict cache, struct pframe* restrict pf)
{
    if(0 == (pf->flags & PFRAME_DIRTY)) {
        return;
    }

    pf->flags &= ~PFRAME_DIRTY;
    cache->num_dirty -= 1;
    atomic_fetch_sub(&dirty_pages, 1, ATOMIC_RELAXED);
}

static struct pframe* add_page(struct pcache* restrict cache, uint64_t index, uintptr_t phys)
{
    memset(phys_to_virt(phys), 0, PAGE_SIZE);

    struct pframe* pf = pframe_of(phys);
    pf->owner = cache;
    pf->index = index;
    pf->flags |= PFRAME_CACHE;

    if(radix_insert(&cache->pages, index, pf) < 0) {
        pf->flags &= ~PFRAME_CACHE;
        pf->owner = nullptr;
        pslab_free_phys(phys);
        return nullptr;
    }

    cache->num_pages += 1;

    spinlock_acquire(&lru_lock);
    lru_append(pf);
    spinlock_release(&lru_lock);

    return pf;
}

static struct pframe* get_page(struct pcache* restrict cache, uint64_t index)
{
    struct pframe* pf = radix_lookup(&cache->pages, index);

    if(pf) {
        return pf;
    }

    mutex_release(&cache->lock);
    uintptr_t phys = pslab_alloc_phys();
    mutex_acquire(&cache->lock);

    if(phys == 0) {
        return nullptr;
    }

    return add_page(cache, index, phys);
}

static void remove_page(struct pcache* restrict cache, struct pframe* restrict pf)
{
    spinlock_acquire(&lru_lock);
    lru_unlink(pf);
    spinlock_release(&lru_lock);

    mark_clean(cache, pf);

    struct pframe* removed = radix_remove(&cache->pages, pf->index);
    kassert(removed == pf);

    cache->num_pages -= 1;

    pf->flags &= ~(PFRAME_CACHE | PFRAME_UPTODATE | PFRAME_ACCESSED);
    pf->owner = nullptr;
    pf->index = 0;

    pslab_free_phys(pframe_phys(pf));
}

static bool sector_size_valid(const struct blk_device* restrict device)
{
    return device->sector_size && device->sector_size <= PAGE_SIZE && 0 == (PAGE_SIZE % device->sector_size);
}

static int fill_page(struct vnode* restrict node, pcache_bmap_type bmap, struct pframe* restrict pf, off_t size)
{
    const struct blk_device* device = node->fs->device;
    size_t sector_size = device->sector_size;
    uint8_t* data = page_data(pf);

    size_t valid = valid_bytes(pf->index, size);
    size_t num_sectors = (valid + sector_size - 1) / sector_size;
    uint64_t first = pf->index * (PAGE_SIZE / sector_size);

    memset(data, 0, PAGE_SIZE);

    for(size_t i = 0; i < num_sectors;) {
        struct pcache_extent extent;
        int retval = bmap(node, first + i, false, &extent);

        if(retval < 0) {
            return retval;
        }

        if(extent.count == 0) {
            return -EIO;
        }

        size_t count = extent.count;

        if(count > num_sectors - i) {
            count = num_sectors - i;
        }

        if(extent.mapped) {
            retval = blk_read(device, extent.lba, count, data + i * sector_size);

            if(retval < 0) {
                return retval;
            }
        }

        i += count;
    }

    // The last sector may run past the end of file
    memset(data + valid, 0, PAGE_SIZE - valid);

    pf->flags |= PFRAME_UPTODATE;

    return 0;
}

static int allocate_span(struct vnode* restrict node, pcache_bmap_type bmap, uint64_t pos, size_t length)
{
    size_t sector_size = node->fs->device->sector_size;
    uint64_t sector = pos / sector_size;
    uint64_t end = (pos + length + sector_size - 1) / sector_size;

    while(sector < end) {
        struct pcache_extent extent;
        int retval = bmap(node, sector, true, &extent);

        if(retval < 0) {
            return retval;
        }

        if(extent.count == 0 || !extent.mapped) {
            return -EIO;
        }

        sector += extent.count;
    }

    return 0;
}

static int write_page(struct vnode* restrict node, pcache_bmap_type bmap, struct pframe* restrict pf, off_t size)
{
    const struct blk_device* device = node->fs->device;
    size_t sector_size = device->sector_size;
    const uint8_t* data = page_data(pf);

    size_t valid = valid_bytes(pf->index, size);
    size_t num_sectors = (valid + sector_size - 1) / sector_size;
    uint64_t first = pf->index * (PAGE_SIZE / sector_size);

    for(size_t i = 0; i < num_sectors;) {
        struct pcache_extent extent;
        int retval = bmap(node, first + i, false, &extent);

        if(retval < 0) {
            return retval;
        }

        if(extent.count == 0) {
            return -EIO;
        }

        size_t count = extent.count;

        if(count > num_sectors - i) {
            count = num_sectors - i;
        }

        // A hole stays a hole; the sectors
        // written to were allocated by pcache_write
        if(extent.mapped) {
            retval = blk_write(device, extent.lba, count, data + i * sector_size);

            if(retval < 0) {
                return retval;
            }
        }

        i += count;
    }

    return 0;
}

static int writeback_locked(struct vnode* restrict node, pcache_bmap_type bmap, off_t size)
{
    struct pcache* cache = &node->pcache;
    int retval = 0;

    void* batch[GATHER_BATCH];
    uint64_t indices[GATHER_BATCH];
    uint64_t start = 0;
    size_t count;

    while(cache->num_dirty > 0 && (count = radix_gather(&cache->pages, start, batch, indices, GATHER_BATCH)) > 0) {
        for(size_t i = 0; i < count; ++i) {
            struct pframe* pf = batch[i];

            if(0 == (pf->flags & PFRAME_DIRTY)) {
                continue;
            }

            int page_retval = write_page(node, bmap, pf, size);

            if(page_retval < 0) {
                if(retval == 0) {
                    retval = page_retval;
                }

                continue;
            }

            mark_clean(cache, pf);
        }

        if(indices[count - 1] == UINT64_MAX) {
            break;
        }

        start = indices[count - 1] + 1;
    }

    return retval;
}

static void kick_writeback(void)
{
    if(!atomic_exchange(&writeback_kicked, true, ATOMIC_ACQ_REL)) {
        event_trigger(&writeback_wakeup, true);
    }
}

static void __noreturn writeback_thread(void* restrict arg)
{
    for(;;) {
        struct event* wakeup = &writeback_wakeup;
        event_await_timeout(&wakeup, 1, UINT64_C(1000000) * CONFIG_PCACHE_WRITEBACK_MSEC);

        atomic_store(&writeback_kicked, false, ATOMIC_RELEASE);

        vfs_sync_all();
    }
}

void pcache_init(struct pcache* restrict cache)
{
    mutex_init(&cache->lock);
    radix_init(&cache->pages);
    cache->num_pages = 0;
    cache->num_dirty = 0;
    cache->queued = false;
    cache->dirty_next = nullptr;
}

void pcache_deinit(struct pcache* restrict cache)
{
    kassert(!cache->queued);

    mutex_acquire(&cache->lock);

    void* batch[GATHER_BATCH];
    size_t count;

    while((count = radix_gather(&cache->pages, 0, batch, nullptr, GATHER_BATCH)) > 0) {
        for(size_t i = 0; i < count; ++i) {
            remove_page(cache, batch[i]);
        }
    }

    mutex_release(&cache->lock);

    radix_deinit(&cache->pages);
    mutex_deinit(&cache->lock);
}

ssize_t pcache_read(struct vnode* restrict node, pcache_bmap_type bmap, void* restrict buffer, size_t sz, off_t offset, off_t size)
{
    if(offset < 0) {
        return -EINVAL;
    }

    if(!sector_size_valid(node->fs->device)) {
        return -EINVAL;
    }

    if(offset >= size) {
        return 0;
    }

    if(sz > (uint64_t)(size - offset)) {
        sz = size - offset;
    }

    struct pcache* cache = &node->pcache;
    size_t done = 0;
    int retval = 0;

    mutex_acquire(&cache->lock);

    while(done < sz) {
        uint64_t pos = offset + done;
        uint64_t index = pos / PAGE_SIZE;
        size_t in_page = pos % PAGE_SIZE;
        size_t chunk = PAGE_SIZE - in_page;

        if(chunk > sz - done) {
            chunk = sz - done;
        }

        struct pframe* pf = get_page(cache, index);

        if(pf == nullptr) {
            retval = -ENOMEM;
            break;
        }

        if(0 == (pf->flags & PFRAME_UPTODATE)) {
            retval = fill_page(node, bmap, pf, size);

            if(retval < 0) {
                remove_page(cache, pf);
                break;
            }
        }

        pf->flags |= PFRAME_ACCESSED;

        memcpy((uint8_t*)(buffer) + done, (const uint8_t*)(page_data(pf)) + in_page, chunk);

        done += chunk;
    }

    mutex_release(&cache->lock);

    if(done == 0 && retval < 0) {
        return retval;
    }

    return done;
}

ssize_t pcache_write(struct vnode* restrict node, pcache_bmap_type bmap, const void* restrict buffer, size_t sz, off_t offset, off_t size)
{
    if(offset < 0) {
        return -EINVAL;
    }

    if(!sector_size_valid(node->fs->device)) {
        return -EINVAL;
    }

    struct pcache* cache = &node->pcache;
    size_t done = 0;
    int retval = 0;

    mutex_acquire(&cache->lock);

    while(done < sz) {
        uint64_t pos = offset + done;
        uint64_t index = pos / PAGE_SIZE;
        size_t in_page = pos % PAGE_SIZE;
        size_t chunk = PAGE_SIZE - in_page;

        if(chunk > sz - done) {
            chunk = sz - done;
        }

        struct pframe* pf = get_page(cache, index);

        if(pf == nullptr) {
            retval = -ENOMEM;
            break;
        }

        // A page written whole needs nothing from the disk
        if(0 == (pf->flags & PFRAME_UPTODATE) && chunk < PAGE_SIZE) {
            retval = fill_page(node, bmap, pf, size);

            if(retval < 0) {
                remove_page(cache, pf);
                break;
            }
        }

        retval = allocate_span(node, bmap, pos, chunk);

        if(retval < 0) {
            if(0 == (pf->flags & PFRAME_UPTODATE)) {
                remove_page(cache, pf);
            }

            break;
        }

        memcpy((uint8_t*)(page_data(pf)) + in_page, (const uint8_t*)(buffer) + done, chunk);

        pf->flags |= PFRAME_UPTODATE | PFRAME_ACCESSED;
        mark_dirty(node, pf);

        done += chunk;

        if((off_t)(pos + chunk) > size) {
            size = pos + chunk;
        }
    }

    // The writer pays for going over the limit with its own pages;
    // it already holds what the filesystem needs for writeback
    if(atomic_load(&dirty_pages, ATOMIC_RELAXED) > dirty_limit) {
        kick_writeback();
        writeback_locked(node, bmap, size);
    }

    mutex_release(&cache->lock);

    if(done == 0 && retval < 0) {
        return retval;
    }

    return done;
}

int pcache_writeback(struct vnode* restrict node, pcache_bmap_type bmap, off_t size)
{
    struct pcache* cache = &node->pcache;

    mutex_acquire(&cache->lock);
    int retval = writeback_locked(node, bmap, size);
    mutex_release(&cache->lock);

    return retval;
}

void pcache_truncate(struct vnode* restrict node, off_t size)
{
    struct pcache* cache = &node->pcache;
    uint64_t keep = ((uint64_t)(size) + PAGE_SIZE - 1) / PAGE_SIZE;

    mutex_acquire(&cache->lock);

    void* batch[GATHER_BATCH];
    size_t count;

    while((count = radix_gather(&cache->pages, keep, batch, nullptr, GATHER_BATCH)) > 0) {
        for(size_t i = 0; i < count; ++i) {
            remove_page(cache, batch[i]);
        }
    }

    // Pages hold zeroes past the end of file, so
    // a later extension does not bring back cut data
    size_t tail = size % PAGE_SIZE;

    if(tail) {
        struct pframe* pf = radix_lookup(&cache->pages, size / PAGE_SIZE);

        if(pf) {
            memset((uint8_t*)(page_data(pf)) + tail, 0, PAGE_SIZE - tail);
        }
    }

    mutex_release(&cache->lock);
}

int pcache_sync(struct vfs* restrict fs)
{
    size_t pending = 0;

    spinlock_acquire(&dirty_lock);

    for(struct vnode* node = dirty_head; node; node = node->pcache.dirty_next) {
        if(fs == nullptr || node->fs == fs) {
            pending += 1;
        }
    }

    spinlock_release(&dirty_lock);

    int retval = 0;

    // Vnodes queued again while this runs wait for the next
    // sync, so a steady writer can't keep the loop going
    while(pending > 0) {
        struct vnode* picked = nullptr;

        spinlock_acquire(&dirty_lock);

        for(struct vnode** prev = &dirty_head; *prev; prev = &(*prev)->pcache.dirty_next) {
            struct vnode* node = *prev;

            if(fs == nullptr || node->fs == fs) {
                *prev = node->pcache.dirty_next;
                node->pcache.dirty_next = nullptr;
                node->pcache.queued = false;
                picked = node;
                break;
            }
        }

        spinlock_release(&dirty_lock);

        if(picked == nullptr) {
            break;
        }

        int node_retval = vnode_fsync(picked);

        if(node_retval < 0 && retval == 0) {
            retval = node_retval;
        }

        vnode_decref(picked);
        pending -= 1;
    }

    return retval;
}

size_t pcache_reclaim(size_t count)
{
    struct thread* thrd = this_thread();

    if(!interrupt_state() || thrd->preempt_count > 0) {
        return 0;
    }

    size_t freed = 0;

    spinlock_acquire(&lru_lock);

    size_t budget = lru_length;

    while(freed < count && budget > 0 && lru_head) {
        struct pframe* pf = lru_head;
        struct pcache* cache = pf->owner;

        budget -= 1;

        lru_unlink(pf);
        lru_append(pf);

        if(mutex_held(&cache->lock) || !mutex_try_acquire(&cache->lock)) {
            continue;
        }

        if(pf->flags & PFRAME_ACCESSED) {
            pf->flags &= ~PFRAME_ACCESSED;
            mutex_release(&cache->lock);
            continue;
        }

        if((pf->flags & PFRAME_DIRTY) || pframe_shared(pf)) {
            mutex_release(&cache->lock);
            continue;
        }

        lru_unlink(pf);

        spinlock_release(&lru_lock);

        struct pframe* removed = radix_remove(&cache->pages, pf->index);
        kassert(removed == pf);

        cache->num_pages -= 1;

        pf->flags &= ~(PFRAME_CACHE | PFRAME_UPTODATE);
        pf->owner = nullptr;
        pf->index = 0;

        mutex_release(&cache->lock);

        pslab_free_phys(pframe_phys(pf));
        freed += 1;

        spinlock_acquire(&lru_lock);
    }

    spinlock_release(&lru_lock);

    return freed;
}

static void init_pcache(void)
{
    dirty_limit = pslab_page_count() / 100 * CONFIG_PCACHE_DIRTY_PERCENT;

    event_init(&writeback_wakeup);

    struct thread* writeback = create_thread_kern(&writeback_thread, nullptr);

    if(writeback == nullptr) {
        panic("pcache: out of memory");
    }

    sched_enqueue(writeback);
}

initcall_bsp_vfs(init_pcache, INIT_FIRST);
