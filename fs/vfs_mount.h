#ifndef E7F3A178_693A_4817_A6DA_3793C8B5CE3F
#define E7F3A178_693A_4817_A6DA_3793C8B5CE3F
#pragma once

#include <core/cdefs.h>
#include <core/types.h>

struct cmdline;
struct blk_device;
struct __k_statvfs;

struct vfs;
struct vfs_ops {
    int (*mount)(struct vfs* restrict fs, const struct blk_device* restrict device, const struct cmdline* restrict params);
    int (*statvfs)(struct vfs* restrict fs, struct __k_statvfs* restrict out);
    int (*unmount)(struct vfs* restrict fs);
    int (*sync)(struct vfs* restrict fs);
};

struct vnode;
struct vfs {
    const struct vfs_ops* ops;
    struct vnode* mountpoint;
    struct vnode* root;
    void* ctxdata;
};

struct vfs_driver;
struct vfs_driver_private {
    struct vfs_driver* next;
};

struct vfs_driver {
    struct vfs_driver_private priv;
    const struct vfs_ops* restrict ops;
    const char* identifier;
};

int vfs_driver_register(struct vfs_driver* restrict driver);
int vfs_driver_unregister(struct vfs_driver* restrict driver);

int vfs_mount(const char* restrict type, const char* restrict path, const struct blk_device* restrict device, const char* restrict params);
int vfs_unmount(const char* restrict path);
int vfs_sync(const char* restrict path);
int vfs_get_root(struct vnode** restrict out);

#endif /* E7F3A178_693A_4817_A6DA_3793C8B5CE3F */
