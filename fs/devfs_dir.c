#include <core/errno.h>
#include <core/spinlock.h>
#include <fs/devfs.h>
#include <fs/dirent.h>
#include <fs/vnode.h>
#include <lib/string.h>
#include <lib/strings.h>

static int devfs_dir_lookup(struct vnode* restrict node, const char* restrict name, struct vnode** restrict out)
{
    struct devfs_state* state = node->ctxdata;
    struct devfs_node* it;

    spinlock_acquire(&state->lock);

    for(it = state->head; it; it = it->next) {
        if(0 == strcmp(it->name, name)) {
            vnode_decref(it->node);

            *out = it->node;

            spinlock_release(&state->lock);

            return 0;
        }
    }

    spinlock_release(&state->lock);

    return -ENOENT;
}

static int devfs_dir_iterate(struct vnode* restrict node, struct dirent* restrict out, size_t index)
{
    struct devfs_state* state = node->ctxdata;
    struct devfs_node* it;
    size_t i = 0;

    spinlock_acquire(&state->lock);

    for(it = state->head; it; it = it->next, i++) {
        if(i == index) {
            kstrncpy(out->name, it->name, sizeof(out->name));

            out->nodetype = it->node->nodetype;

            spinlock_release(&state->lock);

            return 0;
        }
    }

    spinlock_release(&state->lock);

    return -ENOENT;
}

const struct vnode_ops devfs_dir_ops = {
    .lookup = &devfs_dir_lookup,
    .iterate = &devfs_dir_iterate,
};
