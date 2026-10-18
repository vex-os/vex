#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

struct vfs;
struct vnode;

struct vcache {
    struct vcache* next;
    struct vcache* unused_prev;
    struct vcache* unused_next;
    struct vnode* node;
    ino_t nodenumber;
    struct vfs* fs;
    bool unused;
    bool pinned; // The owner holds a reference until vcache_remove
};

struct vnode* vcache_get(struct vfs* restrict fs, ino_t nodenumber);
struct vnode* vcache_get_or_insert(struct vnode* restrict node) __nodiscard;
int vcache_insert_pinned(struct vnode* restrict node) __nodiscard;
bool vcache_keep(struct vnode* restrict node) __nodiscard;
bool vcache_in_use(struct vfs* restrict fs, unsigned root_refs) __nodiscard;
void vcache_evict(struct vfs* restrict fs);
void vcache_remove(struct vnode* restrict node);
void vcache_rekey(struct vnode* restrict node, ino_t nodenumber);
