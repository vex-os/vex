#ifndef B81C08C7_68D9_44CC_AC20_B3B796BB8810
#define B81C08C7_68D9_44CC_AC20_B3B796BB8810
#pragma once

#include <core/cdefs.h>
#include <core/spinlock.h>
#include <core/types.h>

struct vfs_dirent;
struct vfs_stat;
struct vfs;

struct vfs_node;
struct vfs_node_ops {
    int (*lookup)(struct vfs_node* restrict dir, const char* restrict name, struct vfs_node** restrict out);
    int (*open)(struct vfs_node* restrict node, int flags);
    int (*close)(struct vfs_node* restrict node);
    ssize_t (*read)(struct vfs_node* restrict node, void* restrict buffer, size_t sz, off_t offset);
    ssize_t (*write)(struct vfs_node* restrict node, const void* restrict buffer, size_t sz, off_t offset);
    int (*read_dir)(struct vfs_node* restrict dir, struct vfs_dirent* out_dirent, off_t index);
    int (*create)(struct vfs_node* restrict dir, const char* restrict name, unsigned type, struct vfs_node** restrict out);
    int (*unlink)(struct vfs_node* restrict dir, const char* restrict name);
    int (*ioctl)(struct vfs_node* restrict node, unsigned request, void* restrict arg);
    int (*stat)(struct vfs_node* restrict node, struct vfs_stat* restrict out_stat);
};

struct vfs_node {
    struct vfs* fs;
    struct vfs* fs_mounted;
    const struct vfs_node_ops* ops;
    spinlock_type lock;
    unsigned refcount;
    unsigned type;
    void* ctxdata;
};

extern struct vfs_node* vfs_node_alloc(unsigned type, const struct vfs_node_ops* restrict ops, void* ctxdata) __nodiscard;
extern void vfs_node_inc_refcount(struct vfs_node* restrict node);
extern void vfs_node_dec_refcount(struct vfs_node* restrict node);

#endif /* B81C08C7_68D9_44CC_AC20_B3B796BB8810 */
