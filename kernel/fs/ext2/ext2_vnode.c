#include <vex/blk_device.h>
#include <vex/errno.h>
#include <vex/fcache.h>
#include <vex/mount.h>
#include <vex/mutex.h>
#include <vex/pslab.h>
#include <vex/slab.h>
#include <vex/string.h>
#include <vex/strings.h>
#include <vex/vnode.h>

#include "ext2.h"

static struct mutex* fs_lock(struct vnode* restrict node)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    return &ctx->fs_ctx->lock;
}

static bool is_fast_symlink(const struct ext2_vfs_context* restrict fs_ctx, const struct ext2_inode* restrict inode)
{
    uint32_t xattr_sectors = 0;

    if(inode->i_file_acl) {
        xattr_sectors = fs_ctx->block_size / 512;
    }

    return xattr_sectors == le32toh(inode->i_blocks);
}

static int entry_nodetype(struct ext2_vfs_context* restrict fs_ctx, const struct ext2_dirent* restrict entry, unsigned* restrict out)
{
    switch(entry->file_type) {
        case EXT2_FT_REG_FILE:
            *out = VNODE_FILE;
            return 0;

        case EXT2_FT_DIR:
            *out = VNODE_DIRECTORY;
            return 0;

        case EXT2_FT_SYMLINK:
            *out = VNODE_SYM_LINK;
            return 0;

        case EXT2_FT_UNKNOWN:
            break;

        default:
            *out = VNODE_FILE; // No vnode types for devices, FIFOs and sockets yet
            return 0;
    }

    struct ext2_inode inode;
    int retval = ext2_inode_read(fs_ctx, entry->ino, &inode);

    if(retval < 0) {
        return retval;
    }

    if(ext2_inode_nodetype(le16toh(inode.i_mode), out) < 0) {
        *out = VNODE_FILE;
    }

    return 0;
}

static int lookup_locked(struct vnode* restrict node, const char* restrict name, ino_t* restrict out)
{
    if(node->nodetype != VNODE_DIRECTORY) {
        return -ENOTDIR;
    }

    if(strlen(name) > EXT2_NAME_LEN) {
        return -ENAMETOOLONG;
    }

    struct ext2_dirent entry;
    int retval = ext2_dirent_find(node, name, &entry);

    if(retval < 0) {
        return retval;
    }

    *out = entry.ino;

    return 0;
}

static int iterate_locked(struct vnode* restrict node, struct k_dirent* restrict out, size_t index)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    struct ext2_dirent entry;
    uint64_t offset = 0;
    size_t visible = 0;

    if(node->nodetype != VNODE_DIRECTORY) {
        return -ENOTDIR;
    }

    for(;;) {
        int retval = ext2_dirent_read(node, &offset, &entry);

        if(retval < 0) {
            return retval;
        }

        if(visible < index) {
            visible += 1;
            continue;
        }

        unsigned nodetype;
        retval = entry_nodetype(ctx->fs_ctx, &entry, &nodetype);

        if(retval < 0) {
            return retval;
        }

        out->d_ino = entry.ino;
        out->d_nodetype = nodetype;

        memset(out->d_name, 0, NAME_SIZE);
        kstrncpy(out->d_name, entry.name, NAME_SIZE);

        return 0;
    }
}

static ssize_t readlink_locked(struct vnode* restrict node, char* restrict buf, size_t sz)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    const struct ext2_inode* inode = &ctx->inode;
    uint64_t size = ext2_inode_size(inode);

    if(node->nodetype != VNODE_SYM_LINK) {
        return -EINVAL;
    }

    if(sz > size) {
        sz = size;
    }

    if(!is_fast_symlink(ctx->fs_ctx, inode)) {
        return fcache_read(node, &ext2_bmap, buf, sz, 0, size);
    }

    if(size > sizeof(inode->i_block)) {
        return -EIO;
    }

    memcpy(buf, inode->i_block, sz);

    return sz;
}

static int write_inode_locked(struct vnode* restrict node)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    return ext2_inode_write(ctx->fs_ctx, (uint32_t)(node->nodenumber), &ctx->inode);
}

static bool block_mapped_locked(struct vnode* restrict node, uint64_t pos)
{
    struct fcache_extent extent;
    size_t sector_size = node->fs->device->sector_size;
    return ext2_bmap(node, pos / sector_size, false, &extent) == 0 && extent.mapped;
}

static ssize_t write_span_locked(struct vnode* restrict node, const void* restrict buffer, size_t sz, uint64_t offset)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    uint64_t size = ext2_inode_size(&ctx->inode);
    ssize_t written = fcache_write(node, &ext2_bmap, buffer, sz, offset, size);

    if(written <= 0 || offset + written <= size) {
        return written;
    }

    int retval = ext2_inode_set_size(ctx->fs_ctx, &ctx->inode, offset + written);

    if(retval == 0) {
        retval = write_inode_locked(node);
    }

    if(retval < 0) {
        return retval;
    }

    return written;
}

static int zero_fill_locked(struct vnode* restrict node, uint64_t from, uint64_t to)
{
    void* zeroes = pslab_alloc_virt();

    if(zeroes == nullptr) {
        return -ENOMEM;
    }

    memset(zeroes, 0, PAGE_SIZE);

    int retval = 0;

    while(from < to) {
        size_t chunk = to - from;

        if(chunk > PAGE_SIZE) {
            chunk = PAGE_SIZE;
        }

        ssize_t written = write_span_locked(node, zeroes, chunk, from);

        if(written < 0) {
            retval = written;
            break;
        }

        from += written;
    }

    pslab_free_virt(zeroes);

    return retval;
}

static int fill_hole_locked(struct vnode* restrict node, uint64_t pos, uint64_t limit)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    size_t block_size = ctx->fs_ctx->block_size;

    if(block_mapped_locked(node, pos)) {
        return 0;
    }

    uint64_t start = pos - pos % block_size;
    uint64_t stop = start + block_size;

    if(stop > limit) {
        stop = limit;
    }

    return zero_fill_locked(node, start, stop);
}

static int clear_tail_locked(struct vnode* restrict node, uint64_t size, uint64_t new_size)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    size_t block_size = ctx->fs_ctx->block_size;

    if(0 == size % block_size || !block_mapped_locked(node, size)) {
        return 0;
    }

    uint64_t stop = size - size % block_size + block_size;

    if(stop > new_size) {
        stop = new_size;
    }

    return zero_fill_locked(node, size, stop);
}

static ssize_t write_locked(struct vnode* restrict node, const void* restrict buffer, size_t sz, off_t offset)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    size_t block_size = ctx->fs_ctx->block_size;

    if(node->fs->readonly) {
        return -EROFS;
    }

    if(offset < 0) {
        return -EINVAL;
    }

    if(sz == 0) {
        return 0;
    }

    uint64_t size = ext2_inode_size(&ctx->inode);
    uint64_t end = offset + sz;
    uint64_t limit = size;

    if(end > limit) {
        limit = end;
    }

    int retval = 0;

    if((uint64_t)offset > size) {
        retval = clear_tail_locked(node, size, offset);
    }

    if(retval == 0) {
        retval = fill_hole_locked(node, offset, limit);
    }

    if(retval == 0 && (end - 1) / block_size != (uint64_t)offset / block_size) {
        retval = fill_hole_locked(node, end - 1, limit);
    }

    if(retval < 0) {
        return retval;
    }

    return write_span_locked(node, buffer, sz, offset);
}

static int resize_locked(struct vnode* restrict node, uint64_t new_size)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    uint64_t size = ext2_inode_size(&ctx->inode);
    int retval = 0;

    if(new_size > size) {
        retval = clear_tail_locked(node, size, new_size);
    }
    else if(new_size < size) {
        retval = ext2_truncate_blocks(node, new_size);
    }

    if(retval == 0) {
        retval = ext2_inode_set_size(ctx->fs_ctx, &ctx->inode, new_size);
    }

    if(retval == 0) {
        retval = write_inode_locked(node);
    }

    return retval;
}

static int setattr_locked(struct vnode* restrict node, const struct vattr* restrict attr)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    struct ext2_vfs_context* fs_ctx = ctx->fs_ctx;
    struct ext2_inode* inode = &ctx->inode;

    if(node->fs->readonly) {
        return -EROFS;
    }

    if(attr->mask & (VATTR_UID | VATTR_GID)) {
        uid_t uid = ext2_inode_uid(fs_ctx, inode);
        gid_t gid = ext2_inode_gid(fs_ctx, inode);

        if(attr->mask & VATTR_UID) {
            uid = attr->uid;
        }

        if(attr->mask & VATTR_GID) {
            gid = attr->gid;
        }

        if(!ext2_inode_set_owner(fs_ctx, inode, uid, gid)) {
            return -EINVAL;
        }
    }

    if(attr->mask & VATTR_MODE) {
        uint16_t mode = le16toh(inode->i_mode);
        inode->i_mode = htole16((mode & S_IFMT) | (attr->mode & ~S_IFMT));
    }

    if(attr->mask & VATTR_SIZE) {
        return resize_locked(node, attr->size);
    }

    if(attr->mask & (VATTR_UID | VATTR_GID | VATTR_MODE)) {
        return write_inode_locked(node);
    }

    return 0;
}

static void ext2_vnode_release(struct vnode* restrict node)
{
    slab_free(node->ctxdata);
}

static int ext2_vnode_lookup(struct vnode* restrict node, const char* restrict name, ino_t* restrict out)
{
    mutex_acquire(fs_lock(node));
    int retval = lookup_locked(node, name, out);
    mutex_release(fs_lock(node));
    return retval;
}

static int ext2_vnode_iterate(struct vnode* restrict node, struct k_dirent* restrict out, size_t index)
{
    mutex_acquire(fs_lock(node));
    int retval = iterate_locked(node, out, index);
    mutex_release(fs_lock(node));
    return retval;
}

static ssize_t ext2_vnode_read(struct vnode* restrict node, void* restrict buffer, size_t sz, off_t offset)
{
    struct ext2_vnode_context* ctx = node->ctxdata;

    if(node->nodetype == VNODE_DIRECTORY) {
        return -EISDIR;
    }

    if(node->nodetype != VNODE_FILE) {
        return -EINVAL;
    }

    mutex_acquire(fs_lock(node));
    ssize_t retval = fcache_read(node, &ext2_bmap, buffer, sz, offset, ext2_inode_size(&ctx->inode));
    mutex_release(fs_lock(node));

    return retval;
}

static ssize_t ext2_vnode_write(struct vnode* restrict node, const void* restrict buffer, size_t sz, off_t offset)
{
    if(node->nodetype == VNODE_DIRECTORY) {
        return -EISDIR;
    }

    if(node->nodetype != VNODE_FILE) {
        return -EINVAL;
    }

    mutex_acquire(fs_lock(node));
    ssize_t retval = write_locked(node, buffer, sz, offset);
    mutex_release(fs_lock(node));

    return retval;
}

static ssize_t ext2_vnode_readlink(struct vnode* restrict node, char* restrict buf, size_t sz)
{
    mutex_acquire(fs_lock(node));
    ssize_t retval = readlink_locked(node, buf, sz);
    mutex_release(fs_lock(node));
    return retval;
}

static int ext2_vnode_stat(struct vnode* restrict node, struct k_stat* restrict out)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    struct ext2_vfs_context* fs_ctx = ctx->fs_ctx;
    const struct ext2_inode* inode = &ctx->inode;

    memset(out, 0, sizeof(struct k_stat));

    mutex_acquire(&fs_ctx->lock);

    out->st_ino = node->nodenumber;
    out->st_mode = le16toh(inode->i_mode);
    out->st_nlink = le16toh(inode->i_links_count);
    out->st_uid = ext2_inode_uid(fs_ctx, inode);
    out->st_gid = ext2_inode_gid(fs_ctx, inode);
    out->st_size = (off_t)ext2_inode_size(inode);
    out->st_atime = le32toh(inode->i_atime);
    out->st_mtime = le32toh(inode->i_mtime);
    out->st_ctime = le32toh(inode->i_ctime);
    out->st_blksize = fs_ctx->block_size;
    out->st_blocks = le32toh(inode->i_blocks);

    mutex_release(&fs_ctx->lock);

    return 0;
}

static int ext2_vnode_setattr(struct vnode* restrict node, const struct vattr* restrict attr)
{
    mutex_acquire(fs_lock(node));
    int retval = setattr_locked(node, attr);
    mutex_release(fs_lock(node));
    return retval;
}

static int ext2_vnode_fsync(struct vnode* restrict node)
{
    struct ext2_vnode_context* ctx = node->ctxdata;

    mutex_acquire(fs_lock(node));
    int retval = fcache_writeback(node, &ext2_bmap, ext2_inode_size(&ctx->inode));
    mutex_release(fs_lock(node));

    return retval;
}

static int ext2_vnode_map_page(struct vnode* restrict node, uint64_t index, struct pagemap* restrict pmap, uintptr_t virt, unsigned vprot)
{
    struct ext2_vnode_context* ctx = node->ctxdata;

    if(node->nodetype != VNODE_FILE) {
        return -ENODEV;
    }

    mutex_acquire(fs_lock(node));
    int retval = fcache_map_page(node, &ext2_bmap, index, ext2_inode_size(&ctx->inode), pmap, virt, vprot);
    mutex_release(fs_lock(node));

    return retval;
}

const struct vnode_ops ext2_vnode_ops = {
    .release = &ext2_vnode_release,
    .lookup = &ext2_vnode_lookup,
    .iterate = &ext2_vnode_iterate,
    .read = &ext2_vnode_read,
    .write = &ext2_vnode_write,
    .readlink = &ext2_vnode_readlink,
    .stat = &ext2_vnode_stat,
    .setattr = &ext2_vnode_setattr,
    .fsync = &ext2_vnode_fsync,
    .map_page = &ext2_vnode_map_page,
};
