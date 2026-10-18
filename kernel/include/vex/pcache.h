#pragma once

#include <vex/compiler.h>
#include <vex/mutex.h>
#include <vex/radix.h>
#include <vex/types.h>

struct vfs;
struct vnode;

struct pcache {
    struct mutex lock;
    struct radix pages;
    size_t num_pages;
    size_t num_dirty;
    bool queued;
    struct vnode* dirty_next;
};

struct pcache_extent {
    uint64_t lba;
    size_t count;
    bool mapped;
};

// Called from within the pcache call the filesystem made,
// with whatever locks the filesystem took around that call
typedef int (*pcache_bmap_type)(struct vnode* restrict node, uint64_t sector, bool create, struct pcache_extent* restrict out);

void pcache_init(struct pcache* restrict cache);
void pcache_deinit(struct pcache* restrict cache);

ssize_t pcache_read(struct vnode* restrict node, pcache_bmap_type bmap, void* restrict buffer, size_t sz, off_t offset, off_t size);
ssize_t pcache_write(struct vnode* restrict node, pcache_bmap_type bmap, const void* restrict buffer, size_t sz, off_t offset, off_t size);
int pcache_writeback(struct vnode* restrict node, pcache_bmap_type bmap, off_t size);
void pcache_truncate(struct vnode* restrict node, off_t size);
void pcache_forget(struct vnode* restrict node, uint64_t offset, uint64_t length);

int pcache_sync(struct vfs* restrict fs); // nullptr syncs all filesystems
size_t pcache_reclaim(size_t count);

struct vnode* pcache_meta_create(struct vfs* restrict fs) __nodiscard;
void pcache_meta_destroy(struct vnode* restrict meta);
int pcache_meta_read(struct vnode* restrict meta, uint64_t offset, void* restrict buffer, size_t sz);
int pcache_meta_write(struct vnode* restrict meta, uint64_t offset, const void* restrict buffer, size_t sz);
