#pragma once

#include <vex/compiler.h>
#include <vex/path.h>
#include <vex/spinlock.h>

typedef struct blk_device_s blk_device_t;
typedef struct chr_device_s chr_device_t;
typedef struct vfs_s vfs_t;
typedef struct vnode_ops_s vnode_ops_t;

typedef struct devfs_node_s devfs_node_t;
typedef struct devfs_state_s devfs_state_t;

struct devfs_node_s {
    char name[NAME_SIZE];
    devfs_node_t* next;
    vnode_t* node;
};

struct devfs_state_s {
    spinlock_t lock;
    devfs_node_t* head;
    size_t num_devices;
};

extern const vnode_ops_t devfs_dir_ops;

extern int devfs_insert(vfs_t* restrict fs, const char* restrict name, vnode_t* restrict node);
extern void devfs_remove(vfs_t* restrict fs, const char* restrict name);
extern vnode_t* devfs_find(vfs_t* restrict fs, const char* restrict name);

extern void devfs_blk_event_handler(unsigned event, const blk_device_t* restrict device, void* restrict ctxdata);
extern void devfs_chr_event_handler(unsigned event, const chr_device_t* restrict device, void* restrict ctxdata);
