#include <core/atomic.h>
#include <core/errno.h>
#include <fs/vfs_file.h>
#include <fs/vfs_node.h>
#include <fs/vfs_path.h>
#include <fs/vfs_types.h>
#include <lib/string.h>
#include <mm/slab.h>

struct vfs_file* vfs_file_open(const char* restrict path, unsigned flags)
{
    struct vfs_node* node;
    struct vfs_file* file;
    int retval;

    if(path == nullptr) {
        return nullptr;
    }

    retval = vfs_lookup(path, &node);

    if(retval) {
        return nullptr;
    }

    if(node->ops->open) {
        retval = node->ops->open(node, flags);

        if(retval) {
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

struct vfs_file* vfs_file_dupe(struct vfs_file* restrict file)
{
    if(file == nullptr) {
        return nullptr;
    }

    atomic_fetch_add(&file->refcount, 1, ATOMIC_RELAXED);

    return file;
}

void vfs_file_close(struct vfs_file* restrict file)
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

ssize_t vfs_file_read(struct vfs_file* restrict file, void* restrict buffer, size_t sz)
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

ssize_t vfs_file_write(struct vfs_file* restrict file, const void* restrict buffer, size_t sz)
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

int vfs_file_ioctl(struct vfs_file* restrict file, unsigned request, void* restrict arg)
{
    if(file == nullptr) {
        return EINVAL;
    }

    if(file->node->ops->ioctl == nullptr) {
        return ENOTTY;
    }

    return file->node->ops->ioctl(file->node, request, arg);
}

int vfs_file_seek(struct vfs_file* restrict file, off_t offset, int whence)
{
    struct vfs_stat st;
    off_t new_offset;

    if(file == nullptr) {
        return EINVAL;
    }

    switch(whence) {
        case VFS_SEEK_SET:
            new_offset = offset;
            break;

        case VFS_SEEK_CUR:
            new_offset = file->offset + offset;
            break;

        case VFS_SEEK_END:
            if(file->node->ops->stat == nullptr) {
                return ENOSYS;
            }

            if(file->node->ops->stat(file->node, &st)) {
                return EIO;
            }

            new_offset = st.size + offset;
            break;

        default:
            return EINVAL;
    }

    if(new_offset < 0) {
        return EINVAL;
    }

    file->offset = new_offset;

    return 0;
}
