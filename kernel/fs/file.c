#include <vex/atomic.h>
#include <vex/fcntl.h>
#include <vex/file.h>
#include <vex/path.h>
#include <vex/slab.h>
#include <vex/stat.h>
#include <vex/string.h>
#include <vex/vnode.h>

// O_RDWR is both bits together, so each one
// stands for read and write access on its own
static unsigned access_for(unsigned flags)
{
    unsigned mask = 0;

    if(flags & O_RDONLY) {
        mask |= R_OK;
    }

    if(flags & (O_WRONLY | O_TRUNC)) {
        mask |= W_OK;
    }

    return mask;
}

int file_open_node(struct vnode* restrict node, unsigned flags, struct file** restrict out)
{
    if(node->nodetype == VNODE_DIRECTORY && (flags & (O_WRONLY | O_TRUNC))) {
        vnode_decref(node);
        return -EISDIR;
    }

    int retval = vnode_open(node, flags);

    if(retval < 0 && retval != -ENOSYS) {
        vnode_decref(node);
        return retval;
    }

    if((flags & O_TRUNC) && node->nodetype == VNODE_FILE) {
        struct vattr attr = { .mask = VATTR_SIZE, .size = 0 };
        retval = vnode_setattr(node, &attr);

        if(retval < 0) {
            vnode_close(node);
            vnode_decref(node);
            return retval;
        }
    }

    struct file* fp = slab_alloc(sizeof(struct file));

    if(fp == nullptr) {
        vnode_close(node);
        vnode_decref(node);
        return -ENOMEM;
    }

    fp->node = node;
    fp->flags = flags;
    fp->offset = 0;

    atomic_store(&fp->refcount, 1, ATOMIC_RELAXED);

    *out = fp;

    return 0;
}

int file_open(struct vnode* restrict stem, const char* restrict path, unsigned flags, mode_t mode, const struct credentials* restrict cred,
    struct file** restrict out)
{
    if(path == nullptr) {
        return -EINVAL;
    }

    struct vnode* node;
    int retval = path_resolve_at(stem, path, 0, cred, &node);
    bool created = false;

    if(retval == -ENOENT && (flags & O_CREAT)) {
        const char* filename;
        struct vnode* parent;

        retval = path_resolve_parent_at(stem, path, cred, &parent, &filename);

        if(retval < 0) {
            return retval;
        }

        retval = vnode_access(parent, cred, W_OK | X_OK);

        if(retval == 0) {
            retval = vnode_create(parent, filename, VNODE_FILE, mode, cred, &node);
            created = retval == 0;
        }

        vnode_decref(parent);

        if(retval == -EEXIST) {
            retval = path_resolve_at(stem, path, 0, cred, &node); // Lost a race with another creator
        }

        if(retval < 0) {
            return retval;
        }
    }
    else if(retval < 0) {
        return retval;
    }

    // The creator gets the access it asked for
    // even when the new mode itself denies it
    if(!created) {
        retval = vnode_access(node, cred, access_for(flags));

        if(retval < 0) {
            vnode_decref(node);
            return retval;
        }
    }

    return file_open_node(node, flags, out);
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

    if(0 == (fp->flags & O_RDONLY)) {
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

    if(0 == (fp->flags & O_WRONLY)) {
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

int file_truncate(struct file* restrict fp, off_t size)
{
    if(fp == nullptr) {
        return -EINVAL;
    }

    if(0 == (fp->flags & O_WRONLY)) {
        return -EBADF;
    }

    struct vattr attr = { .mask = VATTR_SIZE, .size = size };
    return vnode_setattr(fp->node, &attr);
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

int file_ioctl(struct file* restrict fp, uint32_t request, uintptr_t argument)
{
    if(fp == nullptr)
        return -EINVAL;
    return vnode_ioctl(fp->node, request, argument);
}
