#pragma once

#include <vex/compiler.h>
#include <vex/limits.h>

typedef struct credentials_s credentials_t;
typedef struct vnode_s vnode_t;

#define PATH_SEPARATOR 0x2F

#define PATH_NOFOLLOW 0x0001U // A symlink in the last component is returned, not followed

int path_resolve(const char* restrict path, const credentials_t* restrict credentials, vnode_t** restrict out);
int path_resolve_at(vnode_t* restrict base_dir, const char* restrict path, unsigned flags, const credentials_t* restrict credentials,
    vnode_t** restrict out);
int path_resolve_parent_at(vnode_t* restrict base_dir, const char* restrict path, const credentials_t* restrict credentials,
    vnode_t** restrict out_parent, const char** restrict out_name);
