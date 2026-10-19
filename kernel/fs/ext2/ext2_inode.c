#include <vex/blk_device.h>
#include <vex/errno.h>
#include <vex/fcache.h>
#include <vex/kprintf.h>
#include <vex/mount.h>
#include <vex/vnode.h>

#include "ext2.h"

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

static int table_get(struct ext2_vfs_context* restrict context, const struct ext2_inode* restrict inode, uint32_t table, uint64_t index,
    uint32_t* restrict out)
{
    if(table == 0) {
        *out = le32toh(inode->i_block[index]);
        return 0;
    }

    return indirect_entry(context, table, index, out);
}

static int table_set(struct ext2_vfs_context* restrict context, struct ext2_inode* restrict inode, uint32_t table, uint64_t index,
    uint32_t value)
{
    if(table == 0) {
        inode->i_block[index] = htole32(value);
        return 0;
    }

    uint32_t raw = htole32(value);
    uint64_t offset = ext2_block_offset(context, table) + index * sizeof(uint32_t);

    return fcache_dev_write(context->devnode, offset, &raw, sizeof(raw));
}

static void add_blocks(const struct ext2_vfs_context* restrict context, struct ext2_inode* restrict inode, int count)
{
    int64_t sectors = (int64_t)le32toh(inode->i_blocks) + count * (int64_t)(context->block_size / 512);

    if(sectors < 0) {
        sectors = 0;
    }

    inode->i_blocks = htole32((uint32_t)sectors);
}

// Indices into i_block and then into each indirect block on the way to the data block
static int block_path(const struct ext2_vfs_context* restrict context, uint64_t lbn, uint64_t* restrict indices, unsigned* restrict levels)
{
    if(lbn < EXT2_NDIR_BLOCKS) {
        indices[0] = lbn;
        *levels = 1;
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

    indices[0] = EXT2_IND_BLOCK + depth - 1;

    for(unsigned level = 1; level <= depth; level += 1) {
        span /= addr_per_block;
        indices[level] = lbn / span;
        lbn %= span;
    }

    *levels = depth + 1;

    return 0;
}

static uint32_t block_goal(struct ext2_vfs_context* restrict context, const struct ext2_inode* restrict inode, uint32_t ino, uint32_t table,
    uint64_t index)
{
    uint32_t previous = 0;

    if(index > 0 && table_get(context, inode, table, index - 1, &previous) == 0 && previous) {
        return previous + 1;
    }

    if(table) {
        return table + 1;
    }

    return context->first_data_block + (ino - 1) / context->inodes_per_group * context->blocks_per_group;
}

static int zero_block(struct ext2_vfs_context* restrict context, uint32_t block)
{
    static const uint8_t zeros[1U << EXT2_MIN_BLOCK_LOG_SIZE] = { 0 };

    uint64_t offset = ext2_block_offset(context, block);
    int retval = 0;

    for(size_t done = 0; retval == 0 && done < context->block_size; done += sizeof(zeros)) {
        retval = fcache_dev_write(context->devnode, offset + done, zeros, sizeof(zeros));
    }

    return retval;
}

static int take_block(struct ext2_vfs_context* restrict context, struct ext2_inode* restrict inode, uint32_t ino, uint32_t table,
    uint64_t index, bool indirect, uint32_t* restrict out)
{
    uint32_t block = ext2_block_alloc(context, block_goal(context, inode, ino, table, index));

    if(block == 0) {
        return -ENOSPC;
    }

    int retval = 0;

    if(indirect) {
        retval = zero_block(context, block);
    }

    if(retval == 0) {
        retval = table_set(context, inode, table, index, block);
    }

    if(retval < 0) {
        ext2_block_free(context, block);
        return retval;
    }

    add_blocks(context, inode, 1);

    *out = block;

    return 0;
}

static int block_map(struct ext2_vfs_context* restrict context, struct ext2_inode* restrict inode, uint32_t ino, uint64_t lbn, bool create,
    uint32_t* restrict out)
{
    uint64_t indices[4];
    unsigned levels;
    int retval = block_path(context, lbn, indices, &levels);

    if(retval < 0) {
        return retval;
    }

    uint32_t table = 0;
    uint32_t block = 0;

    for(unsigned level = 0; level < levels; level += 1) {
        retval = table_get(context, inode, table, indices[level], &block);

        if(retval < 0) {
            return retval;
        }

        if(block == 0 && !create) {
            break;
        }

        if(block == 0) {
            retval = take_block(context, inode, ino, table, indices[level], level + 1 < levels, &block);

            if(retval < 0) {
                return retval;
            }
        }
        else if(!data_block_valid(context, block)) {
            return -EIO;
        }

        table = block;
    }

    *out = block;

    return 0;
}

static void release_block(struct ext2_vfs_context* restrict context, struct ext2_inode* restrict inode, uint32_t block, bool indirect)
{
    // A freed indirect block may come back as file data, which
    // its dirty pages in the device cache would then overwrite
    if(indirect) {
        fcache_forget(context->devnode, ext2_block_offset(context, block), context->block_size);
    }

    ext2_block_free(context, block);
    add_blocks(context, inode, -1);
}

static int trim_table(struct ext2_vfs_context* restrict context, struct ext2_inode* restrict inode, uint32_t table, unsigned depth,
    uint64_t keep)
{
    uint32_t entries[64];
    uint64_t span = 1;

    for(unsigned i = 0; i < depth; ++i) {
        span *= context->addr_per_block;
    }

    uint64_t first_slot = keep / span;
    uint64_t offset = ext2_block_offset(context, table);
    int retval = 0;

    for(uint64_t base = first_slot - first_slot % 64; retval == 0 && base < context->addr_per_block; base += 64) {
        size_t count = context->addr_per_block - base;

        if(count > 64) {
            count = 64;
        }

        retval = fcache_dev_read(context->devnode, offset + base * sizeof(uint32_t), entries, count * sizeof(uint32_t));
        bool changed = false;

        for(size_t i = 0; retval == 0 && i < count; ++i) {
            uint64_t slot = base + i;
            uint32_t child = le32toh(entries[i]);

            if(slot < first_slot || child == 0) {
                continue;
            }

            if(!data_block_valid(context, child)) {
                retval = -EIO;
                break;
            }

            uint64_t child_keep = 0;

            if(slot == first_slot) {
                child_keep = keep % span;
            }

            if(depth > 0) {
                retval = trim_table(context, inode, child, depth - 1, child_keep);
            }

            if(retval == 0 && child_keep == 0) {
                release_block(context, inode, child, depth > 0);
                entries[i] = 0;
                changed = true;
            }
        }

        // A table left without entries
        // is freed by the caller; unwritten
        if(retval == 0 && changed && keep > 0) {
            retval = fcache_dev_write(context->devnode, offset + base * sizeof(uint32_t), entries, count * sizeof(uint32_t));
        }
    }

    return retval;
}

int ext2_inode_offset(const struct ext2_vfs_context* restrict context, uint32_t ino, uint64_t* restrict out)
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
    int retval = ext2_inode_offset(context, ino, &offset);

    if(retval < 0) {
        return retval;
    }

    return fcache_dev_read(context->devnode, offset, out, sizeof(struct ext2_inode));
}

int ext2_inode_clear(struct ext2_vfs_context* restrict context, uint32_t ino)
{
    static const uint8_t zeros[EXT2_GOOD_OLD_INODE_SIZE] = { 0 };

    uint64_t offset;
    int retval = ext2_inode_offset(context, ino, &offset);

    for(size_t done = 0; retval == 0 && done < context->inode_size; done += sizeof(zeros)) {
        retval = fcache_dev_write(context->devnode, offset + done, zeros, sizeof(zeros));
    }

    return retval;
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

int ext2_inode_write(struct ext2_vfs_context* restrict context, uint32_t ino, const struct ext2_inode* restrict inode)
{
    uint64_t offset;
    int retval = ext2_inode_offset(context, ino, &offset);

    if(retval < 0) {
        return retval;
    }

    return fcache_dev_write(context->devnode, offset, inode, sizeof(struct ext2_inode));
}

int ext2_inode_set_size(struct ext2_vfs_context* restrict context, struct ext2_inode* restrict inode, uint64_t size)
{
    struct ext2_superblock* super = &context->super;
    bool regular = EXT2_S_IFREG == (le16toh(inode->i_mode) & S_IFMT);

    if(size > UINT32_MAX && !regular) {
        return -EFBIG;
    }

    // Older drivers take i_size as signed
    if(size > INT32_MAX && regular) {
        uint32_t ro_compat = le32toh(super->s_feature_ro_compat);

        if(le32toh(super->s_rev_level) == EXT2_GOOD_OLD_REV) {
            return -EFBIG;
        }

        if(0 == (ro_compat & EXT2_FEATURE_RO_COMPAT_LARGE_FILE)) {
            super->s_feature_ro_compat = htole32(ro_compat | EXT2_FEATURE_RO_COMPAT_LARGE_FILE);

            int retval = ext2_super_commit(context);

            if(retval < 0) {
                return retval;
            }
        }
    }

    inode->i_size = htole32((uint32_t)size);

    if(regular) {
        inode->i_size_high = htole32((uint32_t)(size >> 32));
    }

    return 0;
}

bool ext2_inode_set_owner(const struct ext2_vfs_context* restrict context, struct ext2_inode* restrict inode, uid_t uid, gid_t gid)
{
    uint32_t limit = UINT16_MAX;

    if(ext2_has_id_high(context)) {
        limit = UINT32_MAX;
    }

    if(uid < 0 || gid < 0 || (uint32_t)uid > limit || (uint32_t)gid > limit) {
        return false;
    }

    inode->i_uid = htole16((uint16_t)uid);
    inode->i_gid = htole16((uint16_t)gid);

    if(ext2_has_id_high(context)) {
        inode->osd2.os_linux.l_i_uid_high = htole16((uint16_t)((uint32_t)uid >> 16));
        inode->osd2.os_linux.l_i_gid_high = htole16((uint16_t)((uint32_t)gid >> 16));
    }

    return true;
}

int ext2_truncate_blocks(struct vnode* restrict node, uint64_t size)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    struct ext2_vfs_context* fs_ctx = ctx->fs_ctx;
    struct ext2_inode* inode = &ctx->inode;
    uint64_t keep = (size + fs_ctx->block_size - 1) / fs_ctx->block_size;

    for(uint64_t i = keep; i < EXT2_NDIR_BLOCKS; ++i) {
        uint32_t block = le32toh(inode->i_block[i]);

        if(block && data_block_valid(fs_ctx, block)) {
            release_block(fs_ctx, inode, block, false);
        }

        inode->i_block[i] = 0;
    }

    uint64_t base = EXT2_NDIR_BLOCKS;
    uint64_t span = fs_ctx->addr_per_block;

    for(unsigned depth = 0; depth < 3; depth += 1) {
        uint32_t root = le32toh(inode->i_block[EXT2_IND_BLOCK + depth]);

        if(root && keep < base + span) {
            uint64_t root_keep = 0;

            if(keep > base) {
                root_keep = keep - base;
            }

            if(!data_block_valid(fs_ctx, root)) {
                return -EIO;
            }

            int retval = trim_table(fs_ctx, inode, root, depth, root_keep);

            if(retval < 0) {
                return retval;
            }

            if(root_keep == 0) {
                release_block(fs_ctx, inode, root, true);
                inode->i_block[EXT2_IND_BLOCK + depth] = 0;
            }
        }

        base += span;
        span *= fs_ctx->addr_per_block;
    }

    return 0;
}

int ext2_bmap(struct vnode* restrict node, uint64_t sector, bool create, struct fcache_extent* restrict out)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    struct ext2_vfs_context* fs_ctx = ctx->fs_ctx;
    uint32_t ino = (uint32_t)(node->nodenumber);

    if(create && node->fs->readonly) {
        return -EROFS;
    }

    uint64_t in_block = sector % fs_ctx->sectors_per_block;
    uint32_t blocks_before = ctx->inode.i_blocks;
    uint32_t block;
    int retval = block_map(fs_ctx, &ctx->inode, ino, sector / fs_ctx->sectors_per_block, create, &block);

    // Even a failed mapping may have taken indirect blocks
    if(ctx->inode.i_blocks != blocks_before) {
        int write_retval = ext2_inode_write(fs_ctx, ino, &ctx->inode);

        if(retval == 0) {
            retval = write_retval;
        }
    }

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
