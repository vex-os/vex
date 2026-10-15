#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

struct vfs;
struct vnode;

struct vcache {
    struct vcache* next;
    struct vnode* node;
    ino_t nodenumber;
    struct vfs* fs;
};

struct vnode* vcache_get(struct vfs* restrict fs, ino_t nodenumber);
struct vnode* vcache_get_or_insert(struct vnode* restrict node) __nodiscard;
void vcache_evict(struct vfs* restrict fs);
