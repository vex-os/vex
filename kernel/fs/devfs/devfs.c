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

static_assert(sizeof(struct devfs_node) <= SLAB_MAX_SIZE);
static_assert(sizeof(struct devfs_state) <= SLAB_MAX_SIZE);

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

    int retval = vcache_insert_pinned(node);

    if(retval < 0) {
        slab_free(entry);
        return retval;
    }

    entry->node = node;

    spinlock_acquire(&state->lock);

    entry->next = state->head;
    state->head = entry;
    state->num_devices += 1;

    spinlock_release(&state->lock);

    return 0;
}

void devfs_remove(struct vfs* restrict fs, const char* restrict name)
{
    struct devfs_state* state = fs->root->ctxdata;

    struct devfs_node* removed = nullptr;

    spinlock_acquire(&state->lock);

    for(struct devfs_node** prev = &state->head; *prev; prev = &((**prev).next)) {
        struct devfs_node* it = *prev;

        if(0 == strcmp(it->name, name)) {
            *prev = it->next;
            removed = it;
            state->num_devices -= 1;
            break;
        }
    }

    spinlock_release(&state->lock);

    if(removed) {
        vcache_remove(removed->node);
        vnode_decref(removed->node);
        slab_free(removed);
    }
}

static int devfs_unmount(struct vfs* restrict fs)
{
    struct devfs_state* state = fs->ctxdata;

    blk_event_unsubscribe(&devfs_blk_event_handler, fs);
    chr_event_unsubscribe(&devfs_chr_event_handler, fs);

    spinlock_acquire(&state->lock);

    struct devfs_node* head = state->head;
    state->head = nullptr;

    spinlock_release(&state->lock);

    for(struct devfs_node* it = head; it;) {
        struct devfs_node* next = it->next;
        vcache_remove(it->node);
        vnode_decref(it->node);
        slab_free(it);
        it = next;
    }

    vnode_decref(fs->root);

    slab_free(state);

    return 0;
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

    int retval = blk_event_subscribe(&devfs_blk_event_handler, fs);

    if(retval == 0) {
        retval = chr_event_subscribe(&devfs_chr_event_handler, fs);
    }

    if(retval < 0) {
        devfs_unmount(fs);
        return retval;
    }

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
