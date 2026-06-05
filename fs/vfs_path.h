#ifndef F4CD25DE_A63C_47FB_8669_3F9D595AB108
#define F4CD25DE_A63C_47FB_8669_3F9D595AB108
#pragma once

#include <arch/limits.h>
#include <core/cdefs.h>

#define PATH_SEPARATOR 0x2F

#define PATH_NAME_MAX  255
#define PATH_NAME_SIZE 256

#define PATH_MAX  PAGE_MAX
#define PATH_SIZE PAGE_SIZE

struct vnode;

int path_resolve(const char* restrict path, struct vnode** restrict out);
int path_resolve_at(struct vnode* restrict stem, const char* restrict path, struct vnode** restrict out);

#endif /* F4CD25DE_A63C_47FB_8669_3F9D595AB108 */
