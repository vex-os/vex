#include <string.h>
#include <strings.h>
#include <sys/errno.h>
#include <sys/spinlock.h>
#include <sys/stat.h>
#include <sys/vfs_dirent.h>
#include <sys/vfs_vnode.h>

#include "devfs.h"

static int devfs_dir_lookup(struct vnode* restrict node, const char* restrict name, ino_t* restrict out)
{
    struct devfs_state* state = node->ctxdata;

    spinlock_acquire(&state->lock);

    for(struct devfs_node* it = state->head; it; it = it->next) {
        if(0 == strcmp(it->name, name)) {
            *out = (ino_t)(uintptr_t)it->node;
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
    size_t i = 0;

    spinlock_acquire(&state->lock);

    for(struct devfs_node* it = state->head; it; it = it->next, i++) {
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

static int devfs_dir_stat(struct vnode* restrict node, struct __k_stat* restrict out)
{
    memset(out, 0, sizeof(struct __k_stat));

    out->st_mode = S_IFDIR | S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;

    return 0;
}

const struct vnode_ops devfs_dir_ops = {
    .lookup = &devfs_dir_lookup,
    .iterate = &devfs_dir_iterate,
    .stat = &devfs_dir_stat,
};
