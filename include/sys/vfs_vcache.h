#ifndef CA22D52B_2CBA_48DA_B98C_D9DBB8F9411A
#define CA22D52B_2CBA_48DA_B98C_D9DBB8F9411A
#pragma once

#include <sys/cdefs.h>
#include <sys/types.h>

struct vfs;
struct vnode;

struct vcache {
    struct vcache* next;
    struct vnode* node;
    ino_t nodenumber;
    struct vfs* fs;
};

struct vnode* vcache_get(struct vfs* restrict fs, ino_t nodenumber);
int vcache_insert(struct vnode* restrict node);
void vcache_evict(struct vfs* restrict fs);

#endif /* CA22D52B_2CBA_48DA_B98C_D9DBB8F9411A */
