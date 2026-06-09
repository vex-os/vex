#ifndef F4CD25DE_A63C_47FB_8669_3F9D595AB108
#define F4CD25DE_A63C_47FB_8669_3F9D595AB108
#pragma once

#include <vex/compiler.h>
#include <vex/limits.h>

#define PATH_SEPARATOR 0x2F

struct vnode;

int path_resolve(const char* restrict path, struct vnode** restrict out);
int path_resolve_at(struct vnode* restrict stem, const char* restrict path, struct vnode** restrict out);

#endif /* F4CD25DE_A63C_47FB_8669_3F9D595AB108 */
