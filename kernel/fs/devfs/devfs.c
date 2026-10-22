#include <vex/block_event.h>
#include <vex/block_interface.h>
#include <vex/char_event.h>
#include <vex/char_interface.h>
#include <vex/errno.h>
#include <vex/initcall.h>
#include <vex/mount.h>
#include <vex/slab.h>
#include <vex/spinlock.h>
#include <vex/string.h>
#include <vex/strings.h>
#include <vex/vcache.h>
#include <vex/vnode.h>

#include "devfs_internal.h"

static_assert(sizeof(devfs_node_t) <= SLAB_MAX_SIZE);
static_assert(sizeof(devfs_state_t) <= SLAB_MAX_SIZE);

static int devfs_unmount(vfs_t* restrict fs)
{
    devfs_state_t* state = fs->ctxdata;

    block_event_unsubscribe(&devfs_block_event_handler, fs);
    char_event_unsubscribe(&devfs_char_event_handler, fs);

    spinlock_acquire(&state->lock);

    devfs_node_t* head = state->head;
    state->head = nullptr;

    spinlock_release(&state->lock);

    for(devfs_node_t* it = head; it;) {
        devfs_node_t* next = it->next;
        vcache_remove(it->node);
        vnode_decref(it->node);
        slab_free(it);
        it = next;
    }

    vnode_decref(fs->root);

    slab_free(state);

    return 0;
}

static int devfs_mount(vfs_t* restrict fs, const block_interface_t* restrict interface, const cmdline_t* restrict params)
{
    devfs_state_t* state = slab_alloc(sizeof(devfs_state_t));

    if(state == nullptr) {
        return -ENOMEM;
    }

    memset(state, 0, sizeof(devfs_state_t));
    state->lock = SPINLOCK_INIT;

    vnode_t* root = vnode_alloc(VNODE_DIRECTORY, &devfs_dir_ops, state);

    if(root == nullptr) {
        slab_free(state);
        return -ENOMEM;
    }

    root->fs = fs;

    fs->root = root;
    fs->ctxdata = state;

    int retval = block_event_subscribe(&devfs_block_event_handler, fs);

    if(retval == 0) {
        retval = char_event_subscribe(&devfs_char_event_handler, fs);
    }

    if(retval < 0) {
        devfs_unmount(fs);
        return retval;
    }

    return 0;
}

static int devfs_statvfs(vfs_t* restrict fs, statvfs_t* restrict out)
{
    devfs_state_t* state = fs->ctxdata;

    memset(out, 0, sizeof(statvfs_t));

    out->f_files = state->num_devices;
    out->f_namemax = NAME_MAX;

    return 0;
}

static const vfs_ops_t devfs_ops = {
    .mount = &devfs_mount,
    .statvfs = &devfs_statvfs,
    .unmount = &devfs_unmount,
};

static vfs_driver_t devfs_driver = {
    .name = "devfs",
    .ops = &devfs_ops,
};

int devfs_insert(vfs_t* restrict fs, const char* restrict name, vnode_t* restrict node)
{
    devfs_state_t* state = fs->root->ctxdata;
    devfs_node_t* entry = slab_alloc(sizeof(devfs_node_t));

    if(entry == nullptr) {
        return -ENOMEM;
    }

    memset(entry, 0, sizeof(devfs_node_t));
    kstrncpy(entry->name, name, sizeof(entry->name));

    node->fs = fs;
    node->ino = (ino_t)(uintptr_t)node;

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

void devfs_remove(vfs_t* restrict fs, const char* restrict name)
{
    devfs_state_t* state = fs->root->ctxdata;

    devfs_node_t* removed = nullptr;

    spinlock_acquire(&state->lock);

    for(devfs_node_t** prev = &state->head; *prev; prev = &((**prev).next)) {
        devfs_node_t* it = *prev;

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

vnode_t* devfs_find(vfs_t* restrict fs, const char* restrict name)
{
    devfs_state_t* state = fs->root->ctxdata;
    vnode_t* found = nullptr;

    spinlock_acquire(&state->lock);

    for(devfs_node_t* it = state->head; it; it = it->next) {
        if(0 == strcmp(it->name, name)) {
            found = it->node;
            vnode_incref(found);
            break;
        }
    }

    spinlock_release(&state->lock);

    return found;
}

static void init_devfs(void)
{
    vfs_driver_add(&devfs_driver);
}

initcall_bsp_vfs(init_devfs, INIT_EARLY);
