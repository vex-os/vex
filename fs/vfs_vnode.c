#include <core/assert.h>
#include <core/atomic.h>
#include <core/spinlock.h>
#include <fs/vfs_mount.h>
#include <fs/vfs_vcache.h>
#include <fs/vfs_vnode.h>
#include <lib/string.h>
#include <mm/slab.h>

static __always_inline struct vnode* resolve_mount(struct vnode* node)
{
    if(node->nodetype == VNODE_DIRECTORY && node->fs_mounted)
        return node->fs_mounted->root;
    return node;
}

static struct vnode* alloc_internal(struct vfs* restrict fs, ino_t nodenumber, const struct vnode_ops* restrict ops)
{
    weak_kassert_text(fs, "invalid fs pointer");
    weak_kassert_text(ops, "invalid ops pointer");

    struct vnode* result = slab_calloc(1, sizeof(struct vnode));

    if(result == nullptr) {
        return nullptr;
    }

    result->fs = fs;
    result->ops = ops;
    result->nodenumber = nodenumber;

    atomic_store(&result->refcount, 1, ATOMIC_RELAXED);

    return result;
}

struct vnode* vnode_alloc(unsigned nodetype, const struct vnode_ops* restrict ops, void* ctxdata)
{
    weak_kassert_text(ops, "invalid ops pointer");

    struct vnode* result = slab_calloc(1, sizeof(struct vnode));

    if(result == nullptr) {
        return nullptr;
    }

    result->ops = ops;
    result->nodetype = nodetype;
    result->ctxdata = ctxdata;

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
    weak_kassert_text(node, "invalid node pointer");

    node = resolve_mount(node);

    if(node->ops->lookup == nullptr) {
        return -ENOSYS;
    }

    ino_t nodenumber;
    int retval = node->ops->lookup(node, name, &nodenumber);

    if(retval < 0) {
        return retval;
    }

    struct vnode* cached = vcache_get(node->fs, nodenumber);

    if(cached) {
        out[0] = cached;
        return 0;
    }

    struct vnode* result = alloc_internal(node->fs, nodenumber, node->ops);

    if(result == nullptr) {
        return -ENOMEM;
    }

    if(node->fs->ops->fillnode) {
        retval = node->fs->ops->fillnode(node->fs, result);

        if(retval < 0) {
            slab_free(result);
            return retval;
        }
    }

    retval = vcache_insert(result);

    if(retval < 0) {
        if(result->ops->release)
            result->ops->release(result);
        slab_free(result);
        return retval;
    }

    if(result->nodetype == VNODE_DIRECTORY) {
        result->fs_mounted = vfs_find_mount_for(result);
        result->parent = node;
        vnode_incref(node);
    }

    vnode_incref(result);

    out[0] = result;

    return 0;
}

int vnode_iterate(struct vnode* restrict node, struct dirent* restrict out, size_t index)
{
    weak_kassert_text(node, "invalid node pointer");

    node = resolve_mount(node);

    if(node->ops->iterate == nullptr)
        return -ENOSYS;
    return node->ops->iterate(node, out, index);
}

int vnode_create(struct vnode* restrict node, const char* restrict name, unsigned type, struct vnode** restrict out)
{
    weak_kassert_text(node, "invalid node pointer");

    node = resolve_mount(node);

    if(node->ops->create == nullptr)
        return -ENOSYS;
    return node->ops->create(node, name, type, out);
}

int vnode_unlink(struct vnode* restrict node, const char* restrict name)
{
    weak_kassert_text(node, "invalid node pointer");

    node = resolve_mount(node);

    if(node->ops->unlink == nullptr)
        return -ENOSYS;
    return node->ops->unlink(node, name);
}

int vnode_open(struct vnode* restrict node, unsigned mode)
{
    weak_kassert_text(node, "invalid node pointer");

    node = resolve_mount(node);

    if(node->ops->open == nullptr)
        return -ENOSYS;
    return node->ops->open(node, mode);
}

int vnode_close(struct vnode* restrict node)
{
    weak_kassert_text(node, "invalid node pointer");

    node = resolve_mount(node);

    if(node->ops->close == nullptr)
        return -ENOSYS;
    return node->ops->close(node);
}

ssize_t vnode_read(struct vnode* restrict node, void* restrict buffer, size_t sz, uintptr_t offset)
{
    weak_kassert_text(node, "invalid node pointer");

    node = resolve_mount(node);

    if(node->ops->read == nullptr)
        return -ENOSYS;
    return node->ops->read(node, buffer, sz, offset);
}

ssize_t vnode_write(struct vnode* restrict node, const void* restrict buffer, size_t sz, uintptr_t offset)
{
    weak_kassert_text(node, "invalid node pointer");

    node = resolve_mount(node);

    if(node->ops->write == nullptr)
        return -ENOSYS;
    return node->ops->write(node, buffer, sz, offset);
}

int vnode_stat(struct vnode* restrict node, struct __k_stat* restrict out)
{
    weak_kassert_text(node, "invalid node pointer");

    node = resolve_mount(node);

    if(node->ops->stat == nullptr)
        return -ENOSYS;
    return node->ops->stat(node, out);
}

int vnode_ioctl(struct vnode* restrict node, uint32_t request, void* restrict argument)
{
    weak_kassert_text(node, "invalid node pointer");

    node = resolve_mount(node);

    if(node->ops->ioctl == nullptr)
        return -ENOSYS;
    return node->ops->ioctl(node, request, argument);
}
