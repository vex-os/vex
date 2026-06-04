#include <core/errno.h>
#include <dev/chr_device.h>
#include <dev/chr_event.h>
#include <fs/devfs_internal.h>
#include <fs/vfs_node.h>
#include <fs/vfs_types.h>
#include <lib/string.h>
#include <mm/slab.h>

struct devfs_chr_context {
    const struct chr_device* device;
};

static ssize_t devfs_chr_read(struct vfs_node* restrict node, void* restrict buffer, size_t sz, size_t offset)
{
    const struct devfs_chr_context* context = node->ctxdata;

    return chr_device_read(context->device, offset, buffer, sz);
}

static ssize_t devfs_chr_write(struct vfs_node* restrict node, const void* restrict buffer, size_t sz, size_t offset)
{
    const struct devfs_chr_context* context = node->ctxdata;

    return chr_device_write(context->device, offset, buffer, sz);
}

static int devfs_chr_ioctl(struct vfs_node* restrict node, unsigned request, void* restrict arg)
{
    const struct devfs_chr_context* context = node->ctxdata;

    return chr_device_ioctl(context->device, request, arg);
}

static int devfs_chr_stat(struct vfs_node* restrict node, struct vfs_stat* restrict out_stat)
{
    memset(out_stat, 0, sizeof(struct vfs_stat));
    out_stat->type = VFS_NODE_TYPE_CHR_DEVICE;
    return 0;
}

static void devfs_chr_destroy(struct vfs_node* restrict node)
{
    slab_free(node->ctxdata);
}

static const struct vfs_node_ops devfs_chr_ops = {
    .destroy = &devfs_chr_destroy,
    .read = &devfs_chr_read,
    .write = &devfs_chr_write,
    .ioctl = &devfs_chr_ioctl,
    .stat = &devfs_chr_stat,
};

void devfs_chr_event_handler(unsigned event, const struct chr_device* restrict device, void* restrict ctxdata)
{
    int retval;
    struct devfs_chr_context* context;
    struct vfs_node* vnode;

    if(event == CHR_EVENT_DEVICE_ADDED) {
        context = slab_alloc(sizeof(struct devfs_chr_context));

        if(context == nullptr) {
            return;
        }

        context->device = device;

        vnode = vfs_node_alloc(VFS_NODE_TYPE_CHR_DEVICE, &devfs_chr_ops, context);

        if(vnode == nullptr) {
            slab_free(context);
            return;
        }

        retval = devfs_insert(device->identifier, vnode);

        if(retval < 0) {
            vfs_node_dec_refcount(vnode);
        }

        return;
    }

    if(event == CHR_EVENT_DEVICE_REMOVED) {
        devfs_remove(device->identifier);
        return;
    }
}
