#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

typedef struct vfs_s vfs_t;
typedef struct vnode_s vnode_t;

typedef struct vcache_entry_s vcache_entry_t;

struct vcache_entry_s {
    vcache_entry_t* next;
    vcache_entry_t* unused_prev;
    vcache_entry_t* unused_next;
    vnode_t* node;
    ino_t ino;
    vfs_t* fs;
    bool unused;
    bool pinned; // The owner holds a reference until vcache_remove
};

vnode_t* vcache_get(vfs_t* restrict fs, ino_t ino);
vnode_t* vcache_get_or_insert(vnode_t* restrict node) __nodiscard;
int vcache_insert_pinned(vnode_t* restrict node) __nodiscard;
bool vcache_keep(vnode_t* restrict node) __nodiscard;
bool vcache_in_use(vfs_t* restrict fs, unsigned root_refs) __nodiscard;
void vcache_evict(vfs_t* restrict fs);
void vcache_remove(vnode_t* restrict node);
bool vcache_cached(vnode_t* restrict node) __nodiscard;
void vcache_rekey(vnode_t* restrict node, ino_t ino);
