#include "fatfs.h"

#include <core/errno.h>
#include <dev/blk_device.h>
#include <fs/dirent.h>
#include <fs/stat.h>
#include <fs/vnode.h>
#include <lib/string.h>
#include <lib/strings.h>
#include <mm/slab.h>

static void fatfs_vnode_release(struct vnode* restrict node);
static int fatfs_vnode_lookup(struct vnode* restrict node, const char* restrict name, struct vnode** restrict out);
static int fatfs_vnode_iterate(struct vnode* restrict node, struct dirent* out_dirent, size_t index);
static ssize_t fatfs_vnode_read(struct vnode* restrict node, void* restrict buffer, size_t sz, uintptr_t offset);
static int fatfs_vnode_stat(struct vnode* restrict node, struct __k_stat* restrict out_stat);

const struct vnode_ops fatfs_vnode_ops = {
    .release = &fatfs_vnode_release,
    .lookup = &fatfs_vnode_lookup,
    .iterate = &fatfs_vnode_iterate,
    .read = &fatfs_vnode_read,
    .stat = &fatfs_vnode_stat,
};

static void fatfs_vnode_release(struct vnode* restrict node)
{
    slab_free(node->ctxdata);
}

static int fatfs_vnode_lookup(struct vnode* restrict node, const char* restrict name, struct vnode** restrict out)
{
    struct fatfs_vnode_context* context = node->ctxdata;

    int retval;
    size_t offset;
    struct fatfs_dirent dirent;
    struct fatfs_vnode_context* out_context;
    struct vnode* out_node;
    uint32_t cluster;
    unsigned type;

    offset = 0;

    for(;;) {
        retval = read_dirent(context->fs_ctx, context->first_cluster, &offset, &dirent);

        if(retval < 0) {
            return retval;
        }

        if(strncasecmp(dirent.full_name, name, PATH_NAME_SIZE)) {
            continue;
        }

        cluster = UINT32_C(0);
        cluster |= (uint32_t)(dirent.basic.first_cluster_hi) << 16;
        cluster |= (uint32_t)(dirent.basic.first_cluster_lo);

        if(dirent.basic.attribute & FATFS_ATTR_DIRECTORY) {
            type = VNODE_DIRECTORY;
        }
        else {
            type = VNODE_FILE;
        }

        out_context = slab_alloc(sizeof(struct fatfs_vnode_context));

        if(out_context == nullptr) {
            return -ENOMEM;
        }

        out_context->fs_ctx = context->fs_ctx;
        out_context->first_cluster = cluster;
        out_context->size_bytes = dirent.basic.file_size;

        out_node = vnode_alloc(type, &fatfs_vnode_ops, out_context);

        if(out_node == nullptr) {
            slab_free(out_context);
            return -ENOMEM;
        }

        out[0] = out_node;

        return 0;
    }

    unreachable();
}

static int fatfs_vnode_iterate(struct vnode* restrict node, struct dirent* out, size_t index)
{
    struct fatfs_vnode_context* context = node->ctxdata;

    int retval;
    size_t offset;
    size_t visible;
    struct fatfs_dirent entry;

    visible = 0;
    offset = 0;

    for(;;) {
        retval = read_dirent(context->fs_ctx, context->first_cluster, &offset, &entry);

        if(retval < 0) {
            return retval;
        }

        if(entry.basic.attribute & FATFS_ATTR_VOLUME_ID) {
            continue;
        }

        if(visible < index) {
            visible += 1;
            continue;
        }

        if(entry.basic.attribute & FATFS_ATTR_DIRECTORY) {
            out->nodetype = VNODE_DIRECTORY;
        }
        else {
            out->nodetype = VNODE_FILE;
        }

        memset(out->name, 0, PATH_NAME_SIZE);
        kstrncpy(out->name, entry.full_name, PATH_NAME_SIZE);

        return 0;
    }

    unreachable();
}

static int fatfs_vnode_stat(struct vnode* restrict node, struct __k_stat* restrict out)
{
    struct fatfs_vnode_context* context = node->ctxdata;

    memset(out, 0, sizeof(struct __k_stat));

    out->st_size = (off_t)(context->size_bytes);
    out->st_blksize = (blksize_t)(context->fs_ctx->cluster_size_bytes);

    out->st_blocks = (blkcnt_t)(context->size_bytes);
    out->st_blocks += context->fs_ctx->cluster_size_bytes - 1;
    out->st_blocks /= context->fs_ctx->cluster_size_bytes;

    return 0;
}

static ssize_t fatfs_vnode_read(struct vnode* restrict node, void* restrict buffer, size_t sz, uintptr_t offset)
{
    struct fatfs_vnode_context* context = node->ctxdata;

    int retval;
    size_t chunk;
    size_t cluster_index;
    size_t done;
    size_t i;
    size_t offset_in_cluster;
    size_t offset_in_sector;
    size_t remaining;
    size_t sector_index;
    uint32_t cluster;
    uintptr_t lba;

    remaining = sz;
    done = 0;

    if(offset >= context->size_bytes) {
        return 0;
    }

    if(offset + sz > context->size_bytes) {
        remaining = context->size_bytes - offset;
    }

    cluster_index = offset / context->fs_ctx->cluster_size_bytes;
    cluster = context->first_cluster;

    for(i = 0; i < cluster_index; ++i) {
        cluster = iterate_cluster(context->fs_ctx, cluster);

        if(cluster == UINT32_MAX) {
            if(done > 0)
                return (ssize_t)(done);
            return -EIO;
        }
    }

    offset_in_cluster = offset % context->fs_ctx->cluster_size_bytes;

    while(remaining > 0 && cluster < UINT32_MAX) {
        sector_index = offset_in_cluster / context->fs_ctx->sector_size;
        offset_in_sector = offset_in_cluster % context->fs_ctx->sector_size;

        while(remaining > 0 && sector_index < context->fs_ctx->cluster_size_sectors) {
            lba = cluster_lba(context->fs_ctx, cluster) + sector_index;
            retval = blk_device_read(context->fs_ctx->device, lba, 1, context->fs_ctx->bounce);

            if(retval < 0) {
                if(done > 0)
                    return (ssize_t)(done);
                return retval;
            }

            chunk = context->fs_ctx->sector_size - offset_in_sector;

            if(chunk > remaining) {
                chunk = remaining;
            }

            memcpy(&((uint8_t*)(buffer))[done], &context->fs_ctx->bounce[offset_in_sector], chunk);

            done += chunk;
            remaining -= chunk;
            offset_in_sector = 0;
            sector_index += 1;
        }

        offset_in_cluster = 0;
        cluster = iterate_cluster(context->fs_ctx, cluster);
    }

    return (ssize_t)(done);
}
