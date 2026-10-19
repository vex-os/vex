#include <vex/assert.h>
#include <vex/atomic.h>
#include <vex/errno.h>
#include <vex/limits.h>
#include <vex/mount.h>
#include <vex/slab.h>
#include <vex/spinlock.h>
#include <vex/vcache.h>
#include <vex/vnode.h>

#define NUM_BUCKETS 256

static_assert(sizeof(struct vcache) <= SLAB_MAX_SIZE);

static struct vcache* cache[NUM_BUCKETS];
static struct vcache* unused_head;
static struct vcache* unused_tail;
static size_t num_unused;
static spinlock_type cache_lock = SPINLOCK_INIT;

static unsigned hash_bucket(struct vfs* restrict fs, ino_t nodenumber)
{
    uint64_t hash;
    hash = (uint64_t)(uintptr_t)fs;
    hash *= UINT64_C(0x9E3779B97F4A7C15);
    hash ^= nodenumber * UINT64_C(0x517CC1B727220A95);
    hash >>= 56;
    return hash % NUM_BUCKETS;
}

static void unused_append_nolock(struct vcache* restrict entry)
{
    entry->unused = true;
    entry->unused_prev = unused_tail;
    entry->unused_next = nullptr;

    if(unused_tail) {
        unused_tail->unused_next = entry;
    }
    else {
        unused_head = entry;
    }

    unused_tail = entry;
    num_unused += 1;
}

static void unused_unlink_nolock(struct vcache* restrict entry)
{
    if(entry->unused_prev) {
        entry->unused_prev->unused_next = entry->unused_next;
    }
    else {
        unused_head = entry->unused_next;
    }

    if(entry->unused_next) {
        entry->unused_next->unused_prev = entry->unused_prev;
    }
    else {
        unused_tail = entry->unused_prev;
    }

    entry->unused = false;
    num_unused -= 1;
}

static void bucket_detach_nolock(struct vcache* restrict entry)
{
    unsigned bucket = hash_bucket(entry->fs, entry->nodenumber);

    for(struct vcache** prev = &cache[bucket]; *prev; prev = &(*prev)->next) {
        if(*prev == entry) {
            *prev = entry->next;
            break;
        }
    }
}

static void bucket_unlink_nolock(struct vcache* restrict entry)
{
    bucket_detach_nolock(entry);

    entry->node->cache_entry = nullptr;
}

static struct vnode* take_nolock(struct vcache* restrict entry)
{
    if(entry->unused) {
        unused_unlink_nolock(entry);
    }

    vnode_incref(entry->node);

    return entry->node;
}

static struct vcache* find_nolock(struct vfs* restrict fs, ino_t nodenumber)
{
    for(struct vcache* it = cache[hash_bucket(fs, nodenumber)]; it; it = it->next) {
        if(it->fs == fs && it->nodenumber == nodenumber) {
            return it;
        }
    }

    return nullptr;
}

static struct vnode* insert(struct vnode* restrict node, bool pinned)
{
    struct vcache* entry = slab_alloc(sizeof(struct vcache));

    if(entry == nullptr) {
        return nullptr;
    }

    entry->node = node;
    entry->nodenumber = node->nodenumber;
    entry->fs = node->fs;
    entry->pinned = pinned;

    spinlock_acquire(&cache_lock);

    struct vcache* existing = find_nolock(node->fs, node->nodenumber);

    if(existing) {
        struct vnode* cached = take_nolock(existing);
        spinlock_release(&cache_lock);
        slab_free(entry);
        return cached;
    }

    unsigned bucket = hash_bucket(node->fs, node->nodenumber);

    entry->next = cache[bucket];
    cache[bucket] = entry;
    node->cache_entry = entry;

    spinlock_release(&cache_lock);

    return node;
}

struct vnode* vcache_get(struct vfs* restrict fs, ino_t nodenumber)
{
    spinlock_acquire(&cache_lock);

    struct vcache* entry = find_nolock(fs, nodenumber);
    struct vnode* node = entry ? take_nolock(entry) : nullptr;

    spinlock_release(&cache_lock);

    return node;
}

struct vnode* vcache_get_or_insert(struct vnode* restrict node)
{
    return insert(node, false);
}

int vcache_insert_pinned(struct vnode* restrict node)
{
    struct vnode* cached = insert(node, true);

    if(cached == nullptr) {
        return -ENOMEM;
    }

    if(cached != node) {
        vnode_decref(cached);
        return -EEXIST;
    }

    return 0;
}

bool vcache_keep(struct vnode* restrict node)
{
    struct vcache* evicted = nullptr;

    spinlock_acquire(&cache_lock);

    if(atomic_load(&node->refcount, ATOMIC_ACQUIRE)) {
        spinlock_release(&cache_lock);
        return true;
    }

    struct vcache* entry = node->cache_entry;

    if(entry == nullptr) {
        spinlock_release(&cache_lock);
        return false;
    }

    if(!entry->unused) {
        unused_append_nolock(entry);
    }

    while(num_unused > CONFIG_VCACHE_MAX_UNUSED) {
        struct vcache* oldest = unused_head;

        unused_unlink_nolock(oldest);
        bucket_unlink_nolock(oldest);

        oldest->next = evicted;
        evicted = oldest;
    }

    spinlock_release(&cache_lock);

    while(evicted) {
        struct vcache* next = evicted->next;
        vnode_destroy(evicted->node);
        slab_free(evicted);
        evicted = next;
    }

    return true;
}

bool vcache_in_use(struct vfs* restrict fs, unsigned root_refs)
{
    spinlock_acquire(&cache_lock);

    unsigned long actual = atomic_load(&fs->root->refcount, ATOMIC_RELAXED);
    unsigned long expected = root_refs;

    for(unsigned bucket = 0; bucket < NUM_BUCKETS; bucket += 1) {
        for(struct vcache* it = cache[bucket]; it; it = it->next) {
            if(it->fs == fs) {
                actual += atomic_load(&it->node->refcount, ATOMIC_RELAXED);

                if(it->pinned) {
                    expected += 1;
                }

                if(it->node->nodetype == VNODE_DIRECTORY && it->node->parent) {
                    expected += 1;
                }
            }
        }
    }

    spinlock_release(&cache_lock);

    return actual > expected;
}

void vcache_evict(struct vfs* restrict fs)
{
    struct vcache* evicted = nullptr;

    spinlock_acquire(&cache_lock);

    for(unsigned bucket = 0; bucket < NUM_BUCKETS; bucket += 1) {
        struct vcache** prev = &cache[bucket];

        while(*prev) {
            struct vcache* entry = *prev;

            if(entry->fs != fs) {
                prev = &entry->next;
                continue;
            }

            *prev = entry->next;
            entry->node->cache_entry = nullptr;

            if(entry->unused) {
                unused_unlink_nolock(entry);
                entry->next = evicted;
                evicted = entry;
            }
            else {
                slab_free(entry); // Whoever holds the node frees it with the last reference
            }
        }
    }

    spinlock_release(&cache_lock);

    while(evicted) {
        struct vcache* next = evicted->next;
        vnode_destroy(evicted->node);
        slab_free(evicted);
        evicted = next;
    }
}

void vcache_remove(struct vnode* restrict node)
{
    spinlock_acquire(&cache_lock);

    struct vcache* entry = node->cache_entry;

    if(entry) {
        kassert(!entry->unused); // Callers hold a reference
        bucket_unlink_nolock(entry);
    }

    spinlock_release(&cache_lock);

    slab_free(entry);
}

bool vcache_cached(struct vnode* restrict node)
{
    spinlock_acquire(&cache_lock);
    bool cached = node->cache_entry != nullptr;
    spinlock_release(&cache_lock);
    return cached;
}

void vcache_rekey(struct vnode* restrict node, ino_t nodenumber)
{
    spinlock_acquire(&cache_lock);

    struct vcache* entry = node->cache_entry;

    if(entry) {
        kassert(find_nolock(node->fs, nodenumber) == nullptr); // The filesystem removed the previous owner of the key

        bucket_detach_nolock(entry);

        unsigned bucket = hash_bucket(node->fs, nodenumber);

        entry->nodenumber = nodenumber;
        entry->next = cache[bucket];
        cache[bucket] = entry;
    }

    node->nodenumber = nodenumber;

    spinlock_release(&cache_lock);
}
