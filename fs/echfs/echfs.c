#include <vex/blk_device.h>
#include <vex/cmdline.h>
#include <vex/errno.h>
#include <vex/initcall.h>
#include <vex/kmalloc.h>
#include <vex/mount.h>
#include <vex/slab.h>
#include <vex/string.h>
#include <vex/vnode.h>

#include "echfs.h"

#define ECHFS_RESERVED_BLOCKS UINT64_C(16)

static int echfs_mount(struct vfs* restrict fs, const struct blk_device* restrict device, const struct cmdline* restrict params)
{
    if(device == nullptr) {
        return -EINVAL;
    }

    size_t sector_size = device->sector_size;

    struct echfs_identity* identity = kmalloc(sizeof(struct echfs_identity));

    if(identity == nullptr) {
        return -ENOMEM;
    }

    int retval = blk_read(device, 0, 1, identity);

    if(retval < 0) {
        kfree(identity);
        return retval;
    }

    if(memcmp(identity->signature, "_ECH_FS_", 8)) {
        kfree(identity);
        return -EINVAL;
    }

    uint64_t block_size = identity->block_size;
    uint64_t total_blocks = identity->total_blocks;
    uint64_t maindir_size = identity->maindir_size;

    kfree(identity);

    if(block_size == 0 || block_size % sector_size != 0) {
        return -EINVAL;
    }

    uint64_t sectors_per_block = block_size / sector_size;

    uint64_t fat_start = ECHFS_RESERVED_BLOCKS;
    uint64_t fat_size = (total_blocks * sizeof(uint64_t) + block_size - 1) / block_size;
    uint64_t dir_start = fat_start + fat_size;
    uint64_t dir_size = maindir_size;
    uint64_t data_start = dir_start + dir_size;

    struct echfs_vfs_context* context = kmalloc(sizeof(struct echfs_vfs_context));

    if(context == nullptr) {
        return -ENOMEM;
    }

    memset(context, 0, sizeof(struct echfs_vfs_context));

    context->param_readonly = false;
    context->param_readonly = context->param_readonly || cmdline_contains(params, "ro");
    context->param_readonly = context->param_readonly || cmdline_contains(params, "readonly");

    context->device = device;
    context->block_size = block_size;
    context->sectors_per_block = sectors_per_block;
    context->total_blocks = total_blocks;
    context->fat_start = fat_start;
    context->fat_size = fat_size;
    context->dir_start = dir_start;
    context->dir_size = dir_size;
    context->data_start = data_start;
    context->dir_entries = dir_size * block_size / sizeof(struct echfs_dirent);

    context->bounce = kmalloc(sector_size);

    if(context->bounce == nullptr) {
        kfree(context);
        return -ENOMEM;
    }

    size_t fat_bytes = fat_size * block_size;

    context->fat = kmalloc(fat_bytes);

    if(context->fat == nullptr) {
        kfree(context->bounce);
        kfree(context);
        return -ENOMEM;
    }

    retval = blk_read(device, fat_start * sectors_per_block, fat_size * sectors_per_block, context->fat);

    if(retval < 0) {
        kfree(context->fat);
        kfree(context->bounce);
        kfree(context);
        return retval;
    }

    size_t dir_bytes = dir_size * block_size;

    context->dir_table = kmalloc(dir_bytes);

    if(context->dir_table == nullptr) {
        kfree(context->fat);
        kfree(context->bounce);
        kfree(context);
        return -ENOMEM;
    }

    retval = blk_read(device, dir_start * sectors_per_block, dir_size * sectors_per_block, context->dir_table);

    if(retval < 0) {
        kfree(context->dir_table);
        kfree(context->fat);
        kfree(context->bounce);
        kfree(context);
        return retval;
    }

    struct echfs_vnode_context* root_ctx = slab_alloc(sizeof(struct echfs_vnode_context));

    if(root_ctx == nullptr) {
        kfree(context->dir_table);
        kfree(context->fat);
        kfree(context->bounce);
        kfree(context);
        return -ENOMEM;
    }

    root_ctx->fs_ctx = context;
    root_ctx->dir_entry = UINT64_MAX;
    root_ctx->start_block = ECHFS_DIR_ID_ROOT;
    root_ctx->size = 0;
    root_ctx->cached_block = ECHFS_DIR_ID_ROOT;
    root_ctx->cached_block_idx = 0;

    struct vnode* root = vnode_alloc(VNODE_DIRECTORY, &echfs_vnode_ops, root_ctx);

    if(root == nullptr) {
        slab_free(root_ctx);
        kfree(context->dir_table);
        kfree(context->fat);
        kfree(context->bounce);
        kfree(context);
        return -ENOMEM;
    }

    root->fs = fs;
    root->nodenumber = (ino_t)(-1);

    fs->root = root;
    fs->ctxdata = context;

    return 0;
}

static int echfs_statvfs(struct vfs* restrict fs, struct k_statvfs* restrict out)
{
    struct echfs_vfs_context* context = fs->ctxdata;

    memset(out, 0, sizeof(struct k_statvfs));

    size_t free_blocks = 0;
    size_t data_blocks = context->total_blocks - context->data_start;

    for(uint64_t i = context->data_start; i < context->total_blocks; i++) {
        if(context->fat[i] == ECHFS_TABLE_FREE) {
            free_blocks += 1;
        }
    }

    out->f_bsize = context->device->sector_size;
    out->f_frsize = context->block_size;
    out->f_blocks = data_blocks;
    out->f_bfree = free_blocks;
    out->f_bavail = free_blocks;

    out->f_fsid = (uint32_t)(uintptr_t)(fs);
    out->f_flag = ST_NOSUID;
    out->f_namemax = 200;

    return 0;
}

static int echfs_fillnode(struct vfs* restrict fs, struct vnode* restrict node)
{
    struct echfs_vfs_context* context = fs->ctxdata;

    uint64_t index = (uint64_t)(node->nodenumber);

    if(index >= context->dir_entries) {
        return -ENOENT;
    }

    const struct echfs_dirent* entry = &context->dir_table[index];

    if(entry->directory_id == ECHFS_DIR_ID_END || entry->directory_id == ECHFS_DIR_ID_DELETED) {
        return -ENOENT;
    }

    struct echfs_vnode_context* ctx = slab_alloc(sizeof(struct echfs_vnode_context));

    if(ctx == nullptr) {
        return -ENOMEM;
    }

    ctx->fs_ctx = context;
    ctx->dir_entry = index;
    ctx->start_block = entry->start_block;
    ctx->size = entry->file_size;
    ctx->cached_block = entry->start_block;
    ctx->cached_block_idx = 0;

    node->ctxdata = ctx;

    if(entry->object_type == 1) {
        node->nodetype = VNODE_DIRECTORY;
    }
    else {
        node->nodetype = VNODE_FILE;
    }

    return 0;
}

static int echfs_unmount(struct vfs* restrict fs)
{
    struct echfs_vfs_context* context = fs->ctxdata;

    vnode_decref(fs->root);

    kfree(context->dir_table);
    kfree(context->fat);
    kfree(context->bounce);
    kfree(context);

    return 0;
}

static int echfs_sync(struct vfs* restrict fs)
{
    struct echfs_vfs_context* context = fs->ctxdata;

    int retval;

    if(context->fat_dirty) {
        retval = blk_write(context->device, context->fat_start * context->sectors_per_block, context->fat_size * context->sectors_per_block,
            context->fat);
        if(retval < 0)
            return retval;
        context->fat_dirty = false;
    }

    if(context->dir_dirty) {
        retval = blk_write(context->device, context->dir_start * context->sectors_per_block, context->dir_size * context->sectors_per_block,
            context->dir_table);
        if(retval < 0)
            return retval;
        context->dir_dirty = false;
    }

    return 0;
}

static const struct vfs_ops echfs_vfs_ops = {
    .mount = &echfs_mount,
    .statvfs = &echfs_statvfs,
    .fillnode = &echfs_fillnode,
    .unmount = &echfs_unmount,
    .sync = &echfs_sync,
};

static struct vfs_driver echfs_vfs_driver = {
    .identifier = "echfs",
    .ops = &echfs_vfs_ops,
};

static void init_echfs(void)
{
    add_vfs_driver(&echfs_vfs_driver);
}

initcall_bsp_fs(init_echfs, INIT_EARLY);
