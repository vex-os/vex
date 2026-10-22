#include <vex/chr_device.h>
#include <vex/chr_event.h>
#include <vex/errno.h>
#include <vex/file.h>
#include <vex/slab.h>
#include <vex/stat.h>
#include <vex/string.h>
#include <vex/vms.h>
#include <vex/vnode.h>

#include "devfs_internal.h"

typedef struct devfs_chr_context_s devfs_chr_context_t;

struct devfs_chr_context_s {
    const chr_device_t* device;
};

static_assert(sizeof(devfs_chr_context_t) <= SLAB_MAX_SIZE);

static void devfs_chr_release(vnode_t* restrict node)
{
    devfs_chr_context_t* context = node->ctxdata;
    chr_device_decref(context->device);
    slab_free(context);
}

static int devfs_chr_open(vnode_t* restrict node, file_t* restrict file)
{
    const devfs_chr_context_t* context = node->ctxdata;

    return chr_device_open(context->device, file->flags, &file->ctxdata);
}

static int devfs_chr_close(vnode_t* restrict node, file_t* restrict file)
{
    const devfs_chr_context_t* context = node->ctxdata;

    chr_device_close(context->device, file->ctxdata);

    return 0;
}

static ssize_t devfs_chr_read(vnode_t* restrict node, file_t* restrict file, void* restrict buffer, size_t size, off_t offset)
{
    const devfs_chr_context_t* context = node->ctxdata;

    return chr_device_read(context->device, file->ctxdata, offset, buffer, size, file_flags(file));
}

static ssize_t devfs_chr_write(vnode_t* restrict node, file_t* restrict file, const void* restrict buffer, size_t size, off_t offset)
{
    const devfs_chr_context_t* context = node->ctxdata;

    return chr_device_write(context->device, file->ctxdata, offset, buffer, size, file_flags(file));
}

static off_t devfs_chr_seek(vnode_t* restrict node, file_t* restrict file, off_t offset, int origin)
{
    const devfs_chr_context_t* context = node->ctxdata;

    return chr_device_seek(context->device, file->ctxdata, file->offset, offset, origin);
}

static int devfs_chr_ioctl(vnode_t* restrict node, file_t* restrict file, unsigned request, uintptr_t argument)
{
    const devfs_chr_context_t* context = node->ctxdata;

    return chr_device_ioctl(context->device, file->ctxdata, request, argument);
}

static int devfs_chr_mmap(vnode_t* restrict node, off_t offset, size_t length)
{
    const devfs_chr_context_t* context = node->ctxdata;

    return chr_device_mmap(context->device, offset, length);
}

static int devfs_chr_resolve_page(vnode_t* restrict node, off_t offset, uintptr_t* restrict phys, unsigned* restrict cache)
{
    const devfs_chr_context_t* context = node->ctxdata;

    return chr_device_resolve_page(context->device, offset, phys, cache);
}

static int devfs_chr_stat(vnode_t* restrict node, stat_t* restrict out)
{
    const devfs_chr_context_t* context = node->ctxdata;
    const chr_device_t* device = context->device;

    memset(out, 0, sizeof(stat_t));

    out->st_mode = S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
    out->st_rdev = device->priv.dev;

    return 0;
}

static const vnode_ops_t devfs_chr_ops = {
    .release = &devfs_chr_release,
    .read = &devfs_chr_read,
    .write = &devfs_chr_write,
    .seek = &devfs_chr_seek,
    .ioctl = &devfs_chr_ioctl,
    .stat = &devfs_chr_stat,
    .open = &devfs_chr_open,
    .close = &devfs_chr_close,
    .mmap = &devfs_chr_mmap,
    .resolve_page = &devfs_chr_resolve_page,
};

static void revoke_mappings(vfs_t* restrict fs, const chr_device_t* restrict device)
{
    vnode_t* node = devfs_find(fs, device->name);

    if(node) {
        vms_revoke(node);
        vnode_decref(node);
    }
}

void devfs_chr_event_handler(unsigned event, const chr_device_t* restrict device, void* restrict ctxdata)
{
    if(event == CHR_EVENT_DEVICE_ADDED) {
        devfs_chr_context_t* context = slab_alloc(sizeof(devfs_chr_context_t));

        if(context == nullptr) {
            return;
        }

        context->device = device;

        vnode_t* vnode = vnode_alloc(VNODE_CHAR_DEV, &devfs_chr_ops, context);

        if(vnode == nullptr) {
            slab_free(context);
            return;
        }

        chr_device_incref(device); // Dropped by devfs_chr_release

        int retval = devfs_insert(ctxdata, device->name, vnode);

        if(retval < 0) {
            vnode_decref(vnode);
        }

        return;
    }

    if(event == CHR_EVENT_DEVICE_REMOVED) {
        revoke_mappings(ctxdata, device);
        devfs_remove(ctxdata, device->name);
        return;
    }

    if(event == CHR_EVENT_MEMORY_CHANGED) {
        revoke_mappings(ctxdata, device);
        return;
    }
}
