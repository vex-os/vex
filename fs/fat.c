#include <stdlib.h>
#include <string.h>
#include <sys/blk_device.h>
#include <sys/cmdline.h>
#include <sys/endian.h>
#include <sys/errno.h>
#include <sys/initcall.h>
#include <sys/malloc.h>
#include <sys/slab.h>
#include <sys/vfs_mount.h>
#include <sys/vfs_vnode.h>

#include "fat.h"

static void invalidate_fsinfo(struct fat_vfs_context* restrict context)
{
    if(context->filesystem_type != FAT_TYPE_32 || context->fsinfo_lba == 0) {
        return;
    }

    if(blk_read(context->device, context->fsinfo_lba, 1, context->bounce)) {
        return;
    }

    struct fat_fsinfo_fat32* info = (struct fat_fsinfo_fat32*)(context->bounce);

    bool fsinfo_valid = true;
    fsinfo_valid = fsinfo_valid && le32toh(info->lead_signature) == UINT32_C(0x41615252);
    fsinfo_valid = fsinfo_valid && le32toh(info->struct_signature) == UINT32_C(0x61417272);
    fsinfo_valid = fsinfo_valid && le32toh(info->trail_signature) == UINT32_C(0xAA550000);

    if(fsinfo_valid) {
        info->free_count = htole32(UINT32_C(0xFFFFFFFF));
        info->next_free = htole32(UINT32_C(0xFFFFFFFF));

        blk_write(context->device, context->fsinfo_lba, 1, context->bounce);
    }
}

static int fat_mount(struct vfs* restrict fs, const struct blk_device* restrict device, const struct cmdline* restrict params)
{
    if(device == nullptr) {
        return -EINVAL;
    }

    union fat_extparams* boot = kmalloc(sizeof(union fat_extparams));

    if(boot == nullptr) {
        return -ENOMEM;
    }

    int retval = blk_read(device, 0, 1, boot);

    if(retval < 0) {
        kfree(boot);
        return retval;
    }

    struct fat_bootparams* base = &boot->fat1X.base;

    size_t sector_size = le16toh(base->sector_size);

    if(sector_size != device->sector_size || base->cluster_size == 0) {
        kfree(boot);
        return -EINVAL;
    }

    size_t fat_size;
    size_t total_sectors;

    if(le16toh(base->table_size_16)) {
        fat_size = le16toh(base->table_size_16);
    }
    else {
        fat_size = le32toh(boot->fat32.table_size_32);
    }

    if(le16toh(base->total_sectors_16)) {
        total_sectors = le16toh(base->total_sectors_16);
    }
    else {
        total_sectors = le32toh(base->total_sectors_32);
    }

    size_t root_entries = le16toh(base->root_entries);
    size_t reserved_sectors = le16toh(base->reserved_sectors);

    size_t rootdir_sectors = root_entries * 32;
    rootdir_sectors += sector_size - 1;
    rootdir_sectors /= sector_size;

    uintptr_t data_start = reserved_sectors;
    data_start += base->tables_count * fat_size;
    data_start += rootdir_sectors;

    size_t cluster_count = total_sectors - data_start;
    cluster_count /= base->cluster_size;

    unsigned filesystem_type;

    if(cluster_count < 4085) {
        filesystem_type = FAT_TYPE_12;
    }
    else if(cluster_count < 65525) {
        filesystem_type = FAT_TYPE_16;
    }
    else {
        filesystem_type = FAT_TYPE_32;
    }

    struct fat_vfs_context* context = kmalloc(sizeof(struct fat_vfs_context));

    if(context == nullptr) {
        kfree(boot);
        return -ENOMEM;
    }

    context->device = device;
    context->filesystem_type = filesystem_type;

    context->param_readonly = cmdline_contains(params, "readonly") || cmdline_contains(params, "ro");

    const char* uid_string = cmdline_argument(params, "uid");
    const char* gid_string = cmdline_argument(params, "gid");

    if(uid_string == nullptr) {
        context->param_uid = 0;
    }
    else {
        context->param_uid = strtoul(uid_string, nullptr, 10);
    }

    if(gid_string == nullptr) {
        context->param_gid = 0;
    }
    else {
        context->param_gid = strtoul(gid_string, nullptr, 10);
    }

    if(filesystem_type == FAT_TYPE_32) {
        context->root_cluster = le32toh(boot->fat32.root_cluster);
    }
    else {
        context->root_cluster = 0;
    }

    context->table_start = reserved_sectors;
    context->data_start = data_start;
    context->rootdir_sectors = rootdir_sectors;
    context->cluster_size_s = base->cluster_size;
    context->cluster_size_b = base->cluster_size * sector_size;
    context->sector_size = sector_size;
    context->table_size_s = fat_size;
    context->num_tables = base->tables_count;
    context->cluster_count = cluster_count;

    if(filesystem_type == FAT_TYPE_32) {
        context->fsinfo_lba = le16toh(boot->fat32.fs_info);
    }
    else {
        context->fsinfo_lba = 0;
    }

    context->cache_size = fat_size * sector_size;
    context->cache = kmalloc(context->cache_size);
    context->cache_dirty = false;

    if(context->cache == nullptr) {
        kfree(boot);
        kfree(context);
        return -ENOMEM;
    }

    context->next_free_hint = 2;

    context->bounce = kmalloc(context->sector_size);

    if(context->bounce == nullptr) {
        kfree(boot);
        kfree(context->cache);
        kfree(context);
        return -ENOMEM;
    }

    retval = blk_read(device, context->table_start, fat_size, context->cache);

    if(retval < 0) {
        kfree(boot);
        kfree(context->bounce);
        kfree(context->cache);
        kfree(context);
        return retval;
    }

    kfree(boot);

    struct fat_vnode_context* root_context = slab_alloc(sizeof(struct fat_vnode_context));

    if(root_context == nullptr) {
        kfree(context->bounce);
        kfree(context->cache);
        kfree(context);
        return -ENOMEM;
    }

    root_context->fs_ctx = context;
    root_context->first_cluster = context->root_cluster;
    root_context->size_bytes = 0;
    root_context->cached_cluster = context->root_cluster;
    root_context->cached_cluster_idx = 0;

    struct vnode* root = vnode_alloc(VNODE_DIRECTORY, &fat_vnode_ops, root_context);

    if(root == nullptr) {
        slab_free(root_context);
        kfree(context->bounce);
        kfree(context->cache);
        kfree(context);
        return -ENOMEM;
    }

    root->fs = fs;
    root->nodenumber = 0;

    fs->root = root;
    fs->ctxdata = context;

    return 0;
}

static int fat_statvfs(struct vfs* restrict fs, struct k_statvfs* restrict out)
{
    struct fat_vfs_context* context = fs->ctxdata;

    memset(out, 0, sizeof(struct k_statvfs));

    size_t free_clusters = 0;
    size_t total = context->cluster_count;

    if(context->filesystem_type == FAT_TYPE_32) {
        const uint32_t* fat = (const uint32_t*)(context->cache);

        for(size_t i = 2; i < total + 2; ++i) {
            if((le32toh(fat[i]) & UINT32_C(0x0FFFFFFF)) == 0) {
                free_clusters += 1;
            }
        }
    }
    else if(context->filesystem_type == FAT_TYPE_16) {
        const uint16_t* fat = (const uint16_t*)(context->cache);

        for(size_t i = 2; i < total + 2; ++i) {
            if(le16toh(fat[i]) == 0) {
                free_clusters += 1;
            }
        }
    }
    else {
        for(size_t i = 2; i < total + 2; i++) {
            size_t byte_off = i + i / 2;

            uint16_t value = UINT16_C(0);
            value |= context->cache[byte_off];
            value |= ((uint16_t)context->cache[byte_off + 1] << 8);

            if(i & 1) {
                value >>= 4;
            }
            else {
                value &= 0x0FFF;
            }

            if(value == 0) {
                free_clusters += 1;
            }
        }
    }

    out->f_bsize = context->sector_size;
    out->f_frsize = context->cluster_size_b;
    out->f_blocks = total;
    out->f_bfree = free_clusters;

    if(!context->param_readonly) {
        out->f_bavail = free_clusters;
    }

    out->f_fsid = (uint32_t)(uintptr_t)(fs);
    out->f_flag = ST_NOSUID;
    out->f_namemax = 255;

    if(context->param_readonly) {
        out->f_flag |= ST_RDONLY;
    }

    return 0;
}

static int fat_fillnode(struct vfs* restrict fs, struct vnode* restrict node)
{
    struct fat_vfs_context* context = fs->ctxdata;

    ino_t nodenumber = node->nodenumber;
    uintptr_t lba = nodenumber >> 32;
    uintptr_t byte_offset = nodenumber & 0xFFFFFFFF;

    int retval = blk_read(context->device, lba, 1, context->bounce);

    if(retval < 0) {
        return retval;
    }

    const struct fat_dir_entry* raw_entry = (const struct fat_dir_entry*)(context->bounce + byte_offset);

    uintptr_t cluster = 0;
    cluster |= (uintptr_t)le16toh(raw_entry->first_cluster_hi) << 16;
    cluster |= (uintptr_t)le16toh(raw_entry->first_cluster_lo);

    struct fat_vnode_context* node_context = slab_alloc(sizeof(struct fat_vnode_context));

    if(node_context == nullptr) {
        return -ENOMEM;
    }

    uintptr_t num_entries = context->sector_size / sizeof(union fat_entry);
    uintptr_t dir_entry_s = byte_offset / sizeof(union fat_entry);
    uintptr_t dir_lba = lba;

    // Reconstruct dir_cluster and dir_offset from the nodenumber
    // so that writes can update entries without additional scanning.
    // For FAT12/FAT16 the root directory lives before data_start (cluster == 0).

    uintptr_t dir_cluster;
    size_t dir_offset;

    uintptr_t root_start = context->data_start - context->rootdir_sectors;

    if(dir_lba >= root_start && dir_lba < context->data_start) {
        // Entry is in the fixed FAT12/FAT16 root directory area
        dir_cluster = 0;
        dir_offset = (dir_lba - root_start) * num_entries + dir_entry_s;
    }
    else {
        uintptr_t cluster_lba = dir_lba - dir_lba % context->cluster_size_s;

        dir_cluster = (cluster_lba - context->data_start) / context->cluster_size_s + 2;

        dir_offset = dir_lba - fat_cluster_lba(context, dir_cluster);
        dir_offset = dir_offset * num_entries + dir_entry_s;
    }

    node_context->fs_ctx = context;
    node_context->first_cluster = cluster;
    node_context->size_bytes = le32toh(raw_entry->file_size);
    node_context->dir_cluster = dir_cluster;
    node_context->dir_offset = dir_offset;
    node_context->cached_cluster = cluster;
    node_context->cached_cluster_idx = 0;

    node->ctxdata = node_context;

    if(raw_entry->attribute & FAT_ATTR_DIRECTORY) {
        node->nodetype = VNODE_DIRECTORY;
    }
    else {
        node->nodetype = VNODE_FILE;
    }

    return 0;
}

static int fat_unmount(struct vfs* restrict fs)
{
    struct fat_vfs_context* context = fs->ctxdata;

    vnode_decref(fs->root);

    kfree(context->bounce);
    kfree(context->cache);
    kfree(context);

    return 0;
}

static int fat_sync(struct vfs* restrict fs)
{
    struct fat_vfs_context* context = fs->ctxdata;

    if(!context->cache_dirty) {
        return 0;
    }

    size_t num_sectors = context->cache_size / context->sector_size;

    for(unsigned i = 0; i < context->num_tables; i++) {
        uintptr_t lba = context->table_start + i * context->table_size_s;
        int retval = blk_write(context->device, lba, num_sectors, context->cache);

        if(retval < 0) {
            return retval;
        }
    }

    context->cache_dirty = false;

    invalidate_fsinfo(context);

    return 0;
}

static const struct vfs_ops fat_vfs_ops = {
    .mount = &fat_mount,
    .statvfs = &fat_statvfs,
    .fillnode = &fat_fillnode,
    .unmount = &fat_unmount,
    .sync = &fat_sync,
};

static struct vfs_driver fat_vfs_driver = {
    .identifier = "fat", // FIXME: figure out aliasing so it's possible to mount via fat, and msdos
    .ops = &fat_vfs_ops,
};

define_initcall(fat, INITCALL_STAGE_BSP_VFS, INIT_EARLY)
{
    add_vfs_driver(&fat_vfs_driver);
}
