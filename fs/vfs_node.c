#include <core/atomic.h>
#include <fs/vfs_node.h>
#include <lib/string.h>
#include <mm/slab.h>

struct vfs_node* vfs_node_alloc(unsigned type, const struct vfs_node_ops* restrict ops, void* ctxdata)
{
    struct vfs_node* node = slab_alloc(sizeof(struct vfs_node));

    if(node == nullptr) {
        return nullptr;
    }

    memset(node, 0, sizeof(struct vfs_node));

    node->type = type;
    node->ops = ops;
    node->ctxdata = ctxdata;

    atomic_store(&node->refcount, 1, ATOMIC_RELAXED);

    return node;
}

void vfs_node_inc_refcount(struct vfs_node* restrict node)
{
    atomic_fetch_add(&node->refcount, 1, ATOMIC_RELAXED);
}

void vfs_node_dec_refcount(struct vfs_node* restrict node)
{
    unsigned prev = atomic_fetch_sub(&node->refcount, 1, ATOMIC_ACQ_REL);

    if(prev == 1) {
        if(node->ops && node->ops->destroy)
            node->ops->destroy(node);
        slab_free(node);
    }
}
