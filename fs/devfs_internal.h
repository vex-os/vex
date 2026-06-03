#ifndef B4D08648_671B_4E1D_9F33_80B21DA77044
#define B4D08648_671B_4E1D_9F33_80B21DA77044
#pragma once

#include <core/cdefs.h>
#include <core/spinlock.h>
#include <fs/vfs_types.h>

struct vfs_node;
struct blk_device;
struct chr_device;

struct devfs_node {
    char name[VFS_NAME_SIZE];
    struct vfs_node* vnode;
    struct devfs_node* next;
};

struct devfs_state {
    spinlock_type lock;
    struct devfs_node* head;
};

extern struct vfs* devfs_instance;
extern const struct vfs_node_ops devfs_dir_ops;

extern int devfs_insert(const char* restrict name, struct vfs_node* restrict vnode);
extern void devfs_remove(const char* restrict name);

extern void devfs_blk_event_handler(unsigned event, const struct blk_device* restrict device, void* restrict ctxdata);
extern void devfs_chr_event_handler(unsigned event, const struct chr_device* restrict device, void* restrict ctxdata);

#endif /* B4D08648_671B_4E1D_9F33_80B21DA77044 */
