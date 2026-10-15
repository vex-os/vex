#include <vex/blk_device.h>
#include <vex/blk_event.h>
#include <vex/chr_device.h>
#include <vex/chr_event.h>
#include <vex/errno.h>
#include <vex/initcall.h>
#include <vex/mount.h>
#include <vex/slab.h>
#include <vex/spinlock.h>
#include <vex/string.h>
#include <vex/strings.h>
#include <vex/vcache.h>
#include <vex/vnode.h>

#include "devfs.h"

static_assert(sizeof(struct devfs_node) <= (PAGE_SIZE / 2));
static_assert(sizeof(struct devfs_state) <= (PAGE_SIZE / 2));

int devfs_insert(struct vfs* restrict fs, const char* restrict name, struct vnode* restrict node)
{
    struct devfs_state* state = fs->root->ctxdata;
    struct devfs_node* entry = slab_alloc(sizeof(struct devfs_node));

    if(entry == nullptr) {
        return -ENOMEM;
    }

    memset(entry, 0, sizeof(struct devfs_node));
    kstrncpy(entry->name, name, sizeof(entry->name));

    node->fs = fs;
    node->nodenumber = (ino_t)(uintptr_t)node;

    if(!vcache_get_or_insert(node)) {
        slab_free(entry);
        return -ENOMEM;
    }

    entry->node = node;

    spinlock_acquire(&state->lock);

    entry->next = state->head;
    state->head = entry;

    spinlock_release(&state->lock);

    state->num_devices += 1;

    return 0;
}

void devfs_remove(struct vfs* restrict fs, const char* restrict name)
{
    struct devfs_state* state = fs->root->ctxdata;

    spinlock_acquire(&state->lock);

    for(struct devfs_node** prev = &state->head; *prev; prev = &((**prev).next)) {
        struct devfs_node* it = *prev;

        if(0 == strcmp(it->name, name)) {
            *prev = it->next;

            vnode_decref(it->node);
            slab_free(it);

            break;
        }
    }

    state->num_devices -= 1;

    spinlock_release(&state->lock);
}

static int devfs_mount(struct vfs* restrict fs, const struct blk_device* restrict device, const struct cmdline* restrict params)
{
    struct devfs_state* state = slab_alloc(sizeof(struct devfs_state));

    if(state == nullptr) {
        return -ENOMEM;
    }

    memset(state, 0, sizeof(struct devfs_state));
    state->lock = SPINLOCK_INIT;

    struct vnode* root = vnode_alloc(VNODE_DIRECTORY, &devfs_dir_ops, state);

    if(root == nullptr) {
        slab_free(state);
        return -ENOMEM;
    }

    root->fs = fs;

    fs->root = root;
    fs->ctxdata = state;

    for(const struct blk_device* blk = blk_devices_head; blk; blk = blk->priv.next) {
        if(BLK_FLAG_PRESENT == (blk->flags & (BLK_FLAG_PRESENT | BLK_FLAG_HIDDEN))) {
            if(blk->flags & BLK_FLAG_PARTITION) {
                devfs_blk_event_handler(BLK_EVENT_PARTITION_ADDED, blk, fs);
            }
            else {
                devfs_blk_event_handler(BLK_EVENT_MEDIA_CONNECTED, blk, fs);
            }
        }
    }

    for(const struct chr_device* chr = chr_devices_head; chr; chr = chr->priv.next) {
        devfs_chr_event_handler(CHR_EVENT_DEVICE_ADDED, chr, fs);
    }

    blk_event_subscribe(&devfs_blk_event_handler, fs);
    chr_event_subscribe(&devfs_chr_event_handler, fs);

    return 0;
}

static int devfs_statvfs(struct vfs* restrict fs, struct k_statvfs* restrict out)
{
    struct devfs_state* state = fs->ctxdata;

    memset(out, 0, sizeof(struct k_statvfs));

    out->f_files = state->num_devices;
    out->f_fsid = (uint32_t)(uintptr_t)(fs);
    out->f_namemax = NAME_MAX;

    return 0;
}

static int devfs_unmount(struct vfs* restrict fs)
{
    struct devfs_state* state = fs->ctxdata;

    spinlock_acquire(&state->lock);

    for(struct devfs_node* it = state->head; it;) {
        struct devfs_node* next = it->next;
        slab_free(it);
        it = next;
    }

    spinlock_release(&state->lock);

    vnode_decref(fs->root);

    slab_free(state);

    blk_event_unsubscribe(&devfs_blk_event_handler, fs);
    chr_event_unsubscribe(&devfs_chr_event_handler, fs);

    return 0;
}

static const struct vfs_ops devfs_ops = {
    .mount = &devfs_mount,
    .statvfs = &devfs_statvfs,
    .unmount = &devfs_unmount,
};

static struct vfs_driver devfs_driver = {
    .identifier = "devfs",
    .ops = &devfs_ops,
};

static void init_devfs(void)
{
    add_vfs_driver(&devfs_driver);
}

initcall_bsp_vfs(init_devfs, INIT_EARLY);
