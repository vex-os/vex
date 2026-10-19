#include <vex/errno.h>
#include <vex/fcache.h>
#include <vex/mutex.h>
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
    .readlink = &ext2_vnode_readlink,
    .stat = &ext2_vnode_stat,
    .map_page = &ext2_vnode_map_page,
};
