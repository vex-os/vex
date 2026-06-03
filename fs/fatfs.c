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
    fatfs_TYPE_12 = 12,
    fatfs_TYPE_16 = 16,
    fatfs_TYPE_32 = 32,
};

struct fatfs_bpb {
    uint8_t jmp_boot[3];
    uint8_t oem_name[8];
    uint16_t bytes_per_sec; // should be equal to blk_device::sector_size, or mount fails
    uint8_t sec_per_clus;
    uint16_t rsvd_sec_cnt;
    uint8_t num_fats;
    uint16_t root_ent_cnt;
    uint16_t tot_sec16;
    uint8_t media;
    uint16_t fatfs_sz16;
    uint16_t sec_per_trk;
    uint16_t num_heads;
    uint32_t hidd_sec;
    uint32_t tot_sec32;
} __packed;

#ifndef __INTELLISENSE__
static_assert(36 == sizeof(struct fatfs_bpb));
#endif

struct fatfs_ebr_1x {
    struct fatfs_bpb bpb;
    uint8_t drv_num;
    uint8_t reserved_1;
    uint8_t boot_sig;
    uint32_t vol_id;
    uint8_t vol_lab[11];
    uint8_t fil_sys_type[8];
    uint8_t reserved_2[448];
    uint8_t mbr_sig[2];
} __packed;

struct fatfs_ebr_32 {
    struct fatfs_bpb bpb;
    uint32_t fatfs_sz32;
    uint16_t ext_flags;
    uint16_t fs_ver;
    uint32_t root_clus;
    uint16_t fs_info;
    uint16_t bk_boot_sec;
    uint8_t reserved[12];
    uint8_t drv_num;
    uint8_t reserved_1;
    uint8_t boot_sig;
    uint32_t vol_id;
    uint8_t vol_lab[11];
    uint8_t fil_sys_type[8];
    uint8_t reserved_2[420];
    uint8_t mbr_sig[2];
} __packed;

#ifndef __INTELLISENSE__
static_assert(512 == sizeof(struct fatfs_ebr_1x));
static_assert(512 == sizeof(struct fatfs_ebr_32));
#endif

union fatfs_ebr {
    struct fatfs_ebr_1x ebr1x;
    struct fatfs_ebr_32 ebr32;
};

struct fatfs_fsinfo {
    uint32_t lead_sig;
    uint8_t reserved_1[480];
    uint32_t struc_sig;
    uint32_t free_count;
    uint32_t nxt_free;
    uint8_t reserved_2[12];
    uint32_t trail_sig;
} __packed;

#ifndef __INTELLISENSE__
static_assert(512 == sizeof(struct fatfs_fsinfo));
#endif

#define FATFS_ATTR_READ_ONLY UINT8_C(0x01)
#define FATFS_ATTR_HIDDEN    UINT8_C(0x02)
#define FATFS_ATTR_SYSTEM    UINT8_C(0x04)
#define FATFS_ATTR_VOLUME_ID UINT8_C(0x08)
#define FATFS_ATTR_DIRECTORY UINT8_C(0x10)
#define FATFS_ATTR_ARCHIVE   UINT8_C(0x20)
#define FATFS_ATTR_LONG_NAME UINT8_C(0x0F)

#define FATFS_LFN_SEGMENTS 20

struct fatfs_dirent {
    uint8_t name[11];
    uint8_t attr;
    uint8_t ntres;
    uint8_t crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t lst_acc_date;
    uint16_t fst_clus_hi;
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t fst_clus_lo;
    uint32_t file_size;
} __packed;

struct fatfs_long_dirent {
    uint8_t ord;
    uint16_t name_1[5];
    uint8_t attr;
    uint8_t type;
    uint8_t chksum;
    uint16_t name_2[6];
    uint16_t fst_clus_lo;
    uint16_t name_3[2];
} __packed;

#ifndef __INTELLISENSE__
static_assert(32 == sizeof(struct fatfs_dirent));
static_assert(32 == sizeof(struct fatfs_long_dirent));
#endif

struct fatfs_vfs_context {
    const struct blk_device* device;
    unsigned filesystem_type;
    uintptr_t table_start_lba;
    uintptr_t data_start_lba;
    uintptr_t root_cluster_fat32;
    size_t root_dir_sectors_fat1x;
    size_t cluster_size_bytes;
    size_t cluster_size_sectors;
    size_t sector_size;
    void* fatfs_cache;
    size_t fatfs_cache_sz;
    void* bounce;
};

struct fatfs_node_context {
    struct fatfs_vfs_context* fs_ctx;
    uint32_t first_cluster; // zero for fat1x root directory
    uint32_t size_bytes;
};

static __always_inline inline uintptr_t cluster_to_lba(const struct fatfs_vfs_context* restrict ctx, uint32_t cluster)
{
    return ctx->data_start_lba + (uintptr_t)(cluster - 2) * ctx->cluster_size_sectors;
}

static uint32_t next_cluster(const struct fatfs_vfs_context* restrict ctx, uint32_t cluster)
{
    const uint8_t* table = ctx->fatfs_cache;

    uint16_t value_1x;
    uint32_t value_32;
    uint32_t offset;

    if(ctx->filesystem_type == fatfs_TYPE_12) {
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

    if(ctx->filesystem_type == fatfs_TYPE_16) {
        offset = cluster * 2;
        memcpy(&value_1x, table + offset, sizeof(value_1x));
        value_1x = LE16_to_host(value_1x);

        if(value_1x >= 0xFFF8) {
            return UINT32_MAX;
        }

        return value_1x;
    }

    if(ctx->filesystem_type == fatfs_TYPE_32) {
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

static int read_dirent_internal(const struct fatfs_vfs_context* restrict context, uint32_t first_clu, off_t offset,
    struct fatfs_dirent* restrict out)
{
    uint8_t* bounce = context->bounce;

    int retval;
    size_t cluster_idx;
    size_t entries_count;
    size_t entry_idx_clu;
    size_t entry_idx_sec;
    size_t i;
    size_t offset_szt;
    size_t sec_in_clu;
    size_t sec_in_root;
    uint32_t cluster;

    entries_count = context->sector_size / 32;
    offset_szt = (size_t)(offset);

    if(first_clu == 0) {
        sec_in_root = offset_szt / entries_count;

        if(sec_in_root >= context->root_dir_sectors_fat1x) {
            return -ENOENT;
        }

        retval = blk_device_read(context->device, context->data_start_lba - context->root_dir_sectors_fat1x + sec_in_root, 1, bounce);

        if(retval < 0) {
            return retval;
        }

        memcpy(out, bounce + (offset_szt % entries_count) * 32, 32);

        return 0;
    }

    entries_count = context->cluster_size_bytes / 32;
    cluster_idx = offset_szt / entries_count;
    entry_idx_clu = offset_szt % entries_count;
    entry_idx_sec = entry_idx_clu % (context->sector_size / 32);
    sec_in_clu = (entry_idx_clu * 32) / context->sector_size;

    cluster = first_clu;

    for(i = 0; i < cluster_idx; i++) {
        cluster = next_cluster(context, cluster);

        if(cluster == UINT32_MAX) {
            return -ENOENT;
        }
    }

    retval = blk_device_read(context->device, cluster_to_lba(context, cluster) + sec_in_clu, 1, bounce);

    if(retval < 0) {
        return retval;
    }

    memcpy(out, bounce + entry_idx_sec * 32, 32);
    return 0;
}

static int read_dirent(const struct fatfs_vfs_context* restrict context, uint32_t first_clu, off_t* restrict offset,
    struct fatfs_dirent* restrict out, char* restrict out_name)
{
    const struct fatfs_long_dirent* segment;
    int i, j;
    int retval;
    int segment_count = 0;
    size_t pos;
    struct fatfs_dirent entry;
    struct fatfs_long_dirent segments[FATFS_LFN_SEGMENTS];
    uint16_t codepoint;

    for(;;) {
        retval = read_dirent_internal(context, first_clu, offset[0]++, &entry);

        if(retval < 0) {
            return retval;
        }

        if(entry.name[0] == 0x00) {
            return -ENOENT;
        }

        if(entry.name[0] == 0xE5) {
            segment_count = 0;
            continue;
        }

        if((entry.attr & FATFS_ATTR_LONG_NAME) == FATFS_ATTR_LONG_NAME) {
            if(segment_count < FATFS_LFN_SEGMENTS)
                memcpy(&segments[segment_count++], &entry, 32);
            continue;
        }

        if(entry.attr & FATFS_ATTR_VOLUME_ID) {
            segment_count = 0;
            continue;
        }

        memcpy(out, &entry, sizeof(struct fatfs_dirent));

        if(segment_count > 0) {
            pos = 0;

            for(i = segment_count - 1; i >= 0; --i) {
                segment = &segments[i];
                codepoint = 0;

                for(j = 0; j < 5; ++j) {
                    memcpy(&codepoint, &segment->name_1[j], 2);

                    if(pos < VFS_NAME_SIZE - 1) {
                        out_name[pos++] = (char)(codepoint & 0xFF);
                    }
                }

                for(j = 0; j < 6; ++j) {
                    memcpy(&codepoint, &segment->name_2[j], 2);

                    if(pos < VFS_NAME_SIZE - 1) {
                        out_name[pos++] = (char)(codepoint & 0xFF);
                    }
                }

                for(j = 0; j < 2; ++j) {
                    memcpy(&codepoint, &segment->name_3[j], 2);

                    if(pos < VFS_NAME_SIZE - 1) {
                        out_name[pos++] = (char)(codepoint & 0xFF);
                    }
                }
            }
        }
        else {
            pos = 0;

            for(i = 0; i < 8 && !isspace(entry.name[i]); i++) {
                out_name[pos++] = entry.name[i];
            }

            if(!isspace(entry.name[8])) {
                out_name[pos++] = '.';

                for(i = 8; i < 11 && !isspace(entry.name[i]); i++) {
                    out_name[pos++] = entry.name[i];
                }
            }
        }

        out_name[pos] = 0x00;

        return 0;
    }

    unreachable();
}

static void fatfs_node_destroy(struct vfs_node* restrict node);
static int fatfs_node_lookup(struct vfs_node* restrict dir, const char* restrict name, struct vfs_node** restrict out);
static int fatfs_node_read_dir(struct vfs_node* restrict dir, struct vfs_dirent* out_dirent, off_t index);
static ssize_t fatfs_node_read(struct vfs_node* restrict node, void* restrict buffer, size_t sz, off_t offset);
static int fatfs_node_stat(struct vfs_node* restrict node, struct vfs_stat* restrict out_stat);

static const struct vfs_node_ops fatfs_node_ops = {
    .destroy = &fatfs_node_destroy,
    .lookup = &fatfs_node_lookup,
    .read_dir = &fatfs_node_read_dir,
    .read = &fatfs_node_read,
    .stat = &fatfs_node_stat,
};

static void fatfs_node_destroy(struct vfs_node* restrict node)
{
    slab_free(node->ctxdata);
}

static int fatfs_node_lookup(struct vfs_node* restrict dir, const char* restrict name, struct vfs_node** restrict out)
{
    struct fatfs_node_context* context = dir->ctxdata;

    char entry_name[VFS_NAME_SIZE];
    int retval;
    off_t offset = 0;
    struct fatfs_dirent entry;
    struct fatfs_node_context* sub_context;
    struct vfs_node* sub_node;
    uint32_t cluster;
    unsigned type;

    for(;;) {
        retval = read_dirent(context->fs_ctx, context->first_cluster, &offset, &entry, entry_name);

        if(retval < 0) {
            return retval;
        }

        if(strcasecmp(entry_name, name)) {
            continue;
        }

        cluster = ((uint32_t)(entry.fst_clus_hi) << 16) | (uint32_t)(entry.fst_clus_lo);

        if(entry.attr & FATFS_ATTR_DIRECTORY) {
            type = VFS_NODE_TYPE_DIRECTORY;
        }
        else {
            type = VFS_NODE_TYPE_FILE;
        }

        sub_context = slab_alloc(sizeof(struct fatfs_node_context));

        if(sub_context == nullptr) {
            return -ENOMEM;
        }

        sub_context->fs_ctx = context->fs_ctx;
        sub_context->first_cluster = cluster;
        sub_context->size_bytes = entry.file_size;

        sub_node = vfs_node_alloc(type, &fatfs_node_ops, sub_context);

        if(sub_node == nullptr) {
            slab_free(sub_context);
            return -ENOMEM;
        }

        out[0] = sub_node;

        return 0;
    }

    unreachable();
}

static int fatfs_node_read_dir(struct vfs_node* restrict dir, struct vfs_dirent* out_dirent, off_t index)
{
    struct fatfs_node_context* context = dir->ctxdata;

    int retval;
    off_t offset;
    off_t visible;
    struct fatfs_dirent entry;

    visible = 0;
    offset = 0;

    for(;;) {
        retval = read_dirent(context->fs_ctx, context->first_cluster, &offset, &entry, out_dirent->name);

        if(retval < 0) {
            return retval;
        }

        if(visible++ < index) {
            continue;
        }

        if(entry.attr & FATFS_ATTR_DIRECTORY) {
            out_dirent->type = VFS_NODE_TYPE_DIRECTORY;
        }
        else {
            out_dirent->type = VFS_NODE_TYPE_FILE;
        }

        return 0;
    }

    unreachable();
}

static int fatfs_node_stat(struct vfs_node* restrict node, struct vfs_stat* restrict out_stat)
{
    struct fatfs_node_context* context = node->ctxdata;

    memset(out_stat, 0, sizeof(struct vfs_stat));

    out_stat->size = context->size_bytes;
    out_stat->type = node->type;
    out_stat->block_size = (blksize_t)context->fs_ctx->cluster_size_bytes;
    out_stat->blocks = (blkcnt_t)((context->size_bytes + context->fs_ctx->cluster_size_bytes - 1) / context->fs_ctx->cluster_size_bytes);

    return 0;
}

static ssize_t fatfs_node_read(struct vfs_node* restrict node, void* restrict buffer, size_t sz, off_t offset)
{
    struct fatfs_node_context* context = node->ctxdata;

    int retval;
    size_t bytes_left;
    size_t bytes_to_copy;
    size_t chunk;
    size_t done;
    size_t offset_in_cluster;
    size_t remaining;
    uint32_t cluster;

    remaining = sz;
    done = 0;

    if((size_t)(offset) >= context->size_bytes) {
        return 0;
    }

    if((size_t)(offset) + sz > context->size_bytes) {
        remaining = context->size_bytes - (size_t)offset;
    }

    offset_in_cluster = (size_t)(offset) % context->fs_ctx->cluster_size_bytes;
    cluster = context->first_cluster;

    while(remaining > 0 && cluster < UINT32_MAX) {
        bytes_left = context->fs_ctx->cluster_size_bytes - offset_in_cluster;

        if(remaining < bytes_left) {
            bytes_to_copy = remaining;
        }
        else {
            bytes_to_copy = bytes_left;
        }

        retval = blk_device_read(context->fs_ctx->device, cluster_to_lba(context->fs_ctx, cluster), context->fs_ctx->cluster_size_sectors,
            context->fs_ctx->bounce);

        if(retval < 0) {
            if(done > 0) {
                return (ssize_t)(done);
            }

            return retval;
        }

        chunk = context->fs_ctx->cluster_size_bytes - offset_in_cluster;

        if(chunk > bytes_to_copy) {
            chunk = bytes_to_copy;
        }

        memcpy((uint8_t*)(buffer) + done, (uint8_t*)(context->fs_ctx->bounce) + offset_in_cluster, chunk);

        done += chunk;
        remaining -= chunk;
        offset_in_cluster = 0;

        cluster = next_cluster(context->fs_ctx, cluster);
    }

    return (ssize_t)(done);
}

static int fatfs_vfs_mount(struct vfs* restrict fs, const struct blk_device* restrict device, const struct cmdline* restrict params)
{
    int retval;
    size_t i;
    struct fatfs_bpb* bpb;
    struct fatfs_vfs_context* context;
    struct fatfs_node_context* root_context;
    struct vfs_node* root;
    uint32_t cluster_count;
    uint32_t data_start;
    uint32_t fatfs_size;
    uint32_t root_dir_sectors;
    uint32_t total_sectors;
    union fatfs_ebr* boot;
    unsigned fatfs_type;

    if(device == nullptr) {
        return -EINVAL;
    }

    boot = kmalloc(sizeof(union fatfs_ebr));

    if(boot == nullptr) {
        return -ENOMEM;
    }

    retval = blk_device_read(device, 0, 1, boot);

    if(retval < 0) {
        kfree(boot);

        return retval;
    }

    bpb = &boot->ebr1x.bpb;

    if(bpb->bytes_per_sec != (uint16_t)(device->sector_size)) {
        kfree(boot);

        return -EINVAL;
    }

    if(bpb->sec_per_clus == 0) {
        kfree(boot);

        return -EINVAL;
    }

    if(bpb->fatfs_sz16) {
        fatfs_size = bpb->fatfs_sz16;
    }
    else {
        fatfs_size = boot->ebr32.fatfs_sz32;
    }

    if(bpb->tot_sec16) {
        total_sectors = bpb->tot_sec16;
    }
    else {
        total_sectors = boot->ebr32.bpb.tot_sec32;
    }

    root_dir_sectors = ((uint32_t)(bpb->root_ent_cnt) * 32 + bpb->bytes_per_sec - 1) / bpb->bytes_per_sec;
    data_start = (uint32_t)(bpb->rsvd_sec_cnt) + (uint32_t)(bpb->num_fats) * fatfs_size + root_dir_sectors;
    cluster_count = (total_sectors - data_start) / (uint32_t)(bpb->sec_per_clus);

    if(cluster_count < 4085) {
        fatfs_type = fatfs_TYPE_12;
    }
    else if(cluster_count < 65525) {
        fatfs_type = fatfs_TYPE_16;
    }
    else {
        fatfs_type = fatfs_TYPE_32;
    }

    context = kmalloc(sizeof(struct fatfs_vfs_context));

    if(context == nullptr) {
        kfree(boot);

        return -ENOMEM;
    }

    memset(context, 0, sizeof(struct fatfs_vfs_context));

    context->device = device;
    context->filesystem_type = fatfs_type;
    context->table_start_lba = (uintptr_t)(bpb->rsvd_sec_cnt);
    context->data_start_lba = data_start;
    context->cluster_size_sectors = (size_t)(bpb->sec_per_clus);
    context->cluster_size_bytes = (size_t)(bpb->sec_per_clus) * (size_t)(bpb->bytes_per_sec);
    context->sector_size = (size_t)(bpb->bytes_per_sec);
    context->fatfs_cache_sz = (size_t)fatfs_size * (size_t)(bpb->bytes_per_sec);
    context->fatfs_cache = kmalloc(context->fatfs_cache_sz);

    if(context->filesystem_type == fatfs_TYPE_32) {
        context->root_cluster_fat32 = boot->ebr32.root_clus;
        context->root_dir_sectors_fat1x = 0;
    }
    else {
        context->root_cluster_fat32 = 0;
        context->root_dir_sectors_fat1x = root_dir_sectors;
    }

    context->fatfs_cache_sz = (size_t)(fatfs_size) * (size_t)(bpb->bytes_per_sec);
    context->fatfs_cache = kmalloc(context->fatfs_cache_sz);

    if(context->fatfs_cache == nullptr) {
        kfree(boot);
        kfree(context);

        return -ENOMEM;
    }

    context->bounce = kmalloc(context->sector_size);

    if(context->bounce == nullptr) {
        kfree(boot);
        kfree(context->fatfs_cache);
        kfree(context);

        return -ENOMEM;
    }

    for(i = 0; i < fatfs_size; i++) {
        retval = blk_device_read(device, context->table_start_lba + i, 1, (uint8_t*)(context->fatfs_cache) + i * context->sector_size);

        if(retval < 0) {
            kfree(boot);
            kfree(context->bounce);
            kfree(context->fatfs_cache);
            kfree(context);

            return retval;
        }
    }

    kfree(boot);

    root_context = slab_alloc(sizeof(struct fatfs_node_context));

    if(root_context == nullptr) {
        kfree(context->bounce);
        kfree(context->fatfs_cache);
        kfree(context);

        return -ENOMEM;
    }

    memset(root_context, 0, sizeof(struct fatfs_node_context));

    root_context->fs_ctx = context;
    root_context->first_cluster = context->root_cluster_fat32;
    root_context->size_bytes = 0;

    root = vfs_node_alloc(VFS_NODE_TYPE_DIRECTORY, &fatfs_node_ops, root_context);

    if(root == nullptr) {
        slab_free(root_context);
        kfree(context->bounce);
        kfree(context->fatfs_cache);
        kfree(context);

        return -ENOMEM;
    }

    fs->root = root;
    fs->ctxdata = context;

    return 0;
}

static int fatfs_vfs_unmount(struct vfs* restrict fs)
{
    struct fatfs_vfs_context* context = fs->ctxdata;

    vfs_node_dec_refcount(fs->root);

    kfree(context->bounce);
    kfree(context->fatfs_cache);
    kfree(context);

    return 0;
}

static int fatfs_vfs_sync(struct vfs* restrict fs)
{
    struct fatfs_vfs_context* context = fs->ctxdata;

    int retval;
    size_t fatfs_size = context->fatfs_cache_sz / context->sector_size;
    size_t i;

    for(i = 0; i < fatfs_size; i++) {
        retval = blk_device_write(context->device, context->table_start_lba + i, 1,
            (uint8_t*)(context->fatfs_cache) + i * context->sector_size);

        if(retval < 0) {
            return retval;
        }
    }

    return blk_device_flush(context->device);
}

static const struct vfs_ops fatfs_vfs_ops = {
    .mount = &fatfs_vfs_mount,
    .unmount = &fatfs_vfs_unmount,
    .sync = &fatfs_vfs_sync,
};

static struct vfs_driver fatfs_vfs_driver = {
    .identifier = "fat",
    .ops = &fatfs_vfs_ops,
};

define_initcall(fatfs_init, INITCALL_STAGE_BSP_VFS, INITCALL_ORDER_EARLY)
{
    vfs_driver_register(&fatfs_vfs_driver);
}
