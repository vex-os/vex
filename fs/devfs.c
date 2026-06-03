#include <core/errno.h>
#include <core/spinlock.h>
#include <dev/blk_device.h>
#include <dev/blk_event.h>
#include <fs/vfs_mount.h>
#include <fs/vfs_node.h>
#include <fs/vfs_types.h>
#include <init/initcall.h>
#include <lib/string.h>
#include <lib/strings.h>
#include <mm/malloc.h>
#include <mm/slab.h>
#include <stdint.h>

struct devfs_node {
    char name[VFS_NAME_SIZE];
    struct vfs_node* vnode;
    struct devfs_node* next;
};

struct devfs_state {
    spinlock_type lock;
    struct devfs_node* head;
};

struct devfs_blk_ctx {
    const struct blk_device* dev;
    spinlock_type bounce_lock;
    uint8_t* bounce_buffer;
};

static struct vfs* devfs_instance = nullptr;

static ssize_t devfs_blk_read(struct vfs_node* restrict node, void* restrict buffer, size_t sz, off_t offset)
{
    struct devfs_blk_ctx* ctx = node->ctxdata;
    const struct blk_device* dev = ctx->dev;

    size_t sector_size = dev->sector_size;
    uintptr_t lba = (uintptr_t)(offset / sector_size);
    size_t byte_off = (size_t)(offset % sector_size);
    uint8_t* output = buffer;
    size_t remaining = sz;
    size_t chunk, full;
    uintptr_t cur_lba = lba;
    int retval;

    if(sz == 0) {
        return 0;
    }

    if(byte_off == 0 && (sz % sector_size) == 0) {
        retval = blk_device_read(dev, lba, sz / sector_size, buffer);

        if(retval)
            return -retval;
        return (ssize_t)(sz);
    }

    if(byte_off) {
        chunk = sector_size - byte_off;

        if(chunk > remaining) {
            chunk = remaining;
        }

        spinlock_acquire(&ctx->bounce_lock);
        retval = blk_device_read(dev, cur_lba, 1, ctx->bounce_buffer);

        if(retval) {
            spinlock_release(&ctx->bounce_lock);
            return -retval;
        }

        memcpy(output, ctx->bounce_buffer + byte_off, chunk);
        spinlock_release(&ctx->bounce_lock);

        output += chunk;
        remaining -= chunk;
        cur_lba += 1;
    }

    if(remaining >= sector_size) {
        full = remaining / sector_size;
        retval = blk_device_read(dev, cur_lba, full, output);

        if(retval) {
            return -retval;
        }

        output += full * sector_size;
        remaining -= full * sector_size;
        cur_lba += (uintptr_t)(full);
    }

    if(remaining > 0) {
        spinlock_acquire(&ctx->bounce_lock);

        retval = blk_device_read(dev, cur_lba, 1, ctx->bounce_buffer);

        if(retval) {
            spinlock_release(&ctx->bounce_lock);
            return -retval;
        }

        memcpy(output, ctx->bounce_buffer, remaining);

        spinlock_release(&ctx->bounce_lock);
    }

    return (ssize_t)(sz);
}

static ssize_t devfs_blk_write(struct vfs_node* restrict node, const void* restrict buffer, size_t sz, off_t offset)
{
    struct devfs_blk_ctx* ctx = node->ctxdata;
    const struct blk_device* dev = ctx->dev;

    size_t sector_size = dev->sector_size;
    uintptr_t lba = (uintptr_t)(offset / sector_size);
    size_t byte_off = (size_t)(offset % sector_size);
    const uint8_t* in = buffer;
    size_t remaining = sz;
    size_t chunk, full;
    uintptr_t cur_lba = lba;
    int retval;

    if(sz == 0) {
        return 0;
    }

    if(byte_off == 0 && (sz % sector_size) == 0) {
        retval = blk_device_write(dev, lba, sz / sector_size, buffer);

        if(retval)
            return -retval;
        return (ssize_t)(sz);
    }

    if(byte_off) {
        chunk = sector_size - byte_off;

        if(chunk > remaining) {
            chunk = remaining;
        }

        spinlock_acquire(&ctx->bounce_lock);

        retval = blk_device_read(dev, cur_lba, 1, ctx->bounce_buffer);

        if(retval) {
            spinlock_release(&ctx->bounce_lock);
            return -retval;
        }

        memcpy(ctx->bounce_buffer + byte_off, in, chunk);

        retval = blk_device_write(dev, cur_lba, 1, ctx->bounce_buffer);

        spinlock_release(&ctx->bounce_lock);

        if(retval) {
            return -retval;
        }

        in += chunk;
        remaining -= chunk;
        cur_lba += 1;
    }

    if(remaining >= sector_size) {
        full = remaining / sector_size;
        retval = blk_device_write(dev, cur_lba, full, in);

        if(retval) {
            return -retval;
        }

        in += full * sector_size;
        remaining -= full * sector_size;
        cur_lba += (uintptr_t)(full);
    }

    if(remaining > 0) {
        spinlock_acquire(&ctx->bounce_lock);

        retval = blk_device_read(dev, cur_lba, 1, ctx->bounce_buffer);

        if(retval) {
            spinlock_release(&ctx->bounce_lock);
            return -retval;
        }

        memcpy(ctx->bounce_buffer, in, remaining);

        retval = blk_device_write(dev, cur_lba, 1, ctx->bounce_buffer);

        spinlock_release(&ctx->bounce_lock);

        if(retval) {
            return -retval;
        }
    }

    return (ssize_t)(sz);
}

static int devfs_blk_ioctl(struct vfs_node* restrict node, unsigned request, void* restrict arg)
{
    return ENOTTY;
}

static int devfs_blk_stat(struct vfs_node* restrict node, struct vfs_stat* restrict out_stat)
{
    const struct devfs_blk_ctx* ctx = node->ctxdata;
    const struct blk_device* dev = ctx->dev;

    memset(out_stat, 0, sizeof(struct vfs_stat));

    out_stat->type = VFS_NODE_TYPE_BLK_DEVICE;
    out_stat->size = (off_t)(dev->sector_count * dev->sector_size);
    out_stat->block_size = (blksize_t)(dev->sector_size);
    out_stat->blocks = (blkcnt_t)(dev->sector_count);

    return 0;
}

static void devfs_blk_destroy(struct vfs_node* restrict node)
{
    struct devfs_blk_ctx* ctx = node->ctxdata;
    kfree(ctx->bounce_buffer);
    slab_free(ctx);
}

static const struct vfs_node_ops devfs_blk_ops = {
    .destroy = &devfs_blk_destroy,
    .read = &devfs_blk_read,
    .write = &devfs_blk_write,
    .ioctl = &devfs_blk_ioctl,
    .stat = &devfs_blk_stat,
};

static int devfs_dir_lookup(struct vfs_node* restrict dir, const char* restrict name, struct vfs_node** restrict out)
{
    struct devfs_state* state = dir->ctxdata;
    struct devfs_node* it;

    spinlock_acquire(&state->lock);

    for(it = state->head; it; it = it->next) {
        if(0 == strcmp(it->name, name)) {
            vfs_node_inc_refcount(it->vnode);
            out[0] = it->vnode;
            spinlock_release(&state->lock);
            return 0;
        }
    }

    spinlock_release(&state->lock);

    return ENOENT;
}

static int devfs_dir_readdir(struct vfs_node* restrict dir, struct vfs_dirent* restrict out_dirent, off_t index)
{
    struct devfs_state* state = dir->ctxdata;
    struct devfs_node* it;
    off_t i = 0;

    spinlock_acquire(&state->lock);

    for(it = state->head; it; it = it->next, i++) {
        if(i == index) {
            kstrncpy(out_dirent->name, it->name, sizeof(out_dirent->name));
            out_dirent->type = it->vnode->type;
            spinlock_release(&state->lock);
            return 0;
        }
    }

    spinlock_release(&state->lock);

    return ENOENT;
}

static const struct vfs_node_ops devfs_dir_ops = {
    .lookup = &devfs_dir_lookup,
    .read_dir = &devfs_dir_readdir,
};

static int devfs_insert(const char* restrict name, struct vfs_node* restrict vnode)
{
    struct devfs_state* state;
    struct devfs_node* entry;

    if(devfs_instance == nullptr) {
        return ENODEV;
    }

    state = devfs_instance->root->ctxdata;

    entry = slab_alloc(sizeof(struct devfs_node));

    if(entry == nullptr) {
        return ENOMEM;
    }

    memset(entry, 0, sizeof(struct devfs_node));
    kstrncpy(entry->name, name, sizeof(entry->name));
    entry->vnode = vnode;

    spinlock_acquire(&state->lock);
    entry->next = state->head;
    state->head = entry;
    spinlock_release(&state->lock);

    return 0;
}

static void devfs_remove(const char* restrict name)
{
    struct devfs_state* state;
    struct devfs_node** prev;
    struct devfs_node* it;

    if(devfs_instance == nullptr) {
        return;
    }

    state = devfs_instance->root->ctxdata;

    spinlock_acquire(&state->lock);

    for(prev = &state->head; prev[0]; prev = &prev[0]->next) {
        it = prev[0];

        if(0 == strcmp(it->name, name)) {
            prev[0] = it->next;
            vfs_node_dec_refcount(it->vnode);
            slab_free(it);
            break;
        }
    }

    spinlock_release(&state->lock);
}

static void devfs_blk_handler(unsigned event, const struct blk_device* restrict device, void* restrict ctxdata)
{
    struct devfs_blk_ctx* ctx;
    struct vfs_node* vnode;
    int retval;

    if(event == BLK_EVENT_MEDIA_CONNECTED || event == BLK_EVENT_PARTITION_ADDED) {
        ctx = slab_alloc(sizeof(struct devfs_blk_ctx));

        if(ctx == nullptr) {
            return;
        }

        ctx->dev = device;
        ctx->bounce_lock = SPINLOCK_INIT;
        ctx->bounce_buffer = kmalloc(device->sector_size);

        if(ctx->bounce_buffer == nullptr) {
            slab_free(ctx);
            return;
        }

        vnode = vfs_node_alloc(VFS_NODE_TYPE_BLK_DEVICE, &devfs_blk_ops, ctx);

        if(vnode == nullptr) {
            kfree(ctx->bounce_buffer);
            slab_free(ctx);
            return;
        }

        retval = devfs_insert(device->identifier, vnode);

        if(retval) {
            vfs_node_dec_refcount(vnode);
        }

        return;
    }

    if(event == BLK_EVENT_MEDIA_DISCONNECTED || event == BLK_EVENT_PARTITION_REMOVED) {
        devfs_remove(device->identifier);
        return;
    }
}

static int devfs_mount(struct vfs* restrict fs, struct blk_device* restrict device, const struct cmdline* restrict params)
{
    struct devfs_state* state;
    struct vfs_node* root;

    state = slab_alloc(sizeof(struct devfs_state));

    if(state == nullptr) {
        return ENOMEM;
    }

    memset(state, 0, sizeof(struct devfs_state));
    state->lock = SPINLOCK_INIT;

    root = vfs_node_alloc(VFS_NODE_TYPE_DIRECTORY, &devfs_dir_ops, state);

    if(root == nullptr) {
        slab_free(state);
        return ENOMEM;
    }

    fs->root = root;
    fs->ctxdata = state;

    devfs_instance = fs;

    return 0;
}

static int devfs_unmount(struct vfs* restrict fs)
{
    struct devfs_state* state = fs->ctxdata;
    struct devfs_node* it;
    struct devfs_node* next;

    spinlock_acquire(&state->lock);

    for(it = state->head; it; it = next) {
        next = it->next;
        vfs_node_dec_refcount(it->vnode);
        slab_free(it);
    }

    spinlock_release(&state->lock);

    vfs_node_dec_refcount(fs->root);
    slab_free(state);

    devfs_instance = nullptr;

    return 0;
}

static const struct vfs_ops devfs_ops = {
    .mount = &devfs_mount,
    .unmount = &devfs_unmount,
};

static struct vfs_driver devfs_driver = {
    .identifier = "devfs",
    .ops = &devfs_ops,
};

define_initcall(devfs_init, INITCALL_STAGE_BSP_VFS, INITCALL_ORDER_EARLY)
{
    vfs_driver_register(&devfs_driver);
    blk_event_register_handler(devfs_blk_handler, nullptr);
    vfs_mount("devfs", nullptr, nullptr, nullptr);
}
