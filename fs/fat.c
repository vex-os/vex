#include <core/endian.h>
#include <core/errno.h>
#include <dev/blk_device.h>
#include <fs/vfs_mount.h>
#include <fs/vfs_node.h>
#include <fs/vfs_types.h>
#include <init/initcall.h>
#include <lib/ctype.h>
#include <lib/string.h>
#include <lib/strings.h>
#include <mm/malloc.h>
#include <mm/slab.h>

enum {
    FAT_TYPE_12 = 12,
    FAT_TYPE_16 = 16,
    FAT_TYPE_32 = 32,
};

struct fat_bpb {
    uint8_t jmp_boot[3];
    uint8_t oem_name[8];
    uint16_t sector_size; // should be equal to blk_device::sector_size, probably...
    uint8_t cluster_size; // in sectors
    uint16_t reserved_sectors;
    uint8_t table_count;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t media_type;
    uint16_t fat_size_16; // in sectors
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
} __packed;

#ifndef __INTELLISENSE__
static_assert(36 == sizeof(struct fat_bpb));
#endif

struct fat1x_ebr {
    struct fat_bpb base;
    uint8_t drive_number;
    uint8_t winnt_reserved;
    uint8_t signature;
    uint32_t volume_id;
    uint8_t volume_label[11];
    uint8_t system_identifier[8];
    uint8_t whatever_else[448];
    uint8_t mbr_signature[2];
} __packed;

struct fat32_ebr {
    struct fat_bpb base;
    uint32_t fat_size;
    uint16_t flags;
    uint8_t version[2];
    uint32_t root_cluster;
    uint16_t fsinfo_sector;
    uint16_t backup_boot_sector;
    uint8_t reserved[12];
    uint8_t drive_number;
    uint8_t winnt_reserved;
    uint8_t signature;
    uint32_t volume_id;
    uint8_t volume_label[11];
    uint8_t system_identifier[8];
    uint8_t whatever_else[420];
    uint8_t mbr_signature[2];
} __packed;

#ifndef __INTELLISENSE__
static_assert(512 == sizeof(struct fat1x_ebr));
static_assert(512 == sizeof(struct fat32_ebr));
#endif

union fat_ebr {
    struct fat1x_ebr ebr1x;
    struct fat32_ebr ebr32;
};

struct fat32_fsinfo {
    uint32_t signature_1;
    uint8_t reserved_1[480];
    uint32_t signature_2;
    uint32_t free_clusters;
    uint32_t next_free_cluster;
    uint8_t reserved_2[12];
    uint32_t signature_3;
} __packed;

#ifndef __INTELLISENSE__
static_assert(512 == sizeof(struct fat32_fsinfo));
#endif

#define FAT_ATTR_READONLY  UINT8_C(0x01)
#define FAT_ATTR_HIDDEN    UINT8_C(0x02)
#define FAT_ATTR_SYSTEM    UINT8_C(0x04)
#define FAT_ATTR_VOLUME_ID UINT8_C(0x08)
#define FAT_ATTR_DIRECTORY UINT8_C(0x10)
#define FAT_ATTR_ARCHIVE   UINT8_C(0x20)
#define FAT_ATTR_LFN       UINT8_C(0x0F)

struct fat_dirent_83 {
    uint8_t name[11];
    uint8_t attributes;
    uint8_t winnt_reserved;
    uint8_t create_time_dsec;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t cluster_high;
    uint16_t modify_time;
    uint16_t modify_date;
    uint16_t cluster_low;
    uint32_t size;
} __packed;

struct fat_dirent_lfn {
    uint8_t order;
    uint16_t name_1[5];
    uint8_t attributes;
    uint8_t type;
    uint8_t checksum;
    uint16_t name_2[6];
    uint16_t reserved;
    uint16_t name_3[2];
} __packed;

#ifndef __INTELLISENSE__
static_assert(32 == sizeof(struct fat_dirent_83));
static_assert(32 == sizeof(struct fat_dirent_lfn));
#endif

struct fat_ctx {
    const struct blk_device* device;
    unsigned fat_type;
    uintptr_t fat_start_lba;
    uintptr_t data_start_lba;
    uintptr_t root_cluster_fat32;
    size_t root_dir_sectors_fat1x;
    size_t cluster_size_bytes;
    size_t cluster_size_sectors;
    size_t sector_size;
    void* fat_cache;
    size_t fat_cache_size; // in bytes
};

struct fat_node_ctx {
    struct fat_ctx* fs;
    uint32_t first_cluster; // zero for fat1x root dir
    uint32_t size_bytes;
};

static uint32_t next_cluster(const struct fat_ctx* restrict ctx, uint32_t cluster)
{
    const uint8_t* table = ctx->fat_cache;
    uint16_t value_1x;
    uint32_t value_32;
    uint32_t offset;

    if(ctx->fat_type == FAT_TYPE_12) {
        offset = cluster + cluster / 2;
        memcpy(&value_1x, table + offset, sizeof(value_1x));
        value_1x = LE16_to_host(value_1x);

        if(cluster & 1) {
            value_1x >>= 4;
        }
        else {
            value_1x &= 0x0FFF;
        }

        if(value_1x >= 0xFF8) {
            return UINT32_MAX;
        }

        return value_1x;
    }

    if(ctx->fat_type == FAT_TYPE_16) {
        offset = cluster * 2;
        memcpy(&value_1x, table + offset, sizeof(value_1x));
        value_1x = LE16_to_host(value_1x);

        if(value_1x >= 0xFFF8) {
            return UINT32_MAX;
        }

        return value_1x;
    }

    if(ctx->fat_type == FAT_TYPE_32) {
        offset = cluster * 4;
        memcpy(&value_32, table + offset, sizeof(value_32));
        value_32 = LE32_to_host(value_32) & UINT32_C(0x0FFFFFFF);

        if(value_32 >= 0x0FFFFFF8) {
            return UINT32_MAX;
        }

        return value_32;
    }

    return UINT32_MAX;
}

static __always_inline inline uintptr_t cluster_to_lba(const struct fat_ctx* restrict ctx, uint32_t cluster)
{
    return ctx->data_start_lba + (uintptr_t)(cluster - 2) * ctx->cluster_size_sectors;
}

static int read_dirent(const struct fat_ctx* restrict ctx, uint32_t first_cluster, off_t index, struct fat_dirent_83* restrict out)
{
    uint8_t* bounce; // FIXME: kmalloc is slow

    int retval;
    size_t cluster_index;
    size_t entries_count;
    size_t entry_in_cluster;
    size_t entry_in_sector;
    size_t i;
    size_t index_sizetype;
    size_t sector_in_cluster;
    size_t sector_in_root;
    uint32_t cluster;
    uintptr_t lba;

    entries_count = ctx->cluster_size_bytes / 32;
    index_sizetype = (size_t)(index);

    bounce = kmalloc(ctx->sector_size); // FIXME: kmalloc is slow

    if(bounce == nullptr) {
        return -ENOMEM;
    }

    if(first_cluster == 0) {
        sector_in_root = index_sizetype / entries_count;

        if(sector_in_root >= ctx->root_dir_sectors_fat1x) {
            kfree(bounce);
            return -ENOENT;
        }

        lba = ctx->data_start_lba - ctx->root_dir_sectors_fat1x + sector_in_root;
        retval = blk_device_read(ctx->device, lba, 1, bounce);

        if(retval < 0) {
            kfree(bounce);
            return retval;
        }

        memcpy(out, bounce + (index_sizetype % entries_count) * 32, 32);
        kfree(bounce);
        return 0;
    }

    entries_count = ctx->cluster_size_bytes / 32;
    cluster_index = index_sizetype / entries_count;
    entry_in_cluster = index_sizetype % entries_count;
    sector_in_cluster = (entry_in_cluster * 32) / ctx->sector_size;
    entry_in_sector = entry_in_cluster % entries_count;

    cluster = first_cluster;

    for(i = 0; i < cluster_index; i++) {
        cluster = next_cluster(ctx, cluster);

        if(cluster == UINT32_MAX) {
            kfree(bounce);
            return -ENOENT;
        }
    }

    lba = cluster_to_lba(ctx, cluster) + sector_in_cluster;
    retval = blk_device_read(ctx->device, lba, 1, bounce);

    if(retval < 0) {
        kfree(bounce);
        return retval;
    }

    memcpy(out, bounce + entry_in_sector * 32, 32);
    kfree(bounce);
    return 0;
}

static void dirent_83_name(const struct fat_dirent_83* restrict ent, char* restrict out)
{
    size_t i;
    size_t pos = 0;

    for(i = 0; i < 8 && !isspace(ent->name[i]); i++) {
        out[pos++] = ent->name[i];
    }

    if(!isspace(ent->name[8])) {
        out[pos++] = '.';

        for(i = 8; i < 11 && !isspace(ent->name[i]); i++) {
            out[pos++] = ent->name[i];
        }
    }

    out[pos] = 0x00;
}

static void fat_node_destroy(struct vfs_node* restrict node);
static int fat_node_lookup(struct vfs_node* restrict dir, const char* restrict name, struct vfs_node** restrict out);
static int fat_node_read_dir(struct vfs_node* restrict dir, struct vfs_dirent* out_dirent, off_t index);
static ssize_t fat_node_read(struct vfs_node* restrict node, void* restrict buffer, size_t sz, off_t offset);
static int fat_node_stat(struct vfs_node* restrict node, struct vfs_stat* restrict out_stat);

static const struct vfs_node_ops fat_node_ops = {
    .destroy = &fat_node_destroy,
    .lookup = &fat_node_lookup,
    .read_dir = &fat_node_read_dir,
    .read = &fat_node_read,
    .stat = &fat_node_stat,
};

static void fat_node_destroy(struct vfs_node* restrict node)
{
    slab_free(node->ctxdata);
}

static int fat_node_lookup(struct vfs_node* restrict dir, const char* restrict name, struct vfs_node** restrict out)
{
    struct fat_node_ctx* ctx = dir->ctxdata;

    char canonical_name[13];
    int retval;
    off_t index;
    struct fat_dirent_83 entry;
    struct fat_node_ctx* subctx;
    struct vfs_node* node;
    uint32_t cluster;
    unsigned type;

    index = 0;

    for(;;) {
        retval = read_dirent(ctx->fs, ctx->first_cluster, index++, &entry);

        if(retval < 0) {
            return retval;
        }

        if(entry.name[0] == 0x00) {
            return -ENOENT;
        }

        if(entry.name[0] == 0xE5) {
            continue; // deleted entry
        }

        if((entry.attributes & FAT_ATTR_LFN) == FAT_ATTR_LFN) {
            continue;
        }

        if(entry.attributes & FAT_ATTR_VOLUME_ID) {
            continue;
        }

        dirent_83_name(&entry, canonical_name);

        if(strncasecmp(canonical_name, name, sizeof(canonical_name))) {
            continue;
        }

        cluster = UINT32_C(0);
        cluster |= (uint32_t)(entry.cluster_high) << 16;
        cluster |= (uint32_t)(entry.cluster_low);

        if(entry.attributes & FAT_ATTR_DIRECTORY) {
            type = VFS_NODE_TYPE_DIRECTORY;
        }
        else {
            type = VFS_NODE_TYPE_FILE;
        }

        subctx = slab_alloc(sizeof(struct fat_node_ctx));

        if(subctx == nullptr) {
            return -ENOMEM;
        }

        subctx->fs = ctx->fs;
        subctx->first_cluster = cluster;
        subctx->size_bytes = entry.size;

        node = vfs_node_alloc(type, &fat_node_ops, subctx);

        if(node == nullptr) {
            slab_free(subctx);
            return -ENOMEM;
        }

        out[0] = node;

        return 0;
    }

    unreachable();
}

static int fat_node_read_dir(struct vfs_node* restrict dir, struct vfs_dirent* out_dirent, off_t index)
{
    struct fat_node_ctx* ctx = dir->ctxdata;

    int retval;
    off_t raw;
    off_t visible;
    struct fat_dirent_83 entry;

    visible = 0;
    raw = 0;

    for(;;) {
        retval = read_dirent(ctx->fs, ctx->first_cluster, raw++, &entry);

        if(retval < 0) {
            return retval;
        }

        if(entry.name[0] == 0x00) {
            return -ENOENT;
        }

        if(entry.name[0] == 0xE5) {
            continue; // deleted entry
        }

        if((entry.attributes & FAT_ATTR_LFN) == FAT_ATTR_LFN) {
            continue;
        }

        if(entry.attributes & FAT_ATTR_VOLUME_ID) {
            continue;
        }

        if(visible++ < index) {
            continue;
        }

        dirent_83_name(&entry, out_dirent->name);

        if(entry.attributes & FAT_ATTR_DIRECTORY) {
            out_dirent->type = VFS_NODE_TYPE_DIRECTORY;
        }
        else {
            out_dirent->type = VFS_NODE_TYPE_FILE;
        }

        return 0;
    }

    unreachable();
}

static int fat_node_stat(struct vfs_node* restrict node, struct vfs_stat* restrict out_stat)
{
    struct fat_node_ctx* nctx = node->ctxdata;

    memset(out_stat, 0, sizeof(struct vfs_stat));

    out_stat->size = nctx->size_bytes;
    out_stat->type = node->type;
    out_stat->block_size = (blksize_t)nctx->fs->cluster_size_bytes;
    out_stat->blocks = (blkcnt_t)((nctx->size_bytes + nctx->fs->cluster_size_bytes - 1) / nctx->fs->cluster_size_bytes);

    return 0;
}

static ssize_t fat_node_read(struct vfs_node* restrict node, void* restrict buffer, size_t sz, off_t offset)
{
    struct fat_node_ctx* node_ctx = node->ctxdata;
    const struct fat_ctx* ctx = node_ctx->fs;

    uint8_t* sector_buf; // FIXME: kmalloc is slow

    int retval;
    size_t byte_index;
    size_t bytes_left;
    size_t bytes_to_copy;
    size_t chunk;
    size_t copied;
    size_t done;
    size_t i;
    size_t offset_in_cluster;
    size_t remaining;
    size_t sector_index;
    size_t start_cluster;
    uint32_t cluster;
    uintptr_t base_lba;

    remaining = sz;
    done = 0;

    if((size_t)(offset) >= node_ctx->size_bytes) {
        return 0;
    }

    if((size_t)(offset) + sz > node_ctx->size_bytes) {
        remaining = node_ctx->size_bytes - (size_t)offset;
    }

    start_cluster = (size_t)(offset) / ctx->cluster_size_bytes;
    offset_in_cluster = (size_t)(offset) % ctx->cluster_size_bytes;
    cluster = node_ctx->first_cluster;

    for(i = 0; i < start_cluster; ++i) {
        cluster = next_cluster(ctx, cluster);

        if(cluster == UINT32_MAX) {
            return (ssize_t)(done);
        }
    }

    sector_buf = kmalloc(ctx->sector_size); // FIXME: kmalloc is slow

    if(sector_buf == nullptr) {
        return -ENOMEM;
    }

    while(remaining > 0 && cluster < UINT32_MAX) {
        bytes_left = node_ctx->size_bytes - offset_in_cluster;

        if(remaining < bytes_left) {
            bytes_to_copy = remaining;
        }
        else {
            bytes_to_copy = bytes_left;
        }

        base_lba = cluster_to_lba(ctx, cluster);
        sector_index = offset_in_cluster / ctx->sector_size;
        byte_index = offset_in_cluster % ctx->sector_size;
        copied = 0;

        while(copied < bytes_to_copy) {
            retval = blk_device_read(ctx->device, base_lba + sector_index, 1, sector_buf);

            if(retval < 0) {
                kfree(sector_buf);

                if(done > 0) {
                    return (ssize_t)(done);
                }

                return retval;
            }

            chunk = ctx->sector_size - byte_index;

            if(chunk > bytes_to_copy - copied) {
                chunk = bytes_to_copy - copied;
            }

            memcpy((uint8_t*)(buffer) + done, sector_buf + byte_index, chunk);

            done += chunk;
            copied += chunk;
            remaining -= chunk;
            sector_index += 1;
            byte_index = 0;
        }

        cluster = next_cluster(ctx, cluster);
        offset_in_cluster = 0;
    }

    kfree(sector_buf);

    return (ssize_t)(done);
}

static int fat_sync(struct vfs* restrict fs)
{
    struct fat_ctx* ctx = fs->ctxdata;

    int retval;
    size_t fat_size = ctx->fat_cache_size / ctx->sector_size;
    size_t i;

    for(i = 0; i < fat_size; i++) {
        retval = blk_device_write(ctx->device, ctx->fat_start_lba + i, 1, (uint8_t*)(ctx->fat_cache) + i * ctx->sector_size);

        if(retval < 0) {
            return retval;
        }
    }

    return blk_device_flush(ctx->device);
}

static int fat_mount(struct vfs* restrict fs, const struct blk_device* restrict device, const struct cmdline* restrict params)
{
    int retval;
    size_t i;
    struct fat_bpb* bpb;
    struct fat_ctx* ctx;
    struct fat_node_ctx* root_ctx;
    struct vfs_node* root;
    uint32_t cluster_count;
    uint32_t data_start;
    uint32_t fat_size;
    uint32_t root_dir_sectors;
    uint32_t total_sectors;
    union fat_ebr* boot;
    unsigned fat_type;

    if(device == nullptr) {
        return -EINVAL;
    }

    boot = kmalloc(sizeof(union fat_ebr));

    if(boot == nullptr) {
        return -ENOMEM;
    }

    retval = blk_device_read(device, 0, 1, boot);

    if(retval < 0) {
        kfree(boot);
        return retval;
    }

    bpb = &boot->ebr1x.base;

    if(bpb->sector_size != (uint16_t)(device->sector_size)) {
        return -EINVAL;
    }

    if(bpb->cluster_size == 0) {
        return -EINVAL;
    }

    if(bpb->fat_size_16) {
        fat_size = bpb->fat_size_16;
    }
    else {
        fat_size = boot->ebr32.fat_size;
    }

    if(bpb->total_sectors_16) {
        total_sectors = bpb->total_sectors_16;
    }
    else {
        total_sectors = bpb->total_sectors_32;
    }

    root_dir_sectors = ((uint32_t)bpb->root_entry_count * 32 + bpb->sector_size - 1) / bpb->sector_size;
    data_start = bpb->reserved_sectors + (uint32_t)(bpb->table_count) * fat_size + root_dir_sectors;
    cluster_count = (total_sectors - data_start) / bpb->cluster_size;

    if(cluster_count < 4085) {
        fat_type = FAT_TYPE_12;
    }
    else if(cluster_count < 65525) {
        fat_type = FAT_TYPE_16;
    }
    else {
        fat_type = FAT_TYPE_32;
    }

    ctx = kmalloc(sizeof(struct fat_ctx));

    if(ctx == nullptr) {
        kfree(boot);
        return -ENOMEM;
    }

    memset(ctx, 0, sizeof(struct fat_ctx));

    ctx->device = device;
    ctx->fat_type = fat_type;
    ctx->fat_start_lba = bpb->reserved_sectors;
    ctx->data_start_lba = data_start;
    ctx->sector_size = bpb->sector_size;
    ctx->cluster_size_sectors = bpb->cluster_size;
    ctx->cluster_size_bytes = (size_t)(bpb->cluster_size) * bpb->sector_size;

    if(fat_type == FAT_TYPE_32) {
        ctx->root_cluster_fat32 = boot->ebr32.root_cluster;
        ctx->root_dir_sectors_fat1x = 0;
    }
    else {
        ctx->root_cluster_fat32 = 0;
        ctx->root_dir_sectors_fat1x = root_dir_sectors;
    }

    ctx->fat_cache_size = (size_t)fat_size * bpb->sector_size;
    ctx->fat_cache = kmalloc(ctx->fat_cache_size);

    if(ctx->fat_cache == nullptr) {
        kfree(boot);
        kfree(ctx);
        return -ENOMEM;
    }

    for(i = 0; i < fat_size; i++) {
        retval = blk_device_read(device, ctx->fat_start_lba + i, 1, (uint8_t*)(ctx->fat_cache) + i * bpb->sector_size);

        if(retval < 0) {
            kfree(boot);
            kfree(ctx->fat_cache);
            kfree(ctx);
            return retval;
        }
    }

    kfree(boot);

    root_ctx = slab_alloc(sizeof(struct fat_node_ctx));

    if(root_ctx == nullptr) {
        kfree(ctx->fat_cache);
        kfree(ctx);
        return -ENOMEM;
    }

    memset(root_ctx, 0, sizeof(struct fat_node_ctx));

    root_ctx->fs = ctx;
    root_ctx->first_cluster = (uint32_t)(ctx->root_cluster_fat32);

    root = vfs_node_alloc(VFS_NODE_TYPE_DIRECTORY, &fat_node_ops, root_ctx);

    if(root == nullptr) {
        slab_free(root_ctx);
        kfree(ctx->fat_cache);
        kfree(ctx);
        return -ENOMEM;
    }

    fs->root = root;
    fs->ctxdata = ctx;

    return 0;
}

static int fat_unmount(struct vfs* restrict fs)
{
    struct fat_ctx* ctx = fs->ctxdata;
    fat_sync(fs);
    vfs_node_dec_refcount(fs->root);
    kfree(ctx->fat_cache);
    kfree(ctx);
    return 0;
}

static const struct vfs_ops fatfs_ops = {
    .mount = &fat_mount,
    .unmount = &fat_unmount,
    .sync = &fat_sync,
};

static struct vfs_driver fatfs_driver = {
    .identifier = "fat",
    .ops = &fatfs_ops,
};

define_initcall(fat_init, INITCALL_STAGE_BSP_VFS, INITCALL_ORDER_EARLY)
{
    vfs_driver_register(&fatfs_driver);
}
