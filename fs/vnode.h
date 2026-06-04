#ifndef EAAEA97D_DF46_4C1D_9BCC_3810B6FA33F7
#define EAAEA97D_DF46_4C1D_9BCC_3810B6FA33F7
#pragma once

#include <core/cdefs.h>
#include <core/errno.h>
#include <core/spinlock.h>
#include <core/types.h>

enum {
    VNODE_FILE,
    VNODE_DIRECTORY,
    VNODE_BLOCK_DEV,
    VNODE_CHAR_DEV,
    VNODE_SYM_LINK,
};

#define O_RDONLY 0x0001
#define O_WRONLY 0x0002
#define O_RDWR   0x0003
#define O_CREAT  0x0004
#define O_TRUNC  0x0008
#define O_APPEND 0x0010

struct dirent;
struct __k_stat;

struct vnode;
struct vnode_ops {
    void (*release)(struct vnode* restrict node);
    int (*lookup)(struct vnode* restrict node, const char* restrict name, struct vnode** restrict out);
    int (*iterate)(struct vnode* restrict node, struct dirent* restrict out, size_t index);
    int (*create)(struct vnode* restrict node, const char* restrict name, unsigned type, struct vnode** restrict out);
    int (*unlink)(struct vnode* restrict node, const char* restrict name);
    int (*open)(struct vnode* restrict node, unsigned mode);
    int (*close)(struct vnode* restrict node);
    ssize_t (*read)(struct vnode* restrict node, void* restrict buffer, size_t sz, uintptr_t offset);
    ssize_t (*write)(struct vnode* restrict node, const void* restrict buffer, size_t sz, uintptr_t offset);
    int (*ioctl)(struct vnode* restrict node, uint32_t request, void* restrict argument);
    int (*stat)(struct vnode* restrict node, struct __k_stat* restrict out);
};

struct vfs;
struct vnode {
    struct vfs* fs;
    struct vfs* fs_mounted;
    const struct vnode_ops* ops;
    spinlock_type lock;
    unsigned refcount;
    unsigned nodetype;
    void* ctxdata;
};

struct vnode* vnode_alloc(unsigned nodetype, const struct vnode_ops* restrict ops, void* restrict ctxdata) __nodiscard;
void vnode_incref(struct vnode* restrict node);
bool vnode_decref(struct vnode* restrict node);

static __always_inline inline int vnode_lookup(struct vnode* restrict node, const char* restrict name, struct vnode** restrict out)
{
    if(node->ops->lookup == nullptr)
        return -ENOSYS;
    return node->ops->lookup(node, name, out);
}

static __always_inline inline int vnode_iterate(struct vnode* restrict node, struct dirent* restrict out, size_t index)
{
    if(node->ops->iterate == nullptr)
        return -ENOSYS;
    return node->ops->iterate(node, out, index);
}

static __always_inline inline int vnode_create(struct vnode* restrict node, const char* restrict name, unsigned type,
    struct vnode** restrict out)
{
    if(node->ops->create == nullptr)
        return -ENOSYS;
    return node->ops->create(node, name, type, out);
}

static __always_inline inline int vnode_unlink(struct vnode* restrict node, const char* restrict name)
{
    if(node->ops->unlink == nullptr)
        return -ENOSYS;
    return node->ops->unlink(node, name);
}

static __always_inline inline int vnode_open(struct vnode* restrict node, unsigned mode)
{
    if(node->ops->open == nullptr)
        return -ENOSYS;
    return node->ops->open(node, mode);
}

static __always_inline inline int vnode_close(struct vnode* restrict node)
{
    if(node->ops->close == nullptr)
        return -ENOSYS;
    return node->ops->close(node);
}

static __always_inline inline ssize_t vnode_read(struct vnode* restrict node, void* restrict buffer, size_t sz, uintptr_t offset)
{
    if(node->ops->read == nullptr)
        return -ENOSYS;
    return node->ops->read(node, buffer, sz, offset);
}

static __always_inline inline ssize_t vnode_write(struct vnode* restrict node, const void* restrict buffer, size_t sz, uintptr_t offset)
{
    if(node->ops->write == nullptr)
        return -ENOSYS;
    return node->ops->write(node, buffer, sz, offset);
}

static __always_inline inline int vnode_stat(struct vnode* restrict node, struct __k_stat* restrict out)
{
    if(node->ops->stat == nullptr)
        return -ENOSYS;
    return node->ops->stat(node, out);
}

static __always_inline inline int vnode_ioctl(struct vnode* restrict node, uint32_t request, void* restrict argument)
{
    if(node->ops->ioctl == nullptr)
        return -ENOSYS;
    return node->ops->ioctl(node, request, argument);
}

#endif /* EAAEA97D_DF46_4C1D_9BCC_3810B6FA33F7 */
