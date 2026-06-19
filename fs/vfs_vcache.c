#include <vex/limits.h>
#include <vex/slab.h>
#include <vex/spinlock.h>
#include <vex/vfs_mount.h>
#include <vex/vfs_vcache.h>
#include <vex/vfs_vnode.h>

#define NUM_BUCKETS 256

static_assert(sizeof(struct vcache) <= (PAGE_SIZE / 2));

static struct vcache* cache[NUM_BUCKETS];
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

struct vnode* vcache_get(struct vfs* restrict fs, ino_t nodenumber)
{
    spinlock_acquire(&cache_lock);

    unsigned bucket = hash_bucket(fs, nodenumber);

    for(struct vcache* it = cache[bucket]; it; it = it->next) {
        if(it->fs == fs && it->nodenumber == nodenumber) {
            vnode_incref(it->node);
            spinlock_release(&cache_lock);
            return it->node;
        }
    }

    spinlock_release(&cache_lock);
    return nullptr;
}

int vcache_insert(struct vnode* restrict node)
{
    struct vcache* entry = slab_alloc(sizeof(struct vcache));

    if(entry == nullptr) {
        return -ENOMEM;
    }

    entry->node = node;
    entry->nodenumber = node->nodenumber;
    entry->fs = node->fs;

    spinlock_acquire(&cache_lock);

    unsigned bucket = hash_bucket(node->fs, node->nodenumber);

    entry->next = cache[bucket];
    cache[bucket] = entry;

    spinlock_release(&cache_lock);

    return 0;
}

void vcache_evict(struct vfs* restrict fs)
{
    spinlock_acquire(&cache_lock);

    for(unsigned bucket = 0; bucket < NUM_BUCKETS; bucket += 1) {
        struct vcache** prev = &cache[bucket];

        while(*prev) {
            struct vcache* entry = *prev;

            if(entry->fs == fs) {
                *prev = entry->next;
                vnode_decref(entry->node);
                slab_free(entry);
            }
            else {
                prev = &entry->next;
            }
        }
    }

    spinlock_release(&cache_lock);
}
