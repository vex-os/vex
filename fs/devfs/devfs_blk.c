#include <vex/blk_device.h>
#include <vex/blk_event.h>
#include <vex/errno.h>
#include <vex/kmalloc.h>
#include <vex/slab.h>
#include <vex/spinlock.h>
#include <vex/stat.h>
#include <vex/string.h>
#include <vex/vnode.h>

#include "devfs.h"

struct devfs_blk_context {
    const struct blk_device* device;
    spinlock_type bounce_lock;
    uint8_t* bounce_buffer;
};

static_assert(sizeof(struct devfs_blk_context) <= (PAGE_SIZE / 2));

static void devfs_blk_release(struct vnode* restrict node)
{
    struct devfs_blk_context* context = node->ctxdata;
    kfree(context->bounce_buffer);
    slab_free(context);
}

static ssize_t devfs_blk_read(struct vnode* restrict node, void* restrict buffer, size_t sz, off_t offset)
{
    struct devfs_blk_context* context = node->ctxdata;
    const struct blk_device* device = context->device;

    uint8_t* buffer_ptr = buffer;

    size_t sector_size = device->sector_size;
    size_t byte_offset = offset % sector_size;
    uintptr_t lba = offset / sector_size;

    size_t remaining = sz;
    uintptr_t current = lba;

    if(sz == 0) {
        return 0;
    }

    if(byte_offset == 0 && (sz % sector_size) == 0) {
        int retval = blk_read(device, lba, sz / sector_size, buffer);

        if(retval < 0)
            return retval;
        return sz;
    }

    if(byte_offset) {
        size_t chunk = sector_size - byte_offset;

        if(chunk > remaining) {
            chunk = remaining;
        }

        spinlock_acquire(&context->bounce_lock);
        int retval = blk_read(device, current, 1, context->bounce_buffer);

        if(retval < 0) {
            spinlock_release(&context->bounce_lock);
            return retval;
        }

        memcpy(buffer_ptr, context->bounce_buffer + byte_offset, chunk);
        spinlock_release(&context->bounce_lock);

        buffer_ptr += chunk;
        remaining -= chunk;
        current += 1;
    }

    if(remaining >= sector_size) {
        size_t full = remaining / sector_size;
        int retval = blk_read(device, current, full, buffer_ptr);

        if(retval < 0) {
            return retval;
        }

        buffer_ptr += full * sector_size;
        remaining -= full * sector_size;
        current += full;
    }

    if(remaining > 0) {
        spinlock_acquire(&context->bounce_lock);

        int retval = blk_read(device, current, 1, context->bounce_buffer);

        if(retval < 0) {
            spinlock_release(&context->bounce_lock);
            return retval;
        }

        memcpy(buffer_ptr, context->bounce_buffer, remaining);

        spinlock_release(&context->bounce_lock);
    }

    return sz;
}

static ssize_t devfs_blk_write(struct vnode* restrict node, const void* restrict buffer, size_t sz, off_t offset)
{
    struct devfs_blk_context* context = node->ctxdata;
    const struct blk_device* device = context->device;

    const uint8_t* buffer_ptr = buffer;

    size_t sector_size = device->sector_size;
    size_t byte_offset = offset % sector_size;
    uintptr_t lba = offset / sector_size;

    size_t remaining = sz;
    uintptr_t current = lba;

    if(sz == 0) {
        return 0;
    }

    if(byte_offset == 0 && (sz % sector_size) == 0) {
        int retval = blk_write(device, lba, sz / sector_size, buffer);

        if(retval < 0)
            return retval;
        return sz;
    }

    if(byte_offset) {
        size_t chunk = sector_size - byte_offset;

        if(chunk > remaining) {
            chunk = remaining;
        }

        spinlock_acquire(&context->bounce_lock);

        int retval = blk_read(device, current, 1, context->bounce_buffer);

        if(retval < 0) {
            spinlock_release(&context->bounce_lock);
            return retval;
        }

        memcpy(context->bounce_buffer + byte_offset, buffer_ptr, chunk);

        retval = blk_write(device, current, 1, context->bounce_buffer);

        spinlock_release(&context->bounce_lock);

        if(retval < 0) {
            return retval;
        }

        buffer_ptr += chunk;
        remaining -= chunk;
        current += 1;
    }

    if(remaining >= sector_size) {
        size_t full = remaining / sector_size;
        int retval = blk_write(device, current, full, buffer_ptr);

        if(retval < 0) {
            return retval;
        }

        buffer_ptr += full * sector_size;
        remaining -= full * sector_size;
        current += full;
    }

    if(remaining > 0) {
        spinlock_acquire(&context->bounce_lock);

        int retval = blk_read(device, current, 1, context->bounce_buffer);

        if(retval < 0) {
            spinlock_release(&context->bounce_lock);
            return retval;
        }

        memcpy(context->bounce_buffer, buffer_ptr, remaining);

        retval = blk_write(device, current, 1, context->bounce_buffer);

        spinlock_release(&context->bounce_lock);

        if(retval < 0) {
            return retval;
        }
    }

    return sz;
}

static int devfs_blk_stat(struct vnode* restrict node, struct k_stat* restrict out)
{
    const struct devfs_blk_context* context = node->ctxdata;
    const struct blk_device* device = context->device;

    memset(out, 0, sizeof(struct k_stat));

    out->st_mode = S_IFBLK | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;

    out->st_size = device->sector_count * device->sector_size;
    out->st_blksize = device->sector_size;
    out->st_blocks = device->sector_count;

    return 0;
}

static const struct vnode_ops devfs_blk_ops = {
    .release = &devfs_blk_release,
    .read = &devfs_blk_read,
    .write = &devfs_blk_write,
    .stat = &devfs_blk_stat,
};

void devfs_blk_event_handler(unsigned event, const struct blk_device* restrict device, void* restrict ctxdata)
{
    if(device->flags & BLK_FLAG_HIDDEN) {
        return;
    }

    if(event == BLK_EVENT_MEDIA_CONNECTED || event == BLK_EVENT_PARTITION_ADDED) {
        struct devfs_blk_context* context = slab_alloc(sizeof(struct devfs_blk_context));

        if(context == nullptr) {
            return;
        }

        context->device = device;
        context->bounce_lock = SPINLOCK_INIT;
        context->bounce_buffer = kmalloc(device->sector_size);

        if(context->bounce_buffer == nullptr) {
            slab_free(context);
            return;
        }

        struct vnode* vnode = vnode_alloc(VNODE_BLOCK_DEV, &devfs_blk_ops, context);

        if(vnode == nullptr) {
            kfree(context->bounce_buffer);
            slab_free(context);
            return;
        }

        int retval = devfs_insert(ctxdata, device->identifier, vnode);

        if(retval < 0) {
            vnode_decref(vnode);
        }

        return;
    }

    if(event == BLK_EVENT_MEDIA_DISCONNECTED || event == BLK_EVENT_PARTITION_REMOVED) {
        devfs_remove(ctxdata, device->identifier);
        return;
    }
}
