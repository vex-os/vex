#ifndef A5A3A132_F6DA_406A_864B_C0F7543F7CDD
#define A5A3A132_F6DA_406A_864B_C0F7543F7CDD
#pragma once

#include <core/cdefs.h>
#include <core/types.h>

struct cmdline;

struct blk_device;

struct vfs;
struct vfs_ops {
    int (*mount)(struct vfs* restrict fs, struct blk_device* restrict device, const struct cmdline* restrict params);
    int (*unmount)(struct vfs* restrict fs);
    int (*sync)(struct vfs* restrict fs);
};

struct vfs_node;
struct vfs {
    const struct vfs_ops* ops;
    struct vfs_node* mountpoint;
    struct vfs_node* root;
    void* ctxdata;
};

struct vfs_driver;
struct vfs_driver_private {
    struct vfs_driver* next;
};

struct vfs_driver {
    struct vfs_driver_private priv;
    const struct vfs_ops* ops;
    char identifier[64];
};

extern int vfs_driver_register(struct vfs_driver* restrict driver);
extern int vfs_driver_unregister(struct vfs_driver* restrict driver);

extern int vfs_mount(const char* restrict type, const char* restrict path, struct blk_device* restrict device, const char* restrict params);
extern int vfs_unmount(const char* restrict path);
extern int vfs_get_root(struct vfs_node** restrict out);

#endif /* A5A3A132_F6DA_406A_864B_C0F7543F7CDD */
