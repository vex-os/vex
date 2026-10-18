#pragma once

#include <vex/compiler.h>
#include <vex/limits.h>

#define PATH_SEPARATOR 0x2F

#define PATH_NOFOLLOW 0x0001U // A symlink in the last component is returned, not followed

struct credentials;
struct vnode;

int path_resolve(const char* restrict path, const struct credentials* restrict cred, struct vnode** restrict out);
int path_resolve_at(struct vnode* restrict stem, const char* restrict path, unsigned flags, const struct credentials* restrict cred,
    struct vnode** restrict out);
int path_resolve_parent_at(struct vnode* restrict stem, const char* restrict path, const struct credentials* restrict cred,
    struct vnode** restrict out_parent, const char** restrict out_name);
