#include <fs/fat.h>
#include <fs/stat.h>
#include <fs/vfs_dirent.h>
#include <fs/vfs_vnode.h>

#include <core/errno.h>
#include <dev/blk_device.h>
#include <lib/ctype.h>
#include <lib/string.h>
#include <lib/strings.h>
#include <mm/slab.h>

static void fat_vnode_release(struct vnode* restrict node);
static int fat_vnode_lookup(struct vnode* restrict node, const char* restrict name, struct vnode** restrict out);
static int fat_vnode_iterate(struct vnode* restrict node, struct dirent* restrict out, size_t index);
static int fat_vnode_create(struct vnode* restrict node, const char* restrict name, unsigned type, struct vnode** restrict out);
static ssize_t fat_vnode_read(struct vnode* restrict node, void* restrict buffer, size_t sz, uintptr_t offset);
static ssize_t fat_vnode_write(struct vnode* restrict node, const void* restrict buffer, size_t sz, uintptr_t offset);
static int fat_vnode_stat(struct vnode* restrict node, struct __k_stat* restrict out);

const struct vnode_ops fat_vnode_ops = {
    .release = &fat_vnode_release,
    .lookup = &fat_vnode_lookup,
    .iterate = &fat_vnode_iterate,
    .create = &fat_vnode_create,
    .read = &fat_vnode_read,
    .write = &fat_vnode_write,
    .stat = &fat_vnode_stat,
};

static void fat_vnode_release(struct vnode* restrict node)
{
    slab_free(node->ctxdata);
}

static int fat_vnode_lookup(struct vnode* restrict node, const char* restrict name, struct vnode** restrict out)
{
    struct fat_vnode_context* context = node->ctxdata;
    struct fat_dirent dirent;
    size_t offset = 0;

    for(;;) {
        int retval = fat_dirent_read(context->fs_ctx, context->first_cluster, &offset, &dirent);

        if(retval < 0) {
            return retval;
        }

        if(strncasecmp(dirent.full_name, name, PATH_NAME_SIZE)) {
            continue;
        }

        uintptr_t cluster = 0;
        cluster |= (uintptr_t)(dirent.base.first_cluster_hi) << 16;
        cluster |= (uintptr_t)(dirent.base.first_cluster_lo);

        unsigned nodetype;

        if(dirent.base.attribute & FAT_ATTR_DIRECTORY) {
            nodetype = VNODE_DIRECTORY;
        }
        else {
            nodetype = VNODE_FILE;
        }

        struct fat_vnode_context* out_context = slab_alloc(sizeof(struct fat_vnode_context));

        if(out_context == nullptr) {
            return -ENOMEM;
        }

        out_context->fs_ctx = context->fs_ctx;
        out_context->first_cluster = cluster;
        out_context->size_bytes = dirent.base.file_size;
        out_context->dir_cluster = context->first_cluster;
        out_context->dir_offset = offset - 1;

        struct vnode* out_node = vnode_alloc(nodetype, &fat_vnode_ops, out_context);

        if(out_node == nullptr) {
            slab_free(out_context);
            return -ENOMEM;
        }

        out[0] = out_node;

        return 0;
    }

    unreachable();
}

static int fat_vnode_iterate(struct vnode* restrict node, struct dirent* restrict out, size_t index)
{
    struct fat_vnode_context* context = node->ctxdata;
    struct fat_dirent entry;
    size_t offset = 0;
    size_t visible = 0;

    for(;;) {
        int retval = fat_dirent_read(context->fs_ctx, context->first_cluster, &offset, &entry);

        if(retval < 0) {
            return retval;
        }

        if(entry.base.attribute & FAT_ATTR_VOLUME_ID) {
            continue;
        }

        if(visible < index) {
            visible += 1;
            continue;
        }

        if(entry.base.attribute & FAT_ATTR_DIRECTORY) {
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

static void make_83name(const char* restrict name, uint8_t out[11])
{
    memset(out, ' ', 11);

    const char* dot = nullptr;

    for(const char* p = name; *p; p++) {
        if(*p == '.') {
            dot = p;
        }
    }

    unsigned ni = 0;

    for(const char* p = name; *p && p != dot && ni < 8; p++, ni++) {
        out[ni] = (uint8_t)toupper((unsigned char)*p);
    }

    if(dot) {
        unsigned ei = 8;

        for(const char* p = dot + 1; *p && ei < 11; p++, ei++) {
            out[ei] = (uint8_t)toupper((unsigned char)*p);
        }
    }
}

static int fat_vnode_create(struct vnode* restrict node, const char* restrict name, unsigned type, struct vnode** restrict out)
{
    struct fat_vnode_context* context = node->ctxdata;
    struct fat_vfs_context* fs = context->fs_ctx;

    if(fs->param_readonly) {
        return -EROFS;
    }

    size_t name_len = strnlen(name, PATH_NAME_SIZE);
    unsigned num_lfn = (name_len + 12) / 13;
    unsigned slots = num_lfn + 1;

    size_t slot_offset;
    int retval = fat_dirent_find_free(fs, context->first_cluster, slots, &slot_offset);

    if(retval < 0) {
        return retval;
    }

    uintptr_t first_cluster = fat_cluster_alloc(fs);

    if(first_cluster == UINTPTR_MAX) {
        return -ENOSPC;
    }

    struct fat_dirent dirent;
    memset(&dirent, 0, sizeof(struct fat_dirent));

    make_83name(name, dirent.base.name);
    kstrncpy(dirent.full_name, name, PATH_NAME_SIZE);

    dirent.base.attribute = (type == VNODE_DIRECTORY) ? FAT_ATTR_DIRECTORY : FAT_ATTR_ARCHIVE;
    dirent.base.first_cluster_hi = (uint16_t)(first_cluster >> 16);
    dirent.base.first_cluster_lo = (uint16_t)(first_cluster & 0xFFFF);
    dirent.base.file_size = 0;

    retval = fat_dirent_write(fs, context->first_cluster, slot_offset + num_lfn, &dirent);

    if(retval < 0) {
        fat_cluster_free(fs, first_cluster);
        return retval;
    }

    struct fat_vnode_context* out_ctx = slab_alloc(sizeof(struct fat_vnode_context));

    if(out_ctx == nullptr) {
        fat_cluster_free(fs, first_cluster);
        return -ENOMEM;
    }

    out_ctx->fs_ctx = fs;
    out_ctx->first_cluster = first_cluster;
    out_ctx->size_bytes = 0;
    out_ctx->dir_cluster = context->first_cluster;
    out_ctx->dir_offset = slot_offset + num_lfn;

    struct vnode* out_node = vnode_alloc(type, &fat_vnode_ops, out_ctx);

    if(out_node == nullptr) {
        slab_free(out_ctx);
        fat_cluster_free(fs, first_cluster);
        return -ENOMEM;
    }

    out[0] = out_node;

    return 0;
}

static ssize_t fat_vnode_read(struct vnode* restrict node, void* restrict buffer, size_t sz, uintptr_t offset)
{
    struct fat_vnode_context* context = node->ctxdata;
    struct fat_vfs_context* fs = context->fs_ctx;

    if(offset >= context->size_bytes) {
        return 0;
    }

    size_t remaining = sz;

    if(offset + sz > context->size_bytes) {
        remaining = context->size_bytes - offset;
    }

    uintptr_t cluster = context->first_cluster;
    size_t cluster_index = offset / fs->cluster_size_b;

    for(size_t i = 0; i < cluster_index; i++) {
        cluster = fat_cluster_next(fs, cluster);

        if(cluster == UINTPTR_MAX) {
            return -EIO;
        }
    }

    size_t offset_in_cluster = offset % fs->cluster_size_b;
    size_t done = 0;

    while(remaining > 0 && cluster != UINTPTR_MAX) {
        size_t sector_index = offset_in_cluster / fs->sector_size;
        size_t offset_in_sector = offset_in_cluster % fs->sector_size;

        while(remaining > 0 && sector_index < fs->cluster_size_s) {
            uintptr_t lba = sector_index + fat_cluster_lba(fs, cluster);
            int retval = blk_device_read(fs->device, lba, 1, fs->bounce);

            if(retval < 0) {
                if(done > 0)
                    return done;
                return retval;
            }

            size_t chunk = fs->sector_size - offset_in_sector;

            if(chunk > remaining) {
                chunk = remaining;
            }

            memcpy((uint8_t*)(buffer) + done, fs->bounce + offset_in_sector, chunk);

            done += chunk;
            remaining -= chunk;
            offset_in_sector = 0;
            sector_index += 1;
        }

        offset_in_cluster = 0;
        cluster = fat_cluster_next(fs, cluster);
    }

    return done;
}

static ssize_t fat_vnode_write(struct vnode* restrict node, const void* restrict buffer, size_t sz, uintptr_t offset)
{
    struct fat_vnode_context* context = node->ctxdata;
    struct fat_vfs_context* fs = context->fs_ctx;

    if(fs->param_readonly) {
        return -EROFS;
    }

    uintptr_t cluster = context->first_cluster;
    size_t cluster_index = offset / fs->cluster_size_b;

    for(size_t i = 0; i < cluster_index; i++) {
        uintptr_t next = fat_cluster_next(fs, cluster);

        if(next == UINTPTR_MAX) {
            next = fat_cluster_alloc(fs);

            if(next == UINTPTR_MAX) {
                return -ENOSPC;
            }

            fat_cluster_link(fs, cluster, next);
        }

        cluster = next;
    }

    size_t offset_in_cluster = offset % fs->cluster_size_b;
    size_t done = 0;
    size_t remaining = sz;

    while(remaining > 0) {
        size_t sector_index = offset_in_cluster / fs->sector_size;
        size_t offset_in_sector = offset_in_cluster % fs->sector_size;

        while(remaining > 0 && sector_index < fs->cluster_size_s) {
            uintptr_t lba = fat_cluster_lba(fs, cluster) + sector_index;
            size_t chunk = fs->sector_size - offset_in_sector;

            if(chunk > remaining) {
                chunk = remaining;
            }

            int retval;

            if(offset_in_sector > 0 || chunk < fs->sector_size) {
                retval = blk_device_read(fs->device, lba, 1, fs->bounce);

                if(retval < 0) {
                    return done > 0 ? (ssize_t)done : retval;
                }
            }

            memcpy(fs->bounce + offset_in_sector, (const uint8_t*)buffer + done, chunk);

            retval = blk_device_write(fs->device, lba, 1, fs->bounce);

            if(retval < 0) {
                if(done > 0)
                    return done;
                return retval;
            }

            done += chunk;
            remaining -= chunk;
            offset_in_sector = 0;
            sector_index += 1;
        }

        if(remaining > 0) {
            uintptr_t next = fat_cluster_next(fs, cluster);

            if(next == UINTPTR_MAX) {
                next = fat_cluster_alloc(fs);

                if(next == UINTPTR_MAX) {
                    if(done > 0)
                        return done;
                    return -ENOSPC;
                }

                fat_cluster_link(fs, cluster, next);
            }

            cluster = next;
            offset_in_cluster = 0;
        }
    }

    if(offset + sz > context->size_bytes) {
        context->size_bytes = offset + sz;

        struct fat_dirent dirent;
        size_t dir_off = context->dir_offset;

        fat_dirent_read(context->fs_ctx, context->dir_cluster, &dir_off, &dirent);

        dirent.base.file_size = context->size_bytes;

        fat_dirent_update(context->fs_ctx, context->dir_cluster, context->dir_offset, &dirent.base);
    }

    return done;
}

static int fat_vnode_stat(struct vnode* restrict node, struct __k_stat* restrict out)
{
    struct fat_vnode_context* context = node->ctxdata;
    struct fat_vfs_context* fs = context->fs_ctx;

    memset(out, 0, sizeof(struct __k_stat));

    out->st_uid = fs->param_uid;
    out->st_gid = fs->param_gid;
    out->st_size = context->size_bytes;
    out->st_blksize = fs->cluster_size_b;

    out->st_blocks = context->size_bytes + fs->sector_size - 1;
    out->st_blocks /= fs->sector_size;

    mode_t rw_bits = S_IRUSR | S_IRGRP | S_IROTH;

    if(!fs->param_readonly) {
        rw_bits |= S_IWUSR | S_IWGRP;
    }

    if(node->nodetype == VNODE_DIRECTORY) {
        out->st_mode = S_IFDIR | rw_bits | S_IXUSR | S_IXGRP | S_IXOTH;
    }
    else {
        out->st_mode = S_IFREG | rw_bits;
    }

    return 0;
}
