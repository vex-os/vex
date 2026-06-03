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
#include <mm/slab.h>

struct devfs_node {
    char name[VFS_NAME_SIZE];
    struct vfs_node* vnode;
    struct devfs_node* next;
};

struct devfs_state {
    spinlock_type lock;
    struct devfs_node* head;
};

static struct vfs* devfs_instance = nullptr;

static ssize_t devfs_blk_read(struct vfs_node* restrict node, void* restrict buffer, size_t sz, off_t offset)
{
    const struct blk_device* dev = node->ctxdata;
    uintptr_t lba = (uintptr_t)(offset / dev->sector_size);
    size_t count = (sz + dev->sector_size - 1) / dev->sector_size;
    int retval = blk_device_read(dev, lba, count, buffer);

    if(retval)
        return -retval;
    return (ssize_t)(sz);
}

static ssize_t devfs_blk_write(struct vfs_node* restrict node, const void* restrict buffer, size_t sz, off_t offset)
{
    const struct blk_device* dev = node->ctxdata;
    uintptr_t lba = (uintptr_t)(offset / dev->sector_size);
    size_t count = (sz + dev->sector_size - 1) / dev->sector_size;
    int retval = blk_device_write(dev, lba, count, buffer);

    if(retval)
        return -retval;
    return (ssize_t)(sz);
}

static int devfs_blk_ioctl(struct vfs_node* restrict node, unsigned request, void* restrict arg)
{
    return ENOTTY;
}

static int devfs_blk_stat(struct vfs_node* restrict node, struct vfs_stat* restrict out_stat)
{
    const struct blk_device* dev = node->ctxdata;

    memset(out_stat, 0, sizeof(struct vfs_stat));

    out_stat->type = VFS_NODE_TYPE_BLK_DEVICE;
    out_stat->size = (off_t)(dev->sector_count * dev->sector_size);
    out_stat->block_size = (blksize_t)(dev->sector_size);
    out_stat->blocks = (blkcnt_t)(dev->sector_count);

    return 0;
}

static const struct vfs_node_ops devfs_blk_ops = {
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
    struct vfs_node* vnode;

    if(event == BLK_EVENT_MEDIA_CONNECTED || event == BLK_EVENT_PARTITION_ADDED) {
        vnode = vfs_node_alloc(VFS_NODE_TYPE_BLK_DEVICE, &devfs_blk_ops, (void*)(device));

        if(vnode == nullptr) {
            return;
        }

        if(devfs_insert(device->identifier, vnode)) {
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
    vfs_mount("devfs", nullptr, nullptr, nullptr); // TODO: rootfs instead
}
