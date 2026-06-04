#include "fatfs.h"

#include <core/cmdline.h>
#include <core/errno.h>
#include <dev/blk_device.h>
#include <fs/mount.h>
#include <fs/vnode.h>
#include <init/initcall.h>
#include <mm/malloc.h>
#include <mm/slab.h>
#include <lib/string.h>

static int fatfs_mount(struct vfs* restrict fs, const struct blk_device* restrict device, const struct cmdline* restrict params)
{
    int retval;
    size_t i;
    struct fatfs_bootparams* bpb;
    struct fatfs_context* context;
    struct fatfs_vnode_context* root_context;
    struct vnode* root;
    uint32_t cluster_count;
    uint32_t data_start;
    uint32_t fat_size;
    uint32_t root_dir_sectors;
    uint32_t total_sectors;
    union fatfs_extparams* boot;
    unsigned fs_type;

    if(device == nullptr) {
        return -EINVAL;
    }

    boot = kmalloc(sizeof(union fatfs_extparams));

    if(boot == nullptr) {
        return -ENOMEM;
    }

    retval = blk_device_read(device, 0, 1, boot);

    if(retval < 0) {
        kfree(boot);
        return retval;
    }

    bpb = &boot->fat1X.bpb;

    if(bpb->sector_size != (uint16_t)(device->sector_size) || bpb->cluster_size == 0) {
        kfree(boot);
        return -EINVAL;
    }

    fat_size = bpb->table_size_16 ? bpb->table_size_16 : boot->fat32.table_size_32;
    total_sectors = bpb->total_sectors_16 ? bpb->total_sectors_16 : bpb->total_sectors_32;

    root_dir_sectors = ((uint32_t)(bpb->root_entries) * 32 + bpb->sector_size - 1) / bpb->sector_size;
    data_start = (uint32_t)(bpb->reserved_sectors) + (uint32_t)(bpb->tables_count) * fat_size + root_dir_sectors;
    cluster_count = (total_sectors - data_start) / (uint32_t)(bpb->cluster_size);

    if(cluster_count < 4085) {
        fs_type = FATFS_TYPE_12;
    }
    else if(cluster_count < 65525) {
        fs_type = FATFS_TYPE_16;
    }
    else {
        fs_type = FATFS_TYPE_32;
    }

    context = kmalloc(sizeof(struct fatfs_context));

    if(context == nullptr) {
        kfree(boot);
        return -ENOMEM;
    }

    memset(context, 0, sizeof(struct fatfs_context));

    context->device = device;
    context->filesystem_type = fs_type;

    context->param_readonly = false;
    context->param_readonly = context->param_readonly || cmdline_contains(params, "readonly");
    context->param_readonly = context->param_readonly || cmdline_contains(params, "ro");

    context->table_start_lba = bpb->reserved_sectors;
    context->data_start_lba = data_start;
    context->cluster_size_sectors = bpb->cluster_size;

    context->cluster_size_bytes = bpb->cluster_size;
    context->cluster_size_bytes *= bpb->sector_size;

    context->sector_size = bpb->sector_size;
    context->root_directory_sectors = root_dir_sectors;

    context->fat_cache_sz = fat_size;
    context->fat_cache_sz *= bpb->sector_size;

    if(fs_type == FATFS_TYPE_32) {
        context->root_cluster = boot->fat32.root_cluster;
    }
    else {
        context->root_cluster = 0;
    }

    context->fat_cache = kmalloc(context->fat_cache_sz);

    if(context->fat_cache == nullptr) {
        kfree(boot);
        kfree(context);

        return -ENOMEM;
    }

    context->bounce = kmalloc(context->sector_size);

    if(context->bounce == nullptr) {
        kfree(boot);
        kfree(context->fat_cache);
        kfree(context);

        return -ENOMEM;
    }

    for(i = 0; i < fat_size; i++) {
        retval = blk_device_read(device, context->table_start_lba + i, 1, context->fat_cache + i * context->sector_size);

        if(retval < 0) {
            kfree(boot);
            kfree(context->bounce);
            kfree(context->fat_cache);
            kfree(context);

            return retval;
        }
    }

    kfree(boot);

    root_context = slab_alloc(sizeof(struct fatfs_vnode_context));

    if(root_context == nullptr) {
        kfree(context->bounce);
        kfree(context->fat_cache);
        kfree(context);

        return -ENOMEM;
    }

    root_context->fs_ctx = context;
    root_context->first_cluster = context->root_cluster;
    root_context->size_bytes = 0;

    root = vnode_alloc(VNODE_DIRECTORY, &fatfs_vnode_ops, root_context);

    if(root == nullptr) {
        slab_free(root_context);
        kfree(context->bounce);
        kfree(context->fat_cache);
        kfree(context);

        return -ENOMEM;
    }

    fs->root = root;
    fs->ctxdata = context;

    return 0;
}

static int fatfs_unmount(struct vfs* restrict fs)
{
    struct fatfs_context* context = fs->ctxdata;

    vnode_decref(fs->root);

    kfree(context->bounce);
    kfree(context->fat_cache);
    kfree(context);

    return 0;
}

static int fatfs_sync(struct vfs* restrict fs)
{
    struct fatfs_context* context = fs->ctxdata;

    int retval;
    size_t size;
    size_t i;

    size = context->fat_cache_sz / context->sector_size;

    for(i = 0; i < size; i++) {
        retval = blk_device_write(context->device, context->table_start_lba + i, 1, context->fat_cache + i * context->sector_size);

        if(retval < 0) {
            return retval;
        }
    }

    return 0;
}

static const struct vfs_ops fatfs_vfs_ops = {
    .mount = &fatfs_mount,
    .unmount = &fatfs_unmount,
    .sync = &fatfs_sync,
};

static struct vfs_driver fatfs_vfs_driver = {
    .identifier = "fat",
    .ops = &fatfs_vfs_ops,
};

define_initcall(fatfs_init, INITCALL_STAGE_BSP_VFS, INITCALL_ORDER_EARLY)
{
    vfs_driver_register(&fatfs_vfs_driver);
}
