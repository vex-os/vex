#ifndef B4D08648_671B_4E1D_9F33_80B21DA77044
#define B4D08648_671B_4E1D_9F33_80B21DA77044
#pragma once

#include <sys/cdefs.h>
#include <sys/spinlock.h>
#include <sys/vfs_path.h>

struct blk_device;
struct chr_device;
struct vfs;

struct devfs_node {
    char name[NAME_SIZE];
    struct devfs_node* next;
    struct vnode* node;
};

struct devfs_state {
    spinlock_type lock;
    struct devfs_node* head;
    size_t num_devices;
};

extern const struct vnode_ops devfs_dir_ops;

extern int devfs_insert(struct vfs* restrict fs, const char* restrict name, struct vnode* restrict node);
extern void devfs_remove(struct vfs* restrict fs, const char* restrict name);

extern void devfs_blk_event_handler(unsigned event, const struct blk_device* restrict device, void* restrict ctxdata);
extern void devfs_chr_event_handler(unsigned event, const struct chr_device* restrict device, void* restrict ctxdata);

#endif /* B4D08648_671B_4E1D_9F33_80B21DA77044 */
