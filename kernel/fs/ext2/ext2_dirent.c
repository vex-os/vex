#include <vex/blk_device.h>
#include <vex/errno.h>
#include <vex/fcache.h>
#include <vex/kprintf.h>
#include <vex/string.h>
#include <vex/vnode.h>

#include "ext2.h"

static int read_exact(struct vnode* restrict dir, void* restrict buffer, size_t sz, uint64_t offset)
{
    struct ext2_vnode_context* ctx = dir->ctxdata;
    ssize_t done = fcache_read(dir, &ext2_bmap, buffer, sz, offset, ext2_inode_size(&ctx->inode));

    if(done < 0) {
        return done;
    }

    if((size_t)done != sz) {
        return -EIO;
    }

    return 0;
}

int ext2_dirent_read(struct vnode* restrict dir, uint64_t* restrict offset, struct ext2_dirent* restrict out)
{
    struct ext2_vnode_context* ctx = dir->ctxdata;
    struct ext2_vfs_context* fs_ctx = ctx->fs_ctx;
    uint64_t size = ext2_inode_size(&ctx->inode);
    bool has_filetype = le32toh(fs_ctx->super.s_feature_incompat) & EXT2_FEATURE_INCOMPAT_FILETYPE;

    while(*offset < size) {
        struct ext2_dir_entry head;
        int retval = read_exact(dir, &head, sizeof(head), *offset);

        if(retval < 0) {
            return retval;
        }

        size_t rec_len = le16toh(head.rec_len);
        size_t in_block = *offset % fs_ctx->block_size;

        // Records never cross a block boundary
        bool valid = true;
        valid = valid && rec_len >= ext2_dirent_reclen(head.name_len);
        valid = valid && 0 == rec_len % 4;
        valid = valid && rec_len <= fs_ctx->block_size - in_block;

        if(!valid) {
            pr_warning("ext2: %s: inode %u has a broken directory record at %zu", fs_ctx->device->identifier, (uint32_t)(dir->nodenumber),
                (size_t)(*offset));
            return -EIO;
        }

        uint64_t record = *offset;
        uint32_t ino = le32toh(head.inode);

        *offset += rec_len;

        if(ino == 0) {
            continue;
        }

        retval = read_exact(dir, out->name, head.name_len, record + sizeof(head));

        if(retval < 0) {
            return retval;
        }

        out->name[head.name_len] = 0;
        out->offset = record;
        out->ino = ino;

        if(has_filetype) {
            out->file_type = head.file_type;
        }
        else {
            out->file_type = EXT2_FT_UNKNOWN;
        }

        return 0;
    }

    return -ENOENT;
}

int ext2_dirent_find(struct vnode* restrict dir, const char* restrict name, struct ext2_dirent* restrict out)
{
    uint64_t offset = 0;

    for(;;) {
        int retval = ext2_dirent_read(dir, &offset, out);

        if(retval < 0) {
            return retval;
        }

        if(0 == strcmp(out->name, name)) {
            return 0;
        }
    }
}
