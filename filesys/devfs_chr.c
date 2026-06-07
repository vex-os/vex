#include <string.h>
#include <sys/chr_device.h>
#include <sys/chr_event.h>
#include <sys/errno.h>
#include <sys/slab.h>
#include <sys/stat.h>
#include <sys/vfs_vnode.h>

#include "devfs.h"

struct devfs_chr_context {
    const struct chr_device* device;
};

static void devfs_chr_release(struct vnode* restrict node)
{
    slab_free(node->ctxdata);
}

static ssize_t devfs_chr_read(struct vnode* restrict node, void* restrict buffer, size_t sz, off_t offset)
{
    const struct devfs_chr_context* context = node->ctxdata;

    return chr_device_read(context->device, offset, buffer, sz);
}

static ssize_t devfs_chr_write(struct vnode* restrict node, const void* restrict buffer, size_t sz, off_t offset)
{
    const struct devfs_chr_context* context = node->ctxdata;

    return chr_device_write(context->device, offset, buffer, sz);
}

static int devfs_chr_ioctl(struct vnode* restrict node, unsigned request, void* restrict arg)
{
    const struct devfs_chr_context* context = node->ctxdata;

    return chr_device_ioctl(context->device, request, arg);
}

static int devfs_chr_stat(struct vnode* restrict node, struct k_stat* restrict out)
{
    memset(out, 0, sizeof(struct k_stat));

    out->st_mode = S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;

    return 0;
}

static const struct vnode_ops devfs_chr_ops = {
    .release = &devfs_chr_release,
    .read = &devfs_chr_read,
    .write = &devfs_chr_write,
    .ioctl = &devfs_chr_ioctl,
    .stat = &devfs_chr_stat,
};

void devfs_chr_event_handler(unsigned event, const struct chr_device* restrict device, void* restrict ctxdata)
{
    if(event == CHR_EVENT_DEVICE_ADDED) {
        struct devfs_chr_context* context = slab_alloc(sizeof(struct devfs_chr_context));

        if(context == nullptr) {
            return;
        }

        context->device = device;

        struct vnode* vnode = vnode_alloc(VNODE_CHAR_DEV, &devfs_chr_ops, context);

        if(vnode == nullptr) {
            slab_free(context);
            return;
        }

        int retval = devfs_insert(ctxdata, device->identifier, vnode);

        if(retval < 0) {
            vnode_decref(vnode);
        }

        return;
    }

    if(event == CHR_EVENT_DEVICE_REMOVED) {
        devfs_remove(ctxdata, device->identifier);
        return;
    }
}
