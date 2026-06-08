#include <string.h>
#include <sys/atomic.h>
#include <sys/slab.h>
#include <sys/stat.h>
#include <sys/vfs_file.h>
#include <sys/vfs_path.h>
#include <sys/vfs_vnode.h>

struct file* file_open(const char* restrict path, unsigned flags)
{
    if(path == nullptr) {
        return nullptr;
    }

    struct vnode* node;
    int retval = path_resolve(path, &node);

    if(retval == -ENOENT && (flags & O_CREAT)) {
        const char* slash = nullptr;

        for(const char* it = path; *it; ++it) {
            if(*it == PATH_SEPARATOR) {
                slash = it;
            }
        }

        const char* filename;
        struct vnode* parent;

        if(slash == nullptr) {
            filename = path;
        }
        else {
            filename = &slash[1];
        }

        if(slash == nullptr || slash == path) {
            retval = path_resolve("/", &parent);
        }
        else {
            char dirpath[NAME_SIZE];
            size_t dir_length = slash - path;

            if(dir_length >= NAME_SIZE) {
                return nullptr;
            }

            memcpy(dirpath, path, dir_length);

            dirpath[dir_length] = 0x00;

            retval = path_resolve(dirpath, &parent);
        }

        if(retval < 0) {
            return nullptr;
        }

        retval = vnode_create(parent, filename, VNODE_FILE, &node);

        vnode_decref(parent);

        if(retval < 0) {
            return nullptr;
        }
    }
    else if(retval < 0) {
        return nullptr;
    }

    retval = vnode_open(node, flags);

    if(retval < 0 && retval != -ENOSYS) {
        vnode_decref(node);
        return nullptr;
    }

    struct file* fp = slab_alloc(sizeof(struct file));

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
    if(fp == nullptr || buffer == nullptr) {
        return -EINVAL;
    }

    if(0 == (fp->flags & (O_WRONLY | O_RDWR))) {
        return -EBADF;
    }

    if(fp->flags & O_APPEND) {
        struct k_stat stat;
        int retval = vnode_stat(fp->node, &stat);

        if(retval < 0) {
            return retval;
        }

        if(stat.st_size < 0) {
            return -EINVAL;
        }

        fp->offset = stat.st_size;
    }

    ssize_t result = vnode_write(fp->node, buffer, sz, fp->offset);

    if(result > 0) {
        fp->offset += result;
    }

    return result;
}

int file_seek(struct file* restrict fp, off_t offset, int origin)
{
    if(fp == nullptr) {
        return -EINVAL;
    }

    off_t new_offset;

    if(origin == SEEK_SET) {
        new_offset = offset;
        goto apply_offset;
    }

    if(origin == SEEK_CUR) {
        new_offset = offset + fp->offset;
        goto apply_offset;
    }

    if(origin == SEEK_END) {
        struct k_stat stat;
        int retval = vnode_stat(fp->node, &stat);

        if(retval < 0) {
            return retval;
        }

        new_offset = stat.st_size + offset;
        goto apply_offset;
    }

    return -EINVAL;

apply_offset:
    if(new_offset < 0)
        return -EINVAL;
    fp->offset = new_offset;
    return 0;
}

int file_ioctl(struct file* restrict fp, uint32_t request, void* restrict argument)
{
    if(fp == nullptr)
        return -EINVAL;
    return vnode_ioctl(fp->node, request, argument);
}
