#include <vex/assert.h>
#include <vex/atomic.h>
#include <vex/limits.h>
#include <vex/mount.h>
#include <vex/slab.h>
#include <vex/spinlock.h>
#include <vex/string.h>
#include <vex/vcache.h>
#include <vex/vnode.h>

static_assert(sizeof(struct vnode) <= (PAGE_SIZE / 2));

static struct vnode* alloc_internal(struct vfs* restrict fs, ino_t nodenumber, const struct vnode_ops* restrict ops)
{
    kassert(fs);
    kassert(ops);

    struct vnode* result = slab_alloc(sizeof(struct vnode));

    if(result == nullptr) {
        return nullptr;
    }

    result->fs = fs;
    result->ops = ops;
    result->nodenumber = nodenumber;

    atomic_store(&result->refcount, 1, ATOMIC_RELAXED);

    return result;
}

static int lookup_crossed(struct vnode* restrict node, const char* restrict name, struct vnode** restrict out)
{
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
        *out = cached;
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

    struct vnode* cached_now = vcache_get_or_insert(result);

    if(cached_now != result) {
        if(result->ops->release)
            result->ops->release(result);
        slab_free(result);

        if(cached_now == nullptr) {
            return -ENOMEM;
        }

        *out = cached_now;
        return 0;
    }

    if(result->nodetype == VNODE_DIRECTORY) {
        result->fs_mounted = vfs_get_fs(result);
        result->parent = node;
        vnode_incref(node);
    }

    vnode_incref(result);

    *out = result;

    return 0;
}

static int create_crossed(struct vnode* restrict node, const char* restrict name, unsigned type, struct vnode** restrict out)
{
    if(node->ops->create == nullptr) {
        return -ENOSYS;
    }

    struct vnode* created;
    int retval = node->ops->create(node, name, type, &created);

    if(retval < 0) {
        return retval;
    }

    // Cached like a looked-up node, so a later
    // lookup of the same name finds this vnode
    vnode_incref(created);

    struct vnode* cached_now = vcache_get_or_insert(created);

    if(cached_now != created) {
        if(created->ops->release)
            created->ops->release(created);
        slab_free(created);

        if(cached_now == nullptr) {
            return -ENOMEM;
        }

        *out = cached_now;
        return 0;
    }

    if(created->nodetype == VNODE_DIRECTORY) {
        created->parent = node;
        vnode_incref(node);
    }

    *out = created;

    return 0;
}

static int unlink_crossed(struct vnode* restrict node, const char* restrict name, bool directory)
{
    if(node->ops->unlink == nullptr) {
        return -ENOSYS;
    }

    if(0 == strcmp(name, ".") || 0 == strcmp(name, "..")) {
        return -EINVAL;
    }

    struct vnode* victim;
    int retval = lookup_crossed(node, name, &victim);

    if(retval < 0) {
        return retval;
    }

    if(directory && victim->nodetype != VNODE_DIRECTORY) {
        vnode_decref(victim);
        return -ENOTDIR;
    }

    if(!directory && victim->nodetype == VNODE_DIRECTORY) {
        vnode_decref(victim);
        return -EISDIR;
    }

    if(victim->fs_mounted) {
        vnode_decref(victim);
        return -EBUSY;
    }

    // The filesystem has to vcache_remove the victim under its own lock,
    // before the entry can be reused, or a new file there gets this vnode
    retval = node->ops->unlink(node, name, victim);

    // Whichever reference goes last, possibly
    // the one dropped right below, uncounts it
    if(retval == 0) {
        victim->orphaned = true;
        atomic_fetch_add(&victim->fs->orphans, 1, ATOMIC_RELAXED);
    }

    vnode_decref(victim);

    return retval;
}

struct vnode* vnode_alloc(unsigned nodetype, const struct vnode_ops* restrict ops, void* ctxdata)
{
    kassert(ops);

    struct vnode* result = slab_alloc(sizeof(struct vnode));

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
    kassert(node);

    atomic_fetch_add(&node->refcount, 1, ATOMIC_RELAXED);
}

bool vnode_decref(struct vnode* restrict node)
{
    kassert(node);

    unsigned prev = atomic_fetch_sub(&node->refcount, 1, ATOMIC_ACQ_REL);

    if(prev == 1) {
        struct vnode* parent = node->parent;

        if(node->orphaned) {
            atomic_fetch_sub(&node->fs->orphans, 1, ATOMIC_RELEASE);
        }

        if(node->ops->release)
            node->ops->release(node);
        slab_free(node);

        if(parent) {
            vnode_decref(parent);
        }
    }

    return prev == 1U;
}

int vnode_lookup(struct vnode* restrict node, const char* restrict name, struct vnode** restrict out)
{
    kassert(node);

    struct vnode* target;
    int retval = vfs_cross_mount(node, &target);

    if(retval < 0) {
        return retval;
    }

    retval = lookup_crossed(target, name, out);

    vnode_decref(target);

    return retval;
}

int vnode_iterate(struct vnode* restrict node, struct k_dirent* restrict out, size_t index)
{
    kassert(node);

    struct vnode* target;
    int retval = vfs_cross_mount(node, &target);

    if(retval < 0) {
        return retval;
    }

    if(target->ops->iterate == nullptr) {
        retval = -ENOSYS;
    }
    else {
        retval = target->ops->iterate(target, out, index);
    }

    vnode_decref(target);

    return retval;
}

int vnode_create(struct vnode* restrict node, const char* restrict name, unsigned type, struct vnode** restrict out)
{
    kassert(node);

    struct vnode* target;
    int retval = vfs_cross_mount(node, &target);

    if(retval < 0) {
        return retval;
    }

    retval = create_crossed(target, name, type, out);

    vnode_decref(target);

    return retval;
}

int vnode_unlink(struct vnode* restrict node, const char* restrict name, bool directory)
{
    kassert(node);

    struct vnode* target;
    int retval = vfs_cross_mount(node, &target);

    if(retval < 0) {
        return retval;
    }

    retval = unlink_crossed(target, name, directory);

    vnode_decref(target);

    return retval;
}

int vnode_open(struct vnode* restrict node, unsigned mode)
{
    kassert(node);

    struct vnode* target;
    int retval = vfs_cross_mount(node, &target);

    if(retval < 0) {
        return retval;
    }

    if(target->ops->open == nullptr) {
        retval = -ENOSYS;
    }
    else {
        retval = target->ops->open(target, mode);
    }

    vnode_decref(target);

    return retval;
}

int vnode_close(struct vnode* restrict node)
{
    kassert(node);

    struct vnode* target;
    int retval = vfs_cross_mount(node, &target);

    if(retval < 0) {
        return retval;
    }

    if(target->ops->close == nullptr) {
        retval = -ENOSYS;
    }
    else {
        retval = target->ops->close(target);
    }

    vnode_decref(target);

    return retval;
}

ssize_t vnode_read(struct vnode* restrict node, void* restrict buffer, size_t sz, off_t offset)
{
    kassert(node);

    struct vnode* target;
    ssize_t retval = vfs_cross_mount(node, &target);

    if(retval < 0) {
        return retval;
    }

    if(target->ops->read == nullptr) {
        retval = -ENOSYS;
    }
    else {
        retval = target->ops->read(target, buffer, sz, offset);
    }

    vnode_decref(target);

    return retval;
}

ssize_t vnode_write(struct vnode* restrict node, const void* restrict buffer, size_t sz, off_t offset)
{
    kassert(node);

    struct vnode* target;
    ssize_t retval = vfs_cross_mount(node, &target);

    if(retval < 0) {
        return retval;
    }

    if(target->ops->write == nullptr) {
        retval = -ENOSYS;
    }
    else {
        retval = target->ops->write(target, buffer, sz, offset);
    }

    vnode_decref(target);

    return retval;
}

int vnode_stat(struct vnode* restrict node, struct k_stat* restrict out)
{
    kassert(node);

    struct vnode* target;
    int retval = vfs_cross_mount(node, &target);

    if(retval < 0) {
        return retval;
    }

    if(target->ops->stat == nullptr) {
        retval = -ENOSYS;
    }
    else {
        retval = target->ops->stat(target, out);
    }

    vnode_decref(target);

    return retval;
}

ssize_t vnode_readlink(struct vnode* restrict node, char* restrict buf, size_t sz)
{
    kassert(node);

    if(node->ops->readlink == nullptr)
        return -ENOSYS;
    return node->ops->readlink(node, buf, sz);
}

int vnode_ioctl(struct vnode* restrict node, uint32_t request, uintptr_t argument)
{
    kassert(node);

    struct vnode* target;
    int retval = vfs_cross_mount(node, &target);

    if(retval < 0) {
        return retval;
    }

    if(target->ops->ioctl == nullptr) {
        retval = -ENOSYS;
    }
    else {
        retval = target->ops->ioctl(target, request, argument);
    }

    vnode_decref(target);

    return retval;
}
