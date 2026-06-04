#include <core/errno.h>
#include <core/spinlock.h>
#include <fs/devfs_internal.h>
#include <fs/vfs_node.h>
#include <fs/vfs_types.h>
#include <lib/string.h>
#include <lib/strings.h>

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

    return -ENOENT;
}

static int devfs_dir_read_dir(struct vfs_node* restrict dir, struct vfs_dirent* restrict out_dirent, size_t index)
{
    struct devfs_state* state = dir->ctxdata;
    struct devfs_node* it;
    size_t i = 0;

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

    return -ENOENT;
}

const struct vfs_node_ops devfs_dir_ops = {
    .lookup = &devfs_dir_lookup,
    .read_dir = &devfs_dir_read_dir,
};
