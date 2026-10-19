#include <vex/errno.h>
#include <vex/fcache.h>

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

int ext2_inode_read(struct ext2_vfs_context* restrict context, uint32_t ino, struct ext2_inode* restrict out)
{
    uint64_t offset;
    int retval = inode_offset(context, ino, &offset);

    if(retval < 0) {
        return retval;
    }

    return fcache_dev_read(context->devnode, offset, out, sizeof(struct ext2_inode));
}
