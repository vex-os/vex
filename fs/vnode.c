#include <core/assert.h>
#include <core/atomic.h>
#include <fs/vnode.h>
#include <lib/string.h>
#include <mm/slab.h>

struct vnode* vnode_alloc(unsigned nodetype, const struct vnode_ops* restrict ops, void* ctxdata)
{
    struct vnode* result;

    weak_kassert_text(ops, "invalid ops pointer");

    result = slab_calloc(1, sizeof(struct vnode));

    if(result == nullptr) {
        return nullptr;
    }

    result->ops = ops;
    result->nodetype = nodetype;
    result->ctxdata = ctxdata;

    atomic_store(&result->refcount, 1, ATOMIC_RELAXED);

    return result;
}

void vnode_incref(struct vnode* restrict node)
{
    weak_kassert_text(node, "invalid node pointer");

    atomic_fetch_add(&node->refcount, 1, ATOMIC_RELAXED);
}

bool vnode_decref(struct vnode* restrict node)
{
    weak_kassert_text(node, "invalid node pointer");

    unsigned prev = atomic_fetch_sub(&node->refcount, 1, ATOMIC_ACQ_REL);

    if(prev == 1) {
        if(node->ops->release)
            node->ops->release(node);
        slab_free(node);
    }

    return prev == 1U;
}
