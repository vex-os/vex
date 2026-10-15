#pragma once

#include <vex/compiler.h>
#include <vex/limits.h>

#define PATH_SEPARATOR 0x2F

struct vnode;

int path_resolve(const char* restrict path, struct vnode** restrict out);
int path_resolve_at(struct vnode* restrict stem, const char* restrict path, struct vnode** restrict out);
int path_resolve_parent(const char* restrict path, struct vnode** restrict out_parent, const char** restrict out_name);
