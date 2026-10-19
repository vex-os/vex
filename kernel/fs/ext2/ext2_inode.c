#include <vex/blk_device.h>
#include <vex/errno.h>
#include <vex/fcache.h>
#include <vex/kprintf.h>
#include <vex/vnode.h>

#include "ext2.h"

static int inode_offset(const struct ext2_vfs_context* restrict context, uint32_t ino, uint64_t* restrict out)
{
    if(ino == 0 || ino > le32toh(context->super.s_inodes_count)) {
        return -EINVAL;
    }

    uint32_t group = (ino - 1) / context->inodes_per_group;
    uint32_t index = (ino - 1) % context->inodes_per_group;
    const struct ext2_group_desc* desc = &context->groups[group];

    *out = ext2_block_offset(context, le32toh(desc->bg_inode_table)) + (uint64_t)index * context->inode_size;

    return 0;
}

static bool data_block_valid(const struct ext2_vfs_context* restrict context, uint32_t block)
{
    return block >= context->first_data_block && block < le32toh(context->super.s_blocks_count);
}

static int indirect_entry(struct ext2_vfs_context* restrict context, uint32_t block, uint64_t index, uint32_t* restrict out)
{
    uint32_t value;
    uint64_t offset = ext2_block_offset(context, block) + index * sizeof(uint32_t);
    int retval = fcache_dev_read(context->devnode, offset, &value, sizeof(value));

    if(retval < 0) {
        return retval;
    }

    *out = le32toh(value);

    return 0;
}

static int block_lookup(struct ext2_vfs_context* restrict context, const struct ext2_inode* restrict inode, uint64_t lbn,
    uint32_t* restrict out)
{
    if(lbn < EXT2_NDIR_BLOCKS) {
        *out = le32toh(inode->i_block[lbn]);
        return 0;
    }

    uint64_t addr_per_block = context->addr_per_block;
    uint64_t span = addr_per_block;
    unsigned depth = 1;

    lbn -= EXT2_NDIR_BLOCKS;

    while(lbn >= span) {
        if(depth == 3) {
            return -EFBIG;
        }

        lbn -= span;
        span *= addr_per_block;
        depth += 1;
    }

    uint32_t block = le32toh(inode->i_block[EXT2_IND_BLOCK + depth - 1]);

    while(block && depth > 0) {
        if(!data_block_valid(context, block)) {
            return -EIO;
        }

        span /= addr_per_block;

        int retval = indirect_entry(context, block, lbn / span, &block);

        if(retval < 0) {
            return retval;
        }

        lbn %= span;
        depth -= 1;
    }

    *out = block;

    return 0;
}

int ext2_inode_read(struct ext2_vfs_context* restrict context, uint32_t ino, struct ext2_inode* restrict out)
{
    uint64_t offset;
    int retval = inode_offset(context, ino, &offset);

    if(retval < 0) {
        return retval;
    }

    return fcache_dev_read(context->devnode, offset, out, sizeof(struct ext2_inode));
}

int ext2_inode_nodetype(uint16_t mode, unsigned* restrict out)
{
    switch(mode & S_IFMT) {
        case S_IFREG:
            *out = VNODE_FILE;
            return 0;

        case S_IFDIR:
            *out = VNODE_DIRECTORY;
            return 0;

        case S_IFLNK:
            *out = VNODE_SYM_LINK;
            return 0;

        default:
            return -EOPNOTSUPP;
    }
}

int ext2_bmap(struct vnode* restrict node, uint64_t sector, bool create, struct fcache_extent* restrict out)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    struct ext2_vfs_context* fs_ctx = ctx->fs_ctx;

    if(create) {
        return -EROFS;
    }

    uint64_t in_block = sector % fs_ctx->sectors_per_block;
    uint32_t block;
    int retval = block_lookup(fs_ctx, &ctx->inode, sector / fs_ctx->sectors_per_block, &block);

    if(retval < 0) {
        return retval;
    }

    out->count = fs_ctx->sectors_per_block - in_block;

    if(block == 0) {
        out->lba = 0;
        out->mapped = false;
        return 0;
    }

    if(!data_block_valid(fs_ctx, block)) {
        pr_warning("ext2: %s: inode %u maps a block outside the filesystem", fs_ctx->device->identifier, (uint32_t)(node->nodenumber));
        return -EIO;
    }

    out->lba = ext2_block_lba(fs_ctx, block) + in_block;
    out->mapped = true;

    return 0;
}
