#include <vex/blk_device.h>
#include <vex/errno.h>
#include <vex/fcache.h>
#include <vex/kprintf.h>
#include <vex/string.h>

#include "ext2.h"

void ext2_xattr_release(struct ext2_vfs_context* restrict context, uint32_t block)
{
    const char* name = context->device->identifier;

    if(block < context->first_data_block || block >= le32toh(context->super.s_blocks_count)) {
        pr_warning("ext2: %s: xattr block %u is outside the filesystem", name, block);
        return;
    }

    struct ext2_xattr_header header;
    uint64_t offset = ext2_block_offset(context, block);
    int retval = fcache_dev_read(context->devnode, offset, &header, sizeof(header));

    if(retval == 0 && le32toh(header.h_magic) != EXT2_XATTR_MAGIC) {
        retval = -EIO;
    }

    if(retval < 0) {
        pr_warning("ext2: %s: failed to read xattr block %u: %s", name, block, strerror(retval));
        return;
    }

    uint32_t refcount = le32toh(header.h_refcount);

    if(refcount > 1) {
        header.h_refcount = htole32(refcount - 1);

        if(fcache_dev_write(context->devnode, offset, &header, sizeof(header)) < 0) {
            pr_warning("ext2: %s: failed to drop a reference to xattr block %u", name, block);
        }

        return;
    }

    fcache_forget(context->devnode, offset, context->block_size);
    ext2_block_free(context, block);
}
