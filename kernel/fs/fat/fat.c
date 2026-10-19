#include <vex/blk_device.h>
#include <vex/cmdline.h>
#include <vex/endian.h>
#include <vex/errno.h>
#include <vex/fcache.h>
#include <vex/initcall.h>
#include <vex/kmalloc.h>
#include <vex/mount.h>
#include <vex/slab.h>
#include <vex/stdlib.h>
#include <vex/string.h>
#include <vex/vnode.h>

#include "fat.h"

static_assert(sizeof(struct fat_vnode_context) <= SLAB_MAX_SIZE);

static int fat_probe(struct vfs* restrict fs, const struct blk_device* restrict device)
{
    union fat_extparams boot;

    int retval = blk_read_sector_prefix(device, 0, &boot, sizeof(boot));

    if(retval < 0) {
        return retval;
    }

    struct fat_bootparams* base = &boot.fat1X.base;

    if(base->sector_size == 0 || base->cluster_size == 0) {
        return -ENODEV;
    }

    if(le16toh(base->table_size_16) == 0 && le32toh(boot.fat32.table_size_32) == 0) {
        return -ENODEV;
    }

    if(le16toh(base->total_sectors_16) == 0 && le32toh(base->total_sectors_32) == 0) {
        return -ENODEV;
    }

    return 0;
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

    int retval = blk_read_sector_prefix(device, 0, boot, sizeof(*boot));

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

    if(base->tables_count == 0 || fat_size == 0 || total_sectors <= data_start || total_sectors > device->sector_count) {
        kfree(boot);
        return -EINVAL;
    }

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

    // Every data cluster needs a table entry: the cluster table code
    // relies on this to keep its accesses inside the cached table
    size_t table_bytes = fat_size * sector_size;
    size_t table_entries;

    if(filesystem_type == FAT_TYPE_12) {
        table_entries = table_bytes * 2 / 3;
    }
    else if(filesystem_type == FAT_TYPE_16) {
        table_entries = table_bytes / sizeof(uint16_t);
    }
    else {
        table_entries = table_bytes / sizeof(uint32_t);
    }

    if(cluster_count == 0 || table_entries < cluster_count + 2) {
        kfree(boot);
        return -EINVAL;
    }

    if(filesystem_type == FAT_TYPE_32) {
        uintptr_t root_cluster = le32toh(boot->fat32.root_cluster);

        if(root_cluster < 2 || root_cluster >= cluster_count + 2) {
            kfree(boot);
            return -EINVAL;
        }
    }

    struct fat_vfs_context* context = kmalloc(sizeof(struct fat_vfs_context));

    if(context == nullptr) {
        kfree(boot);
        return -ENOMEM;
    }

    mutex_init(&context->lock);

    context->device = device;
    context->filesystem_type = filesystem_type;

    context->param_readonly = cmdline_contains(params, "readonly") || cmdline_contains(params, "ro");
    context->param_quiet = cmdline_contains(params, "quiet");

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

    context->fsinfo_invalidated = false;
    context->next_free_hint = 2;
    context->devnode = fcache_dev_create(fs);

    kfree(boot);

    if(context->devnode == nullptr) {
        kfree(context);
        return -ENOMEM;
    }

    struct fat_vnode_context* root_context = slab_alloc(sizeof(struct fat_vnode_context));

    if(root_context == nullptr) {
        fcache_dev_destroy(context->devnode);
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
        fcache_dev_destroy(context->devnode);
        kfree(context);
        return -ENOMEM;
    }

    root->fs = fs;
    root->nodenumber = 0;

    fs->root = root;
    fs->ctxdata = context;
    fs->readonly = context->param_readonly;

    return 0;
}

static int statvfs_locked(struct vfs* restrict fs, struct k_statvfs* restrict out)
{
    struct fat_vfs_context* context = fs->ctxdata;

    memset(out, 0, sizeof(struct k_statvfs));

    size_t free_clusters = fat_cluster_count_free(context);
    size_t total = context->cluster_count;

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

static int fillnode_locked(struct vfs* restrict fs, struct vnode* restrict node)
{
    struct fat_vfs_context* context = fs->ctxdata;

    ino_t nodenumber = node->nodenumber;
    uintptr_t lba = nodenumber >> 32;
    uintptr_t byte_offset = nodenumber & 0xFFFFFFFF;

    struct fat_dir_entry raw_entry;
    int retval = fcache_dev_read(context->devnode, lba * context->sector_size + byte_offset, &raw_entry, sizeof(raw_entry));

    if(retval < 0) {
        return retval;
    }

    uintptr_t cluster = 0;
    cluster |= (uintptr_t)le16toh(raw_entry.first_cluster_hi) << 16;
    cluster |= (uintptr_t)le16toh(raw_entry.first_cluster_lo);

    struct fat_vnode_context* node_context = slab_alloc(sizeof(struct fat_vnode_context));

    if(node_context == nullptr) {
        return -ENOMEM;
    }

    uintptr_t num_entries = context->sector_size / sizeof(union fat_entry);
    uintptr_t dir_entry_s = byte_offset / sizeof(union fat_entry);
    uintptr_t dir_lba = lba;

    // Recovered from the node number so writes don't have to scan the directory
    uintptr_t dir_cluster;
    size_t dir_offset;

    uintptr_t root_start = context->data_start - context->rootdir_sectors;

    if(dir_lba >= root_start && dir_lba < context->data_start) {
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
    node_context->size_bytes = le32toh(raw_entry.file_size);
    node_context->dir_cluster = dir_cluster;
    node_context->dir_offset = dir_offset;
    node_context->cached_cluster = cluster;
    node_context->cached_cluster_idx = 0;

    node->ctxdata = node_context;

    if(raw_entry.attribute & FAT_ATTR_DIRECTORY) {
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

    fcache_dev_destroy(context->devnode);
    mutex_deinit(&context->lock);
    kfree(context);

    return 0;
}

static int fat_statvfs(struct vfs* restrict fs, struct k_statvfs* restrict out)
{
    struct fat_vfs_context* context = fs->ctxdata;
    mutex_acquire(&context->lock);
    int retval = statvfs_locked(fs, out);
    mutex_release(&context->lock);
    return retval;
}

static int fat_fillnode(struct vfs* restrict fs, struct vnode* restrict node)
{
    struct fat_vfs_context* context = fs->ctxdata;
    mutex_acquire(&context->lock);
    int retval = fillnode_locked(fs, node);
    mutex_release(&context->lock);
    return retval;
}

static int fat_sync(struct vfs* restrict fs)
{
    struct fat_vfs_context* context = fs->ctxdata;
    return vnode_fsync(context->devnode);
}

static const struct vfs_ops fat_vfs_ops = {
    .probe = &fat_probe,
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

static void init_fat(void)
{
    add_vfs_driver(&fat_vfs_driver);
}

initcall_bsp_vfs(init_fat, INIT_EARLY);
