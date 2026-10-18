#pragma once

#include <vex/compiler.h>
#include <vex/errno.h>
#include <vex/statvfs.h>
#include <vex/types.h>

struct cmdline;
struct blk_device;
struct vnode;

struct vfs;
struct vfs_ops {
    int (*probe)(struct vfs* restrict fs, const struct blk_device* restrict device);
    int (*mount)(struct vfs* restrict fs, const struct blk_device* restrict device, const struct cmdline* restrict params);
    int (*statvfs)(struct vfs* restrict fs, struct k_statvfs* restrict out);
    int (*fillnode)(struct vfs* restrict fs, struct vnode* restrict node);
    int (*unmount)(struct vfs* restrict fs);
    int (*sync)(struct vfs* restrict fs);
};

struct vfs {
    const struct vfs_ops* ops;
    struct vnode* mountpoint;
    struct vnode* root;
    const struct blk_device* device; // Referenced for as long as the filesystem is mounted
    void* ctxdata;
    bool readonly;    // Set by the driver while mounting
    bool unmounting;  // Under mounts_lock; turns away operations crossing into the filesystem
    unsigned orphans; // Unlinked vnodes still referenced; they keep the filesystem busy
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

int add_vfs_driver(struct vfs_driver* restrict driver);
int remove_vfs_driver(struct vfs_driver* restrict driver);

int vfs_mount(const char* restrict type, const char* restrict path, const struct blk_device* restrict device, const char* restrict params);
int vfs_unmount(const char* restrict path);

int vfs_sync_fs(struct vfs* restrict fs);
int vfs_sync_all(void);
int vfs_statvfs(struct vfs* restrict fs, struct k_statvfs* restrict out);

struct vnode* vfs_get_root(void);
struct vnode* vfs_get_mountpoint(struct vfs* restrict fs);
struct vfs* vfs_get_fs(struct vnode* restrict node);
int vfs_cross_mount(struct vnode* restrict node, struct vnode** restrict out);
