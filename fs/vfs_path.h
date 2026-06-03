#ifndef B8BF5B2E_8040_427C_ACA0_904F43379E8A
#define B8BF5B2E_8040_427C_ACA0_904F43379E8A
#pragma once

#include <core/cdefs.h>

#define PATH_SEPARATOR 0x2F

struct vfs_node;

extern int vfs_lookup(const char* restrict path, struct vfs_node** restrict out);
extern int vfs_lookup_at(struct vfs_node* restrict base, const char* restrict path, struct vfs_node** restrict out);

#endif /* B8BF5B2E_8040_427C_ACA0_904F43379E8A */
