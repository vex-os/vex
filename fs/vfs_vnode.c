#include <core/assert.h>
#include <core/atomic.h>
#include <fs/vfs_mount.h>
#include <fs/vfs_vnode.h>
#include <lib/string.h>
#include <mm/slab.h>

/* If a directory node has a filesystem mounted on it, all operations are
 * transparently forwarded to the mounted filesystem's root vnode. */
static __always_inline struct vnode* vnode_resolve_mount(struct vnode* node)
{
    if(node->nodetype == VNODE_DIRECTORY && node->fs_mounted)
        return node->fs_mounted->root;
    return node;
}

struct vnode* vnode_alloc(unsigned nodetype, const struct vnode_ops* restrict ops, void* ctxdata)
{
    struct vnode* result;

    weak_kassert_text(ops, "invalid ops pointer");

    result = slab_calloc(1, sizeof(struct vnode));

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
    node = vnode_resolve_mount(node);
    if(node->ops->lookup == nullptr)
        return -ENOSYS;
    int retval = node->ops->lookup(node, name, out);
    if(retval < 0)
        return retval;
    if((*out)->nodetype == VNODE_DIRECTORY && (*out)->fs_mounted == nullptr)
        (*out)->fs_mounted = vfs_find_mount_for(*out);
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
