#include <vex/errno.h>
#include <vex/mutex.h>
#include <vex/slab.h>
#include <vex/string.h>
#include <vex/vnode.h>

#include "ext2.h"

static void ext2_vnode_release(struct vnode* restrict node)
{
    slab_free(node->ctxdata);
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

const struct vnode_ops ext2_vnode_ops = {
    .release = &ext2_vnode_release,
    .stat = &ext2_vnode_stat,
};
