#pragma once

#include <vex/compiler.h>
#include <vex/errno.h>
#include <vex/statvfs.h>
#include <vex/types.h>

typedef struct cmdline_s cmdline_t;
typedef struct blk_device_s blk_device_t;
typedef struct vnode_s vnode_t;

typedef struct vfs_s vfs_t;
typedef struct vfs_ops_s vfs_ops_t;
typedef struct vfs_driver_s vfs_driver_t;
typedef struct vfs_driver_private_s vfs_driver_private_t;

struct vfs_ops_s {
    int (*probe)(vfs_t* restrict fs, const blk_device_t* restrict device);
    int (*mount)(vfs_t* restrict fs, const blk_device_t* restrict device, const cmdline_t* restrict params);
    int (*statvfs)(vfs_t* restrict fs, statvfs_t* restrict out);
    int (*fillnode)(vfs_t* restrict fs, vnode_t* restrict node);
    int (*unmount)(vfs_t* restrict fs);
    int (*sync)(vfs_t* restrict fs);
};

struct vfs_s {
    const vfs_ops_t* ops;
    vnode_t* mountpoint;
    vnode_t* root;
    const blk_device_t* device; // Referenced for as long as the filesystem is mounted
    void* ctxdata;
    bool readonly;    // Set by the driver while mounting
    bool unmounting;  // Under mounts_lock; turns away operations crossing into the filesystem
    unsigned orphans; // Unlinked vnodes still referenced; they keep the filesystem busy
};

struct vfs_driver_private_s {
    vfs_driver_t* next;
};

struct vfs_driver_s {
    vfs_driver_private_t priv;
    const vfs_ops_t* restrict ops;
    const char* name;
};

int vfs_driver_add(vfs_driver_t* restrict driver);
int vfs_driver_remove(vfs_driver_t* restrict driver);

int vfs_mount(const char* restrict type, const char* restrict path, const blk_device_t* restrict device, const char* restrict params);
int vfs_unmount(const char* restrict path);

int vfs_sync_fs(vfs_t* restrict fs);
int vfs_sync_all(void);
int vfs_statvfs(vfs_t* restrict fs, statvfs_t* restrict out);

vnode_t* vfs_get_root(void);
vnode_t* vfs_get_mountpoint(vfs_t* restrict fs);
vfs_t* vfs_get_fs(vnode_t* restrict node);
int vfs_cross_mount(vnode_t* restrict node, vnode_t** restrict out);
