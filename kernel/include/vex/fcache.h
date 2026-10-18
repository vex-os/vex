#pragma once

#include <vex/compiler.h>
#include <vex/mutex.h>
#include <vex/radix.h>
#include <vex/types.h>

struct vfs;
struct vnode;

struct fcache {
    struct mutex lock;
    struct radix tree;
    size_t num_pages;
    size_t num_dirty;
    bool queued;
    struct vnode* dirty_next;
};

struct fcache_extent {
    uint64_t lba;
    size_t count;
    bool mapped;
};

// Called from within the fcache call the filesystem made,
// with whatever locks the filesystem took around that call
typedef int (*fcache_bmap_type)(struct vnode* restrict node, uint64_t sector, bool create, struct fcache_extent* restrict out);

void fcache_init(struct fcache* restrict cache);
void fcache_deinit(struct fcache* restrict cache);

ssize_t fcache_read(struct vnode* restrict node, fcache_bmap_type bmap, void* restrict buffer, size_t sz, off_t offset, off_t size);
ssize_t fcache_write(struct vnode* restrict node, fcache_bmap_type bmap, const void* restrict buffer, size_t sz, off_t offset, off_t size);
int fcache_writeback(struct vnode* restrict node, fcache_bmap_type bmap, off_t size);
void fcache_truncate(struct vnode* restrict node, off_t size);
void fcache_forget(struct vnode* restrict node, uint64_t offset, uint64_t length);

int fcache_sync(struct vfs* restrict fs); // nullptr syncs all filesystems
size_t fcache_reclaim(size_t count);

struct vnode* fcache_dev_create(struct vfs* restrict fs) __nodiscard;
void fcache_dev_destroy(struct vnode* restrict devnode);
int fcache_dev_read(struct vnode* restrict devnode, uint64_t offset, void* restrict buffer, size_t sz);
int fcache_dev_write(struct vnode* restrict devnode, uint64_t offset, const void* restrict buffer, size_t sz);
