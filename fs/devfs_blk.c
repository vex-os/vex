#include <core/errno.h>
#include <core/spinlock.h>
#include <dev/blk_device.h>
#include <dev/blk_event.h>
#include <fs/devfs_internal.h>
#include <fs/vfs_node.h>
#include <fs/vfs_types.h>
#include <lib/string.h>
#include <mm/malloc.h>
#include <mm/slab.h>

struct devfs_blk_context {
    const struct blk_device* device;
    spinlock_type bounce_lock;
    uint8_t* bounce_buffer;
};

static ssize_t devfs_blk_read(struct vfs_node* restrict node, void* restrict buffer, size_t sz, off_t offset)
{
    struct devfs_blk_context* context = node->ctxdata;
    const struct blk_device* device = context->device;

    size_t sector_size = device->sector_size;
    uintptr_t lba = (uintptr_t)(offset / sector_size);
    size_t byte_offset = (size_t)(offset % sector_size);

    uint8_t* buffer_ptr = buffer;

    uintptr_t current = lba;
    size_t remaining = sz;
    size_t chunk;
    size_t full;
    int retval;

    if(sz == 0) {
        return 0;
    }

    if(byte_offset == 0 && (sz % sector_size) == 0) {
        retval = blk_device_read(device, lba, sz / sector_size, buffer);

        if(retval < 0)
            return retval;
        return (ssize_t)(sz);
    }

    if(byte_offset) {
        chunk = sector_size - byte_offset;

        if(chunk > remaining) {
            chunk = remaining;
        }

        spinlock_acquire(&context->bounce_lock);
        retval = blk_device_read(device, current, 1, context->bounce_buffer);

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
        full = remaining / sector_size;
        retval = blk_device_read(device, current, full, buffer_ptr);

        if(retval < 0) {
            return retval;
        }

        buffer_ptr += full * sector_size;
        remaining -= full * sector_size;
        current += (uintptr_t)(full);
    }

    if(remaining > 0) {
        spinlock_acquire(&context->bounce_lock);

        retval = blk_device_read(device, current, 1, context->bounce_buffer);

        if(retval < 0) {
            spinlock_release(&context->bounce_lock);
            return retval;
        }

        memcpy(buffer_ptr, context->bounce_buffer, remaining);

        spinlock_release(&context->bounce_lock);
    }

    return (ssize_t)(sz);
}

static ssize_t devfs_blk_write(struct vfs_node* restrict node, const void* restrict buffer, size_t sz, off_t offset)
{
    struct devfs_blk_context* context = node->ctxdata;
    const struct blk_device* device = context->device;

    size_t sector_size = device->sector_size;
    uintptr_t lba = (uintptr_t)(offset / sector_size);
    size_t byte_offset = (size_t)(offset % sector_size);

    const uint8_t* buffer_ptr = buffer;

    uintptr_t current = lba;
    size_t remaining = sz;
    size_t chunk;
    size_t full;
    int retval;

    if(sz == 0) {
        return 0;
    }

    if(byte_offset == 0 && (sz % sector_size) == 0) {
        retval = blk_device_write(device, lba, sz / sector_size, buffer);

        if(retval < 0)
            return retval;
        return (ssize_t)(sz);
    }

    if(byte_offset) {
        chunk = sector_size - byte_offset;

        if(chunk > remaining) {
            chunk = remaining;
        }

        spinlock_acquire(&context->bounce_lock);

        retval = blk_device_read(device, current, 1, context->bounce_buffer);

        if(retval < 0) {
            spinlock_release(&context->bounce_lock);
            return retval;
        }

        memcpy(context->bounce_buffer + byte_offset, buffer_ptr, chunk);

        retval = blk_device_write(device, current, 1, context->bounce_buffer);

        spinlock_release(&context->bounce_lock);

        if(retval < 0) {
            return retval;
        }

        buffer_ptr += chunk;
        remaining -= chunk;
        current += 1;
    }

    if(remaining >= sector_size) {
        full = remaining / sector_size;
        retval = blk_device_write(device, current, full, buffer_ptr);

        if(retval < 0) {
            return retval;
        }

        buffer_ptr += full * sector_size;
        remaining -= full * sector_size;
        current += (uintptr_t)(full);
    }

    if(remaining > 0) {
        spinlock_acquire(&context->bounce_lock);

        retval = blk_device_read(device, current, 1, context->bounce_buffer);

        if(retval < 0) {
            spinlock_release(&context->bounce_lock);
            return retval;
        }

        memcpy(context->bounce_buffer, buffer_ptr, remaining);

        retval = blk_device_write(device, current, 1, context->bounce_buffer);

        spinlock_release(&context->bounce_lock);

        if(retval < 0) {
            return retval;
        }
    }

    return (ssize_t)(sz);
}

static int devfs_blk_ioctl(struct vfs_node* restrict node, unsigned request, void* restrict arg)
{
    return -ENOTTY;
}

static int devfs_blk_stat(struct vfs_node* restrict node, struct vfs_stat* restrict out_stat)
{
    const struct devfs_blk_context* context = node->ctxdata;
    const struct blk_device* device = context->device;

    memset(out_stat, 0, sizeof(struct vfs_stat));

    out_stat->type = VFS_NODE_TYPE_BLK_DEVICE;
    out_stat->size = (off_t)(device->sector_count * device->sector_size);
    out_stat->block_size = (blksize_t)(device->sector_size);
    out_stat->blocks = (blkcnt_t)(device->sector_count);

    return 0;
}

static void devfs_blk_destroy(struct vfs_node* restrict node)
{
    struct devfs_blk_context* context = node->ctxdata;
    kfree(context->bounce_buffer);
    slab_free(context);
}

static const struct vfs_node_ops devfs_blk_ops = {
    .destroy = &devfs_blk_destroy,
    .read = &devfs_blk_read,
    .write = &devfs_blk_write,
    .ioctl = &devfs_blk_ioctl,
    .stat = &devfs_blk_stat,
};

void devfs_blk_event_handler(unsigned event, const struct blk_device* restrict device, void* restrict ctxdata)
{
    struct devfs_blk_context* context;
    struct vfs_node* vnode;
    int retval;

    if(device->flags & BLK_FLAG_HIDDEN) {
        return;
    }

    if(event == BLK_EVENT_MEDIA_CONNECTED || event == BLK_EVENT_PARTITION_ADDED) {
        context = slab_alloc(sizeof(struct devfs_blk_context));

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

        vnode = vfs_node_alloc(VFS_NODE_TYPE_BLK_DEVICE, &devfs_blk_ops, context);

        if(vnode == nullptr) {
            kfree(context->bounce_buffer);
            slab_free(context);
            return;
        }

        retval = devfs_insert(device->identifier, vnode);

        if(retval < 0) {
            vfs_node_dec_refcount(vnode);
        }

        return;
    }

    if(event == BLK_EVENT_MEDIA_DISCONNECTED || event == BLK_EVENT_PARTITION_REMOVED) {
        devfs_remove(device->identifier);
        return;
    }
}
