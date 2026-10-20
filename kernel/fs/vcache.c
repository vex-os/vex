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

static_assert(sizeof(vcache_entry_t) <= SLAB_MAX_SIZE);

static vcache_entry_t* cache[NUM_BUCKETS];
static vcache_entry_t* unused_head;
static vcache_entry_t* unused_tail;
static size_t num_unused;
static spinlock_t cache_lock = SPINLOCK_INIT;

static unsigned hash_bucket(vfs_t* restrict fs, ino_t ino)
{
    uint64_t hash;
    hash = (uint64_t)(uintptr_t)fs;
    hash *= UINT64_C(0x9E3779B97F4A7C15);
    hash ^= ino * UINT64_C(0x517CC1B727220A95);
    hash >>= 56;
    return hash % NUM_BUCKETS;
}

static void unused_append_nolock(vcache_entry_t* restrict entry)
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

static void unused_unlink_nolock(vcache_entry_t* restrict entry)
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

static void bucket_detach_nolock(vcache_entry_t* restrict entry)
{
    unsigned bucket = hash_bucket(entry->fs, entry->ino);

    for(vcache_entry_t** prev = &cache[bucket]; *prev; prev = &(*prev)->next) {
        if(*prev == entry) {
            *prev = entry->next;
            break;
        }
    }
}

static void bucket_unlink_nolock(vcache_entry_t* restrict entry)
{
    bucket_detach_nolock(entry);

    entry->node->cache_entry = nullptr;
}

static vnode_t* take_nolock(vcache_entry_t* restrict entry)
{
    if(entry->unused) {
        unused_unlink_nolock(entry);
    }

    vnode_incref(entry->node);

    return entry->node;
}

static vcache_entry_t* find_nolock(vfs_t* restrict fs, ino_t ino)
{
    for(vcache_entry_t* it = cache[hash_bucket(fs, ino)]; it; it = it->next) {
        if(it->fs == fs && it->ino == ino) {
            return it;
        }
    }

    return nullptr;
}

static vnode_t* insert(vnode_t* restrict node, bool pinned)
{
    vcache_entry_t* entry = slab_alloc(sizeof(vcache_entry_t));

    if(entry == nullptr) {
        return nullptr;
    }

    entry->node = node;
    entry->ino = node->ino;
    entry->fs = node->fs;
    entry->pinned = pinned;

    spinlock_acquire(&cache_lock);

    vcache_entry_t* existing = find_nolock(node->fs, node->ino);

    if(existing) {
        vnode_t* cached = take_nolock(existing);
        spinlock_release(&cache_lock);
        slab_free(entry);
        return cached;
    }

    unsigned bucket = hash_bucket(node->fs, node->ino);

    entry->next = cache[bucket];
    cache[bucket] = entry;
    node->cache_entry = entry;

    spinlock_release(&cache_lock);

    return node;
}

vnode_t* vcache_get(vfs_t* restrict fs, ino_t ino)
{
    spinlock_acquire(&cache_lock);

    vcache_entry_t* entry = find_nolock(fs, ino);
    vnode_t* node = entry ? take_nolock(entry) : nullptr;

    spinlock_release(&cache_lock);

    return node;
}

vnode_t* vcache_get_or_insert(vnode_t* restrict node)
{
    return insert(node, false);
}

int vcache_insert_pinned(vnode_t* restrict node)
{
    vnode_t* cached = insert(node, true);

    if(cached == nullptr) {
        return -ENOMEM;
    }

    if(cached != node) {
        vnode_decref(cached);
        return -EEXIST;
    }

    return 0;
}

bool vcache_keep(vnode_t* restrict node)
{
    vcache_entry_t* evicted = nullptr;

    spinlock_acquire(&cache_lock);

    if(atomic_load(&node->refcount, ATOMIC_ACQUIRE)) {
        spinlock_release(&cache_lock);
        return true;
    }

    vcache_entry_t* entry = node->cache_entry;

    if(entry == nullptr) {
        spinlock_release(&cache_lock);
        return false;
    }

    if(!entry->unused) {
        unused_append_nolock(entry);
    }

    while(num_unused > CONFIG_VCACHE_MAX_UNUSED) {
        vcache_entry_t* oldest = unused_head;

        unused_unlink_nolock(oldest);
        bucket_unlink_nolock(oldest);

        oldest->next = evicted;
        evicted = oldest;
    }

    spinlock_release(&cache_lock);

    while(evicted) {
        vcache_entry_t* next = evicted->next;
        vnode_destroy(evicted->node);
        slab_free(evicted);
        evicted = next;
    }

    return true;
}

bool vcache_in_use(vfs_t* restrict fs, unsigned root_refs)
{
    spinlock_acquire(&cache_lock);

    unsigned long actual = atomic_load(&fs->root->refcount, ATOMIC_RELAXED);
    unsigned long expected = root_refs;

    for(unsigned bucket = 0; bucket < NUM_BUCKETS; bucket += 1) {
        for(vcache_entry_t* it = cache[bucket]; it; it = it->next) {
            if(it->fs == fs) {
                actual += atomic_load(&it->node->refcount, ATOMIC_RELAXED);

                if(it->pinned) {
                    expected += 1;
                }

                if(it->node->type == VNODE_DIRECTORY && it->node->parent) {
                    expected += 1;
                }
            }
        }
    }

    spinlock_release(&cache_lock);

    return actual > expected;
}

void vcache_evict(vfs_t* restrict fs)
{
    vcache_entry_t* evicted = nullptr;

    spinlock_acquire(&cache_lock);

    for(unsigned bucket = 0; bucket < NUM_BUCKETS; bucket += 1) {
        vcache_entry_t** prev = &cache[bucket];

        while(*prev) {
            vcache_entry_t* entry = *prev;

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
        vcache_entry_t* next = evicted->next;
        vnode_destroy(evicted->node);
        slab_free(evicted);
        evicted = next;
    }
}

void vcache_remove(vnode_t* restrict node)
{
    spinlock_acquire(&cache_lock);

    vcache_entry_t* entry = node->cache_entry;

    if(entry) {
        kassert(!entry->unused); // Callers hold a reference
        bucket_unlink_nolock(entry);
    }

    spinlock_release(&cache_lock);

    slab_free(entry);
}

bool vcache_cached(vnode_t* restrict node)
{
    spinlock_acquire(&cache_lock);
    bool cached = node->cache_entry != nullptr;
    spinlock_release(&cache_lock);
    return cached;
}

void vcache_rekey(vnode_t* restrict node, ino_t ino)
{
    spinlock_acquire(&cache_lock);

    vcache_entry_t* entry = node->cache_entry;

    if(entry) {
        kassert(find_nolock(node->fs, ino) == nullptr); // The filesystem removed the previous owner of the key

        bucket_detach_nolock(entry);

        unsigned bucket = hash_bucket(node->fs, ino);

        entry->ino = ino;
        entry->next = cache[bucket];
        cache[bucket] = entry;
    }

    node->ino = ino;

    spinlock_release(&cache_lock);
}
