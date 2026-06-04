#include <core/assert.h>
#include <core/atomic.h>
#include <core/spinlock.h>
#include <fs/vfs_mount.h>
#include <fs/vfs_vnode.h>
#include <lib/string.h>
#include <mm/slab.h>

#define VCACHE_BUCKETS 256

struct vcache_entry {
    struct vcache_entry* next;
    struct vfs* fs;
    ino_t ino;
    struct vnode* node;
};

static struct vcache_entry* vcache[VCACHE_BUCKETS];
static spinlock_type vcache_lock = SPINLOCK_INIT;

static unsigned vcache_bucket(struct vfs* fs, ino_t ino)
{
    uintptr_t h = (uintptr_t)fs ^ (uintptr_t)(ino >> 3);
    return (unsigned)(h % VCACHE_BUCKETS);
}

static struct vnode* vcache_get(struct vfs* fs, ino_t ino)
{
    spinlock_acquire(&vcache_lock);

    unsigned b = vcache_bucket(fs, ino);

    for(struct vcache_entry* e = vcache[b]; e; e = e->next) {
        if(e->fs == fs && e->ino == ino) {
            vnode_incref(e->node);
            spinlock_release(&vcache_lock);
            return e->node;
        }
    }

    spinlock_release(&vcache_lock);
    return nullptr;
}

int vnode_cache_insert(struct vnode* node)
{
    struct vcache_entry* entry = slab_alloc(sizeof(struct vcache_entry));

    if(entry == nullptr)
        return -ENOMEM;

    entry->fs = node->fs;
    entry->ino = node->ino;
    entry->node = node;

    spinlock_acquire(&vcache_lock);

    unsigned b = vcache_bucket(node->fs, node->ino);
    entry->next = vcache[b];
    vcache[b] = entry;

    spinlock_release(&vcache_lock);

    return 0;
}

void vnode_cache_evict_fs(struct vfs* restrict fs)
{
    spinlock_acquire(&vcache_lock);

    for(unsigned b = 0; b < VCACHE_BUCKETS; b++) {
        struct vcache_entry** prev = &vcache[b];

        while(*prev) {
            struct vcache_entry* e = *prev;

            if(e->fs == fs) {
                *prev = e->next;
                vnode_decref(e->node);
                slab_free(e);
            }
            else {
                prev = &e->next;
            }
        }
    }

    spinlock_release(&vcache_lock);
}

static __always_inline struct vnode* vnode_resolve_mount(struct vnode* node)
{
    if(node->nodetype == VNODE_DIRECTORY && node->fs_mounted)
        return node->fs_mounted->root;
    return node;
}

struct vnode* vnode_alloc(unsigned nodetype, const struct vnode_ops* restrict ops, void* ctxdata)
{
    weak_kassert_text(ops, "invalid ops pointer");

    struct vnode* result = slab_calloc(1, sizeof(struct vnode));

    if(result == nullptr)
        return nullptr;

    result->ops = ops;
    result->nodetype = nodetype;
    result->ctxdata = ctxdata;

    atomic_store(&result->refcount, 1, ATOMIC_RELAXED);

    return result;
}

struct vnode* vnode_alloc_ino(struct vfs* restrict fs, ino_t ino, const struct vnode_ops* restrict ops)
{
    weak_kassert_text(fs, "invalid fs pointer");
    weak_kassert_text(ops, "invalid ops pointer");

    struct vnode* result = slab_calloc(1, sizeof(struct vnode));

    if(result == nullptr)
        return nullptr;

    result->fs = fs;
    result->ino = ino;
    result->ops = ops;

    atomic_store(&result->refcount, 1, ATOMIC_RELAXED);

    return result;
}

void vnode_incref(struct vnode* restrict node)
{
    weak_kassert_text(node, "invalid node pointer");
    atomic_fetch_add(&node->refcount, 1, ATOMIC_RELAXED);
}

bool vnode_decref(struct vnode* restrict node)
{
    weak_kassert_text(node, "invalid node pointer");

    unsigned prev = atomic_fetch_sub(&node->refcount, 1, ATOMIC_ACQ_REL);

    if(prev == 1) {
        if(node->ops->release)
            node->ops->release(node);
        slab_free(node);
    }

    return prev == 1U;
}

int vnode_lookup(struct vnode* restrict node, const char* restrict name, struct vnode** restrict out)
{
    node = vnode_resolve_mount(node);

    if(node->ops->lookup == nullptr)
        return -ENOSYS;

    ino_t ino;
    int retval = node->ops->lookup(node, name, &ino);

    if(retval < 0)
        return retval;

    struct vnode* cached = vcache_get(node->fs, ino);

    if(cached) {
        *out = cached;
        return 0;
    }

    struct vnode* result = vnode_alloc_ino(node->fs, ino, node->ops);

    if(result == nullptr)
        return -ENOMEM;

    if(node->ops->iget != nullptr) {
        retval = node->ops->iget(result);

        if(retval < 0) {
            slab_free(result);
            return retval;
        }
    }

    retval = vnode_cache_insert(result);

    if(retval < 0) {
        if(result->ops->release)
            result->ops->release(result);
        slab_free(result);
        return retval;
    }

    if(result->nodetype == VNODE_DIRECTORY)
        result->fs_mounted = vfs_find_mount_for(result);

    vnode_incref(result);
    *out = result;

    return 0;
}

int vnode_iterate(struct vnode* restrict node, struct dirent* restrict out, size_t index)
{
    node = vnode_resolve_mount(node);
    if(node->ops->iterate == nullptr)
        return -ENOSYS;
    return node->ops->iterate(node, out, index);
}

int vnode_create(struct vnode* restrict node, const char* restrict name, unsigned type, struct vnode** restrict out)
{
    node = vnode_resolve_mount(node);
    if(node->ops->create == nullptr)
        return -ENOSYS;
    return node->ops->create(node, name, type, out);
}

int vnode_unlink(struct vnode* restrict node, const char* restrict name)
{
    node = vnode_resolve_mount(node);
    if(node->ops->unlink == nullptr)
        return -ENOSYS;
    return node->ops->unlink(node, name);
}

int vnode_open(struct vnode* restrict node, unsigned mode)
{
    node = vnode_resolve_mount(node);
    if(node->ops->open == nullptr)
        return -ENOSYS;
    return node->ops->open(node, mode);
}

int vnode_close(struct vnode* restrict node)
{
    node = vnode_resolve_mount(node);
    if(node->ops->close == nullptr)
        return -ENOSYS;
    return node->ops->close(node);
}

ssize_t vnode_read(struct vnode* restrict node, void* restrict buffer, size_t sz, uintptr_t offset)
{
    node = vnode_resolve_mount(node);
    if(node->ops->read == nullptr)
        return -ENOSYS;
    return node->ops->read(node, buffer, sz, offset);
}

ssize_t vnode_write(struct vnode* restrict node, const void* restrict buffer, size_t sz, uintptr_t offset)
{
    node = vnode_resolve_mount(node);
    if(node->ops->write == nullptr)
        return -ENOSYS;
    return node->ops->write(node, buffer, sz, offset);
}

int vnode_stat(struct vnode* restrict node, struct __k_stat* restrict out)
{
    node = vnode_resolve_mount(node);
    if(node->ops->stat == nullptr)
        return -ENOSYS;
    return node->ops->stat(node, out);
}

int vnode_ioctl(struct vnode* restrict node, uint32_t request, void* restrict argument)
{
    node = vnode_resolve_mount(node);
    if(node->ops->ioctl == nullptr)
        return -ENOSYS;
    return node->ops->ioctl(node, request, argument);
}
