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

/// Caches the node under its fs and ino, unless another vnode is cached there already
/// @return The node itself, the vnode already cached there with a new reference, the caller
/// then destroying its own node, or nullptr when out of memory

vnode_t* vcache_get_or_insert(vnode_t* restrict node) __nodiscard;

/// Caches a node its owner keeps a reference to until vcache_remove, as devfs does
/// @retval 0 The node is cached
/// @retval -EEXIST Another vnode is cached under the same key
/// @retval -ENOMEM Out of memory
int vcache_insert_pinned(vnode_t* restrict node) __nodiscard;

/// Keeps a cached node among the unused ones, evicting the oldest past the limit
/// @pre vnode_decref calls this once the count reaches zero
/// @return Whether the node lives on, so that the caller must not destroy it
bool vcache_keep(vnode_t* restrict node) __nodiscard;

/// Whether any vnode of the filesystem has references beyond the owners' references to pinned
/// nodes, the references directories hold to their parents, and root_refs to the root
bool vcache_in_use(vfs_t* restrict fs, unsigned root_refs) __nodiscard;
void vcache_evict(vfs_t* restrict fs);

/// Takes the node out of the cache, so that lookups no longer find
/// it; the vnode itself lives on until its last reference
/// @pre The caller holds a reference to the node
void vcache_remove(vnode_t* restrict node);
bool vcache_cached(vnode_t* restrict node) __nodiscard;

/// Moves the node to a new inode number
/// @pre Whatever was cached under that number is already removed
void vcache_rekey(vnode_t* restrict node, ino_t ino);
