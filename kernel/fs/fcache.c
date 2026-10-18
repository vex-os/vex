#include <vex/assert.h>
#include <vex/atomic.h>
#include <vex/blk_device.h>
#include <vex/errno.h>
#include <vex/event.h>
#include <vex/fcache.h>
#include <vex/hart.h>
#include <vex/hhdm.h>
#include <vex/initcall.h>
#include <vex/mount.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/pframe.h>
#include <vex/pslab.h>
#include <vex/rmap.h>
#include <vex/sched.h>
#include <vex/spinlock.h>
#include <vex/string.h>
#include <vex/thread.h>
#include <vex/tlb.h>
#include <vex/vnode.h>

#define GATHER_BATCH 16

static spinlock_type lru_lock = SPINLOCK_INIT;
static struct pframe* lru_head = nullptr;
static struct pframe* lru_tail = nullptr;
static size_t lru_length = 0;

static spinlock_type dirty_lock = SPINLOCK_INIT;
static struct vnode* dirty_head = nullptr;

static size_t dirty_pages = 0;

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
    struct fcache* cache = &node->pages;

    spinlock_acquire(&dirty_lock);

    if(!cache->queued) {
        vnode_incref(node);
        cache->queued = true;
        cache->dirty_next = dirty_head;
        dirty_head = node;
    }

    spinlock_release(&dirty_lock);
}

static void mark_dirty(struct vnode* restrict node, struct pframe* restrict pf, unsigned sectors)
{
    struct fcache* cache = &node->pages;

    pf->flags |= sectors << PFRAME_DIRTY_SHIFT;

    if(pf->flags & PFRAME_DIRTY) {
        return;
    }

    pf->flags |= PFRAME_DIRTY;
    cache->num_dirty += 1;
    atomic_fetch_add(&dirty_pages, 1, ATOMIC_RELAXED);

    queue_dirty(node);
}

static void mark_clean(struct fcache* restrict cache, struct pframe* restrict pf)
{
    if(0 == (pf->flags & PFRAME_DIRTY)) {
        return;
    }

    pf->flags &= ~(PFRAME_DIRTY | (UINT32_MAX << PFRAME_DIRTY_SHIFT));
    cache->num_dirty -= 1;
    atomic_fetch_sub(&dirty_pages, 1, ATOMIC_RELAXED);
}

static struct pframe* add_page(struct fcache* restrict cache, uint64_t index, uintptr_t phys)
{
    memset(phys_to_virt(phys), 0, PAGE_SIZE);

    struct pframe* pf = pframe_of(phys);
    pf->owner = cache;
    pf->index = index;
    pf->flags |= PFRAME_CACHE;

    if(radix_insert(&cache->tree, index, pf) < 0) {
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

static struct pframe* get_page(struct fcache* restrict cache, uint64_t index)
{
    struct pframe* pf = radix_lookup(&cache->tree, index);

    if(pf) {
        return pf;
    }

    mutex_release(&cache->lock);
    uintptr_t phys = pslab_alloc_phys();
    mutex_acquire(&cache->lock);

    if(phys == 0) {
        return nullptr;
    }

    pf = radix_lookup(&cache->tree, index);

    if(pf) {
        pslab_free_phys(phys);
        return pf;
    }

    return add_page(cache, index, phys);
}

static void remove_page(struct fcache* restrict cache, struct pframe* restrict pf)
{
    spinlock_acquire(&lru_lock);
    lru_unlink(pf);
    spinlock_release(&lru_lock);

    mark_clean(cache, pf);

    struct pframe* removed = radix_remove(&cache->tree, pf->index);
    kassert(removed == pf);

    cache->num_pages -= 1;

    pf->flags &= ~(PFRAME_CACHE | PFRAME_UPTODATE | PFRAME_ACCESSED);
    pf->owner = nullptr;
    pf->index = 0;

    size_t mappings = rmap_unmap_all(pf);

    if(mappings) {
        tlb_shootdown();

        for(size_t i = 0; i < mappings; ++i) {
            bool last = pframe_decref(pf);
            kassert(!last); // The cache still holds its own
        }
    }

    if(pframe_decref(pf)) {
        pslab_free_phys(pframe_phys(pf));
    }
}

static bool sector_size_valid(const struct blk_device* restrict device)
{
    return device->sector_size && device->sector_size <= PAGE_SIZE && 0 == (PAGE_SIZE % device->sector_size);
}

static unsigned sector_span(struct vnode* restrict node, size_t in_page, size_t length)
{
    size_t sector_size = node->fs->device->sector_size;
    size_t first = in_page / sector_size;
    size_t last = (in_page + length - 1) / sector_size;

    return ((2U << last) - 1) & ~((1U << first) - 1);
}

static int fill_page(struct vnode* restrict node, fcache_bmap_type bmap, struct pframe* restrict pf, off_t size)
{
    const struct blk_device* device = node->fs->device;
    size_t sector_size = device->sector_size;
    uint8_t* data = page_data(pf);

    size_t valid = valid_bytes(pf->index, size);
    size_t num_sectors = (valid + sector_size - 1) / sector_size;
    uint64_t first = pf->index * (PAGE_SIZE / sector_size);

    memset(data, 0, PAGE_SIZE);

    for(size_t i = 0; i < num_sectors;) {
        struct fcache_extent extent;
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

static int allocate_span(struct vnode* restrict node, fcache_bmap_type bmap, uint64_t pos, size_t length)
{
    size_t sector_size = node->fs->device->sector_size;
    uint64_t sector = pos / sector_size;
    uint64_t end = (pos + length + sector_size - 1) / sector_size;

    while(sector < end) {
        struct fcache_extent extent;
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

static int write_run(struct vnode* restrict node, fcache_bmap_type bmap, uint64_t sector, size_t count, const uint8_t* restrict data)
{
    const struct blk_device* device = node->fs->device;

    while(count > 0) {
        struct fcache_extent extent;
        int retval = bmap(node, sector, false, &extent);

        if(retval < 0) {
            return retval;
        }

        if(extent.count == 0) {
            return -EIO;
        }

        size_t chunk = extent.count;

        if(chunk > count) {
            chunk = count;
        }

        // A hole stays a hole; the sectors
        // written to were allocated by fcache_write
        if(extent.mapped) {
            retval = blk_write(device, extent.lba, chunk, data);

            if(retval < 0) {
                return retval;
            }
        }

        sector += chunk;
        count -= chunk;
        data += chunk * device->sector_size;
    }

    return 0;
}

static int write_page(struct vnode* restrict node, fcache_bmap_type bmap, struct pframe* restrict pf, off_t size)
{
    size_t sector_size = node->fs->device->sector_size;
    const uint8_t* data = page_data(pf);
    unsigned dirty = pf->flags >> PFRAME_DIRTY_SHIFT;

    size_t valid = valid_bytes(pf->index, size);
    size_t num_sectors = (valid + sector_size - 1) / sector_size;
    uint64_t first = pf->index * (PAGE_SIZE / sector_size);

    for(size_t i = 0; i < num_sectors;) {
        if(0 == (dirty & (1U << i))) {
            i += 1;
            continue;
        }

        size_t run = 1;

        while(i + run < num_sectors && (dirty & (1U << (i + run)))) {
            run += 1;
        }

        int retval = write_run(node, bmap, first + i, run, data + i * sector_size);

        if(retval < 0) {
            return retval;
        }

        i += run;
    }

    return 0;
}

static int writeback_locked(struct vnode* restrict node, fcache_bmap_type bmap, off_t size)
{
    struct fcache* cache = &node->pages;
    int retval = 0;

    void* batch[GATHER_BATCH];
    uint64_t indices[GATHER_BATCH];
    uint64_t start = 0;
    size_t count;

    while(cache->num_dirty > 0 && (count = radix_gather(&cache->tree, start, batch, indices, GATHER_BATCH)) > 0) {
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

static bool over_dirty_limit(void)
{
    size_t dirtyable_count = pslab_free_count() + atomic_load(&lru_length, ATOMIC_RELAXED);
    size_t dirtyable_percent = dirtyable_count / 100 * CONFIG_FCACHE_DIRTY_PERCENT;
    return atomic_load(&dirty_pages, ATOMIC_RELAXED) > dirtyable_percent;
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
        event_await_timeout(&wakeup, 1, UINT64_C(1000000) * CONFIG_FCACHE_WRITEBACK_MSEC);

        atomic_store(&writeback_kicked, false, ATOMIC_RELEASE);

        vfs_sync_all();
    }
}

static int dev_bmap(struct vnode* restrict node, uint64_t sector, bool create, struct fcache_extent* restrict out)
{
    const struct blk_device* device = node->fs->device;

    if(sector >= device->sector_count) {
        return -EIO;
    }

    out->lba = sector;
    out->count = device->sector_count - sector;
    out->mapped = true;

    return 0;
}

static off_t dev_size(const struct vnode* restrict node)
{
    const struct blk_device* device = node->fs->device;
    return device->sector_count * device->sector_size;
}

static int dev_fsync(struct vnode* restrict node)
{
    return fcache_writeback(node, &dev_bmap, dev_size(node));
}

static const struct vnode_ops dev_ops = {
    .fsync = &dev_fsync,
};

void fcache_init(struct fcache* restrict cache)
{
    mutex_init(&cache->lock);
    radix_init(&cache->tree);
    cache->num_pages = 0;
    cache->num_dirty = 0;
    cache->queued = false;
    cache->dirty_next = nullptr;
}

void fcache_deinit(struct fcache* restrict cache)
{
    kassert(!cache->queued);

    mutex_acquire(&cache->lock);

    void* batch[GATHER_BATCH];
    size_t count;

    while((count = radix_gather(&cache->tree, 0, batch, nullptr, GATHER_BATCH)) > 0) {
        for(size_t i = 0; i < count; ++i) {
            remove_page(cache, batch[i]);
        }
    }

    mutex_release(&cache->lock);

    radix_deinit(&cache->tree);
    mutex_deinit(&cache->lock);
}

ssize_t fcache_read(struct vnode* restrict node, fcache_bmap_type bmap, void* restrict buffer, size_t sz, off_t offset, off_t size)
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

    struct fcache* cache = &node->pages;
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

ssize_t fcache_write(struct vnode* restrict node, fcache_bmap_type bmap, const void* restrict buffer, size_t sz, off_t offset, off_t size)
{
    if(offset < 0) {
        return -EINVAL;
    }

    if(!sector_size_valid(node->fs->device)) {
        return -EINVAL;
    }

    struct fcache* cache = &node->pages;
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

        // Written back, this file's own dirty pages can be reclaimed
        if(pf == nullptr && cache->num_dirty > 0) {
            kick_writeback();
            writeback_locked(node, bmap, size);
            pf = get_page(cache, index);
        }

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
        mark_dirty(node, pf, sector_span(node, in_page, chunk));

        done += chunk;

        if((off_t)(pos + chunk) > size) {
            size = pos + chunk;
        }
    }

    // The writer pays for going over the limit with its own pages;
    // it already holds what the filesystem needs for writeback
    if(over_dirty_limit()) {
        kick_writeback();
        writeback_locked(node, bmap, size);
    }

    mutex_release(&cache->lock);

    if(done == 0 && retval < 0) {
        return retval;
    }

    return done;
}

int fcache_map_page(struct vnode* restrict node, fcache_bmap_type bmap, uint64_t index, off_t size, struct pagemap* restrict pmap,
    uintptr_t virt, unsigned vprot)
{
    if(!sector_size_valid(node->fs->device)) {
        return -EINVAL;
    }

    if(size <= 0 || index > (uint64_t)(size - 1) / PAGE_SIZE) {
        return -ENXIO;
    }

    struct fcache* cache = &node->pages;

    mutex_acquire(&cache->lock);

    struct pframe* pf = get_page(cache, index);

    if(pf == nullptr) {
        mutex_release(&cache->lock);
        return -ENOMEM;
    }

    if(0 == (pf->flags & PFRAME_UPTODATE)) {
        int retval = fill_page(node, bmap, pf, size);

        if(retval < 0) {
            remove_page(cache, pf);
            mutex_release(&cache->lock);
            return retval;
        }
    }

    pf->flags |= PFRAME_ACCESSED;
    pframe_incref(pf);

    // Mapped under the cache lock, so a truncation either finds the mapping or comes before it
    int retval = rmap_map(pmap, virt, pframe_phys(pf), vprot);

    if(retval < 0) {
        bool last = pframe_decref(pf);
        kassert(!last); // The cache still holds its own
    }

    mutex_release(&cache->lock);

    return retval;
}

int fcache_writeback(struct vnode* restrict node, fcache_bmap_type bmap, off_t size)
{
    struct fcache* cache = &node->pages;

    mutex_acquire(&cache->lock);
    int retval = writeback_locked(node, bmap, size);
    mutex_release(&cache->lock);

    return retval;
}

void fcache_truncate(struct vnode* restrict node, off_t size)
{
    struct fcache* cache = &node->pages;
    uint64_t keep = ((uint64_t)(size) + PAGE_SIZE - 1) / PAGE_SIZE;

    mutex_acquire(&cache->lock);

    void* batch[GATHER_BATCH];
    size_t count;

    while((count = radix_gather(&cache->tree, keep, batch, nullptr, GATHER_BATCH)) > 0) {
        for(size_t i = 0; i < count; ++i) {
            remove_page(cache, batch[i]);
        }
    }

    // Pages hold zeroes past the end of file, so
    // a later extension does not bring back cut data
    size_t tail = size % PAGE_SIZE;

    if(tail) {
        struct pframe* pf = radix_lookup(&cache->tree, size / PAGE_SIZE);

        if(pf) {
            memset((uint8_t*)(page_data(pf)) + tail, 0, PAGE_SIZE - tail);
        }
    }

    mutex_release(&cache->lock);
}

void fcache_forget(struct vnode* restrict node, uint64_t offset, uint64_t length)
{
    struct fcache* cache = &node->pages;
    uint64_t end = offset + length;

    mutex_acquire(&cache->lock);

    for(uint64_t pos = offset; pos < end;) {
        size_t in_page = pos % PAGE_SIZE;
        size_t chunk = PAGE_SIZE - in_page;

        if(chunk > end - pos) {
            chunk = end - pos;
        }

        struct pframe* pf = radix_lookup(&cache->tree, pos / PAGE_SIZE);

        if(pf) {
            pf->flags &= ~(sector_span(node, in_page, chunk) << PFRAME_DIRTY_SHIFT);

            if(0 == (pf->flags >> PFRAME_DIRTY_SHIFT)) {
                mark_clean(cache, pf);
            }
        }

        pos += chunk;
    }

    mutex_release(&cache->lock);
}

int fcache_sync(struct vfs* restrict fs)
{
    size_t pending = 0;

    spinlock_acquire(&dirty_lock);

    for(struct vnode* node = dirty_head; node; node = node->pages.dirty_next) {
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

        for(struct vnode** prev = &dirty_head; *prev; prev = &(*prev)->pages.dirty_next) {
            struct vnode* node = *prev;

            if(fs == nullptr || node->fs == fs) {
                *prev = node->pages.dirty_next;
                node->pages.dirty_next = nullptr;
                node->pages.queued = false;
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

struct vnode* fcache_dev_create(struct vfs* restrict fs)
{
    struct vnode* devnode = vnode_alloc(VNODE_BLOCK_DEV, &dev_ops, nullptr);

    if(devnode) {
        devnode->fs = fs;
    }

    return devnode;
}

void fcache_dev_destroy(struct vnode* restrict devnode)
{
    vnode_fsync(devnode);

    bool queued = false;

    spinlock_acquire(&dirty_lock);

    for(struct vnode** prev = &dirty_head; *prev; prev = &(*prev)->pages.dirty_next) {
        if(*prev == devnode) {
            *prev = devnode->pages.dirty_next;
            devnode->pages.dirty_next = nullptr;
            devnode->pages.queued = false;
            queued = true;
            break;
        }
    }

    spinlock_release(&dirty_lock);

    if(queued) {
        vnode_decref(devnode);
    }

    vnode_decref(devnode);
}

int fcache_dev_read(struct vnode* restrict devnode, uint64_t offset, void* restrict buffer, size_t sz)
{
    ssize_t done = fcache_read(devnode, &dev_bmap, buffer, sz, offset, dev_size(devnode));

    if(done < 0) {
        return done;
    }

    return (size_t)(done) == sz ? 0 : -EIO;
}

int fcache_dev_write(struct vnode* restrict devnode, uint64_t offset, const void* restrict buffer, size_t sz)
{
    ssize_t done = fcache_write(devnode, &dev_bmap, buffer, sz, offset, dev_size(devnode));

    if(done < 0) {
        return done;
    }

    return (size_t)(done) == sz ? 0 : -EIO;
}

size_t fcache_reclaim(size_t count)
{
    struct thread* thrd = this_thread();

    if(!interrupt_state() || thrd->preempt_count > 0) {
        return 0;
    }

    size_t freed = 0;

    spinlock_acquire(&lru_lock);

    // Twice round, so that a page left with only its second chance is taken
    size_t budget = 2 * lru_length;

    while(freed < count && budget > 0 && lru_head) {
        struct pframe* pf = lru_head;
        struct fcache* cache = pf->owner;

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

        struct pframe* removed = radix_remove(&cache->tree, pf->index);
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

static void init_fcache(void)
{
    event_init(&writeback_wakeup);

    struct thread* writeback = create_thread_kern(&writeback_thread, nullptr);

    if(writeback == nullptr) {
        panic("fcache: out of memory");
    }

    sched_enqueue(writeback);
}

initcall_bsp_vfs(init_fcache, INIT_FIRST);
