#pragma once

#include <vex/compiler.h>
#include <vex/path.h>
#include <vex/spinlock.h>

typedef struct block_interface_s block_interface_t;
typedef struct char_interface_s char_interface_t;
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

extern void devfs_block_event_handler(unsigned event, const block_interface_t* restrict interface, void* restrict ctxdata);
extern void devfs_char_event_handler(unsigned event, const char_interface_t* restrict interface, void* restrict ctxdata);
