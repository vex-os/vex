#include <core/atomic.h>
#include <fs/file.h>
#include <fs/path.h>
#include <fs/stat.h>
#include <fs/vnode.h>
#include <lib/string.h>
#include <mm/slab.h>

struct file* file_open(const char* restrict path, unsigned flags)
{
    int retval;
    struct file* fp;
    struct vnode* node;

    if(path == nullptr) {
        return nullptr;
    }

    retval = path_resolve(path, &node);

    if(retval < 0) {
        return nullptr;
    }

    retval = vnode_open(node, flags);

    if(retval < 0 && retval != -ENOSYS) {
        vnode_decref(node);
        return nullptr;
    }

    fp = slab_calloc(1, sizeof(struct file));

    if(fp == nullptr) {
        vnode_close(node);
        vnode_decref(node);
        return nullptr;
    }

    fp->node = node;
    fp->flags = flags;
    fp->offset = 0;

    atomic_store(&fp->refcount, 1, ATOMIC_RELAXED);

    return fp;
}

struct file* file_dupe(struct file* restrict fp)
{
    if(fp == nullptr) {
        return nullptr;
    }

    atomic_fetch_add(&fp->refcount, 1, ATOMIC_RELAXED);

    return fp;
}

void file_close(struct file* restrict fp)
{
    if(fp == nullptr) {
        return;
    }

    if(1 == atomic_fetch_sub(&fp->refcount, 1, ATOMIC_ACQ_REL)) {
        vnode_close(fp->node);
        vnode_decref(fp->node);
        slab_free(fp);
    }
}

ssize_t file_read(struct file* restrict fp, void* restrict buffer, size_t sz)
{
    ssize_t result;

    if(fp == nullptr || buffer == nullptr) {
        return -EINVAL;
    }

    if(0 == (fp->flags & (O_RDONLY | O_RDWR))) {
        return -EBADF;
    }

    result = vnode_read(fp->node, buffer, sz, fp->offset);

    if(result > 0) {
        fp->offset += result;
    }

    return result;
}

ssize_t file_write(struct file* restrict fp, const void* restrict buffer, size_t sz)
{
    int retval;
    ssize_t result;
    struct __k_stat stat;

    if(fp == nullptr || buffer == nullptr) {
        return -EINVAL;
    }

    if(0 == (fp->flags & (O_WRONLY | O_RDWR))) {
        return -EBADF;
    }

    if(fp->flags & O_APPEND) {
        retval = vnode_stat(fp->node, &stat);

        if(retval < 0 || stat.st_size < 0) {
            return retval;
        }

        fp->offset = stat.st_size;
    }

    result = vnode_write(fp->node, buffer, sz, fp->offset);

    if(result > 0) {
        fp->offset += result;
    }

    return result;
}

int file_seek(struct file* restrict fp, ptrdiff_t offset, int origin)
{
    int retval;
    ptrdiff_t new_offset;
    struct __k_stat stat;

    if(fp == nullptr) {
        return -EINVAL;
    }

    if(origin == SEEK_SET) {
        new_offset = offset;
        goto apply_offset;
    }

    if(origin == SEEK_CUR) {
        new_offset = offset + (ptrdiff_t)(fp->offset);
        goto apply_offset;
    }

    if(origin == SEEK_END) {
        retval = vnode_stat(fp->node, &stat);

        if(retval < 0) {
            return retval;
        }

        new_offset = stat.st_size + offset;
        goto apply_offset;
    }

    return -EINVAL;

apply_offset:
    if(new_offset < 0) {
        return -EINVAL;
    }

    fp->offset = (uintptr_t)(new_offset);

    return 0;
}

int file_ioctl(struct file* restrict fp, uint32_t request, void* restrict argument)
{
    if(fp == nullptr)
        return -EINVAL;
    return vnode_ioctl(fp->node, request, argument);
}
