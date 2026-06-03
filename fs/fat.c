#include <core/errno.h>
#include <dev/blk_device.h>
#include <fs/vfs_mount.h>
#include <fs/vfs_node.h>
#include <fs/vfs_types.h>
#include <init/initcall.h>
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

static int fat_mount(struct vfs* restrict fs, const struct blk_device* restrict device, const struct cmdline* restrict params)
{
    union fat_ebr* boot;
    struct fat_bpb* bpb;
    struct fat_ctx* ctx;
    struct fat_node_ctx* root_ctx;
    struct vfs_node* root;
    uint32_t fat_size;
    uint32_t total_sectors;
    uint32_t root_dir_sectors;
    uint32_t data_start;
    uint32_t cluster_count;
    unsigned fat_type;
    int retval;
    size_t i;

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
        return -EINVAL; // we're fucked!
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
            pr_error("failed to read FAT from disk");
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

    root = vfs_node_alloc(VFS_NODE_TYPE_DIRECTORY, nullptr, root_ctx);

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

static int fat_sync(struct vfs* restrict fs)
{
    struct fat_ctx* ctx = fs->ctxdata;
    size_t fat_size = ctx->fat_cache_size / ctx->sector_size;
    int retval;
    size_t i;

    for(i = 0; i < fat_size; i++) {
        retval = blk_device_write(ctx->device, ctx->fat_start_lba + i, 1, (uint8_t*)(ctx->fat_cache) + i * ctx->sector_size);

        if(retval < 0) {
            return retval;
        }
    }

    return blk_device_flush(ctx->device);
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
