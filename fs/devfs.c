#include <core/errno.h>
#include <core/spinlock.h>
#include <dev/blk_device.h>
#include <dev/blk_event.h>
#include <dev/chr_device.h>
#include <dev/chr_event.h>
#include <fs/devfs.h>
#include <fs/mount.h>
#include <fs/vnode.h>
#include <init/initcall.h>
#include <lib/string.h>
#include <lib/strings.h>
#include <mm/slab.h>

struct vfs* devfs_instance = nullptr;

int devfs_insert(const char* restrict name, struct vnode* restrict node)
{
    struct devfs_node* entry;
    struct devfs_state* state;

    if(devfs_instance == nullptr) {
        return -ENODEV;
    }

    state = devfs_instance->root->ctxdata;

    entry = slab_alloc(sizeof(struct devfs_node));

    if(entry == nullptr) {
        return -ENOMEM;
    }

    memset(entry, 0, sizeof(struct devfs_node));
    kstrncpy(entry->name, name, sizeof(entry->name));

    entry->node = node;

    spinlock_acquire(&state->lock);

    entry->next = state->head;
    state->head = entry;

    spinlock_release(&state->lock);

    return 0;
}

void devfs_remove(const char* restrict name)
{
    struct devfs_node* it;
    struct devfs_node** prev;
    struct devfs_state* state;

    if(devfs_instance == nullptr) {
        return;
    }

    state = devfs_instance->root->ctxdata;

    spinlock_acquire(&state->lock);

    for(prev = &state->head; *prev; prev = &(prev[0])->next) {
        it = *prev;

        if(0 == strcmp(it->name, name)) {
            *prev = it->next;

            vnode_decref(it->node);
            slab_free(it);

            break;
        }
    }

    spinlock_release(&state->lock);
}

static int devfs_mount(struct vfs* restrict fs, const struct blk_device* restrict device, const struct cmdline* restrict params)
{
    const struct blk_device* blk;
    const struct chr_device* chr;
    struct devfs_state* state;
    struct vnode* root;

    state = slab_alloc(sizeof(struct devfs_state));

    if(state == nullptr) {
        return -ENOMEM;
    }

    memset(state, 0, sizeof(struct devfs_state));
    state->lock = SPINLOCK_INIT;

    root = vnode_alloc(VNODE_DIRECTORY, &devfs_dir_ops, state);

    if(root == nullptr) {
        slab_free(state);
        return -ENOMEM;
    }

    fs->root = root;
    fs->ctxdata = state;

    devfs_instance = fs;

    for(blk = blk_devices_head; blk; blk = blk->priv.next) {
        if(BLK_FLAG_PRESENT == (blk->flags & (BLK_FLAG_PRESENT | BLK_FLAG_HIDDEN))) {
            if(blk->flags & BLK_FLAG_PARTITION) {
                devfs_blk_event_handler(BLK_EVENT_PARTITION_ADDED, blk, nullptr);
            }
            else {
                devfs_blk_event_handler(BLK_EVENT_MEDIA_CONNECTED, blk, nullptr);
            }
        }
    }

    for(chr = chr_devices_head; chr; chr = chr->priv.next) {
        devfs_chr_event_handler(CHR_EVENT_DEVICE_ADDED, chr, nullptr);
    }

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

        vnode_decref(it->node);

        slab_free(it);
    }

    spinlock_release(&state->lock);

    vnode_decref(fs->root);

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
    blk_event_register_handler(&devfs_blk_event_handler, nullptr);
    chr_event_register_handler(&devfs_chr_event_handler, nullptr);
}
