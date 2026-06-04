#include <core/atomic.h>
#include <core/errno.h>
#include <fs/vfs_file.h>
#include <fs/vfs_node.h>
#include <fs/vfs_path.h>
#include <fs/vfs_types.h>
#include <lib/string.h>
#include <mm/slab.h>

struct vfs_file* vfs_open(const char* restrict path, unsigned flags)
{
    int retval;
    struct vfs_file* file;
    struct vfs_node* node;

    if(path == nullptr) {
        return nullptr;
    }

    retval = vfs_lookup(path, &node);

    if(retval < 0) {
        return nullptr;
    }

    if(node->ops->open) {
        retval = node->ops->open(node, flags);

        if(retval < 0) {
            vfs_node_dec_refcount(node);
            return nullptr;
        }
    }

    file = slab_alloc(sizeof(struct vfs_file));

    if(file == nullptr) {
        if(node->ops->close)
            node->ops->close(node);
        vfs_node_dec_refcount(node);
        return nullptr;
    }

    memset(file, 0, sizeof(struct vfs_file));

    file->node = node;
    file->flags = flags;
    file->offset = 0;

    atomic_store(&file->refcount, 1, ATOMIC_RELAXED);

    return file;
}

struct vfs_file* vfs_dupe(struct vfs_file* restrict file)
{
    if(file == nullptr) {
        return nullptr;
    }

    atomic_fetch_add(&file->refcount, 1, ATOMIC_RELAXED);

    return file;
}

void vfs_close(struct vfs_file* restrict file)
{
    if(file == nullptr) {
        return;
    }

    if(1 == atomic_fetch_sub(&file->refcount, 1, ATOMIC_ACQ_REL)) {
        if(file->node->ops->close) {
            file->node->ops->close(file->node);
        }

        vfs_node_dec_refcount(file->node);

        slab_free(file);
    }
}

ssize_t vfs_read(struct vfs_file* restrict file, void* restrict buffer, size_t sz)
{
    ssize_t result;

    if(file == nullptr || buffer == nullptr) {
        return -EINVAL;
    }

    if(0 == (file->flags & (VFS_O_RDONLY | VFS_O_RDWR))) {
        return -EBADF;
    }

    if(file->node->ops->read == nullptr) {
        return -ENOSYS;
    }

    result = file->node->ops->read(file->node, buffer, sz, file->offset);

    if(result > 0) {
        file->offset += result;
    }

    return result;
}

ssize_t vfs_write(struct vfs_file* restrict file, const void* restrict buffer, size_t sz)
{
    ssize_t result;

    if(file == nullptr || buffer == nullptr) {
        return -EINVAL;
    }

    if(0 == (file->flags & (VFS_O_WRONLY | VFS_O_RDWR))) {
        return -EBADF;
    }

    if(file->node->ops->write == nullptr) {
        return -ENOSYS;
    }

    if(file->flags & VFS_O_APPEND) {
        struct vfs_stat st;

        if(file->node->ops->stat && 0 == file->node->ops->stat(file->node, &st)) {
            file->offset = st.size;
        }
    }

    result = file->node->ops->write(file->node, buffer, sz, file->offset);

    if(result > 0) {
        file->offset += result;
    }

    return result;
}

int vfs_ioctl(struct vfs_file* restrict file, unsigned request, void* restrict arg)
{
    if(file == nullptr) {
        return -EINVAL;
    }

    if(file->node->ops->ioctl == nullptr) {
        return -ENOTTY;
    }

    return file->node->ops->ioctl(file->node, request, arg);
}

int vfs_seek(struct vfs_file* restrict file, ssize_t offset, int whence)
{
    int retval;
    ssize_t new_offset;
    struct vfs_stat stat;

    if(file == nullptr) {
        return -EINVAL;
    }

    if(whence == VFS_SEEK_SET) {
        new_offset = offset;
        goto apply_offset;
        unreachable();
    }

    if(whence == VFS_SEEK_CUR) {
        new_offset = offset + (ssize_t)(file->offset);
        goto apply_offset;
        unreachable();
    }

    if(whence == VFS_SEEK_END) {
        if(file->node->ops->stat == nullptr) {
            return -ENOSYS;
        }

        retval = file->node->ops->stat(file->node, &stat);

        if(retval < 0) {
            return retval;
        }

        new_offset = stat.size + offset;
        goto apply_offset;
        unreachable();
    }

    return -EINVAL;

apply_offset:
    if(new_offset < 0)
        return -EINVAL;
    file->offset = (size_t)(new_offset);
    return 0;
}
