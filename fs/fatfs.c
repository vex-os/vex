#include <core/cmdline.h>
#include <core/endian.h>
#include <core/errno.h>
#include <dev/blk_device.h>
#include <fs/dirent.h>
#include <fs/mount.h>
#include <fs/path.h>
#include <fs/stat.h>
#include <fs/vnode.h>
#include <init/initcall.h>
#include <lib/ctype.h>
#include <lib/string.h>
#include <lib/strings.h>
#include <mm/malloc.h>
#include <mm/slab.h>

#define FATFS_TYPE_12 12
#define FATFS_TYPE_16 16
#define FATFS_TYPE_32 32

struct fatfs_bootparams {
    uint8_t jmp_boot[3];        ///< BS_jmpBoot
    uint8_t oem_name[8];        ///< BS_OEMName
    uint16_t sector_size;       ///< BPB_BytsPerSec
    uint8_t cluster_size;       ///< BPB_SecPerClus
    uint16_t reserved_sectors;  ///< BPB_RsvdSecCnt
    uint8_t tables_count;       ///< BPB_NumFATs
    uint16_t root_entries;      ///< BPB_RootEntCnt
    uint16_t total_sectors_16;  ///< BPB_TotSec16
    uint8_t media_descriptor;   ///< BPB_Media
    uint16_t table_size_16;     ///< BPB_FATSz16
    uint16_t sectors_per_track; ///< BPB_SecPerTrk
    uint16_t heads_count;       ///< BPB_NumHeads
    uint32_t hidden_sectors;    ///< BPB_HiddSec
    uint32_t total_sectors_32;  ///< BPB_TotSec32
} __packed;

#ifndef __INTELLISENSE__
static_assert(36 == sizeof(struct fatfs_bootparams));
#endif

struct fatfs_extparams_fat1X {
    struct fatfs_bootparams bpb;
    uint8_t drive_number;     ///< BS_DrvNum
    uint8_t reserved_1;       ///< BS_Reserved1
    uint8_t boot_signature;   ///< BS_BootSig
    uint32_t volume_id;       ///< BS_VolID
    uint8_t volume_label[11]; ///< BS_VolLab
    uint8_t fs_type[8];       ///< BS_FilSysType
    uint8_t boot_code[448];
    uint8_t mbr_signature[2];
} __packed;

struct fatfs_extparams_fat32 {
    struct fatfs_bootparams bpb;
    uint32_t table_size_32;     ///< BPB_FATSz32
    uint16_t flags;             ///< BPB_ExtFlags
    uint16_t version;           ///< BPB_FSVer
    uint32_t root_cluster;      ///< BPB_RootClus
    uint16_t fs_info;           ///< BPB_FSInfo
    uint16_t backup_bootsector; ///< BPB_BkBootSec
    uint8_t reserved[12];       /// BPB_Reserved
    uint8_t drive_number;       ///< BS_DrvNum
    uint8_t reserved_1;         ///< BS_Reserved1
    uint8_t boot_signature;     ///< BS_BootSig
    uint32_t volume_id;         ///< BS_VolID
    uint8_t volume_label[11];   ///< BS_VolLab
    uint8_t fs_type[8];         ///< BS_FilSysType
    uint8_t boot_code[420];
    uint8_t mbr_signature[2];
} __packed;

#ifndef __INTELLISENSE__
static_assert(512 == sizeof(struct fatfs_extparams_fat1X));
static_assert(512 == sizeof(struct fatfs_extparams_fat32));
#endif

union fatfs_extparams {
    struct fatfs_extparams_fat1X fat1X;
    struct fatfs_extparams_fat32 fat32;
};

struct fatfs_fsinfo_fat32 {
    uint32_t lead_signature; ///< FSI_LeadSig
    uint8_t reserved_1[480];
    uint32_t struct_signature; ///< FSI_StrucSig
    uint32_t free_count;       ///< FSI_Free_Count
    uint32_t next_free;        ///< FSI_Nxt_Free
    uint8_t reserved_2[12];
    uint32_t trail_signature; ///< FSI_TrailSig
} __packed;

#ifndef __INTELLISENSE__
static_assert(512 == sizeof(struct fatfs_fsinfo_fat32));
#endif

#define FATFS_ATTR_READ_ONLY UINT8_C(0x01)
#define FATFS_ATTR_HIDDEN    UINT8_C(0x02)
#define FATFS_ATTR_SYSTEM    UINT8_C(0x04)
#define FATFS_ATTR_VOLUME_ID UINT8_C(0x08)
#define FATFS_ATTR_DIRECTORY UINT8_C(0x10)
#define FATFS_ATTR_ARCHIVE   UINT8_C(0x20)
#define FATFS_ATTR_LONG_NAME UINT8_C(0x0F)

struct fatfs_table_entry_basic {
    uint8_t name[11];          ///< DIR_Name
    uint8_t attribute;         ///< DIR_Attr
    uint8_t nt_reserved;       ///< DIR_NTRes
    uint8_t create_time_tenth; ///< DIR_CrtTimeTenth
    uint16_t create_time;      ///< DIR_CrtTime
    uint16_t create_date;      ///< DIR_CrtDate
    uint16_t access_date;      ///< DIR_LstAccDate
    uint16_t first_cluster_hi; ///< DIR_FstClusHI
    uint16_t write_time;       ///< DIR_WrtTime
    uint16_t write_date;       ///< DIR_WrtDate
    uint16_t first_cluster_lo; ///< DIR_FstClusLO
    uint32_t file_size;        ///< DIR_FileSize
} __packed;

#define FATFS_LFN_MAX_SEGMENTS 20

struct fatfs_table_entry_long {
    uint8_t order;             ///< LDIR_Ord
    uint16_t name_1[5];        ///< LDIR_Name1
    uint8_t attr;              ///< LDIR_Attr
    uint8_t type;              ///< LDIR_Type
    uint8_t checksum;          ///< LDIR_Chksum
    uint16_t name_2[6];        ///< LDIR_Name2
    uint16_t first_cluster_lo; ///< LDIR_FstClusLO
    uint16_t name_3[2];        ///< LDIR_Name3
} __packed;

#ifndef __INTELLISENSE__
static_assert(32 == sizeof(struct fatfs_table_entry_basic));
static_assert(32 == sizeof(struct fatfs_table_entry_long));
#endif

union fatfs_table_entry {
    struct fatfs_table_entry_basic basic;
    struct fatfs_table_entry_long lfn;
};

struct fatfs_dirent {
    struct fatfs_table_entry_basic basic;
    char full_name[PATH_NAME_SIZE];
};

struct fatfs_context {
    const struct blk_device* device;
    union fatfs_extparams extparams;
    unsigned filesystem_type;

    bool param_readonly;

    uintptr_t table_start_lba;
    uintptr_t data_start_lba;
    uint32_t root_cluster;
    size_t root_directory_sectors;
    size_t cluster_size_sectors;
    size_t cluster_size_bytes;
    size_t sector_size;

    uint8_t* fat_cache;
    size_t fat_cache_sz;

    uint8_t* bounce;
};

struct fatfs_vnode_context {
    struct fatfs_context* fs_ctx;
    uint32_t first_cluster; // zero for fat1x root directory
    uint32_t size_bytes;
};

static uintptr_t cluster_lba(const struct fatfs_context* restrict context, uint32_t cluster)
{
    return context->data_start_lba + (uintptr_t)(cluster - 2) * context->cluster_size_sectors;
}

static uint32_t iterate_cluster(const struct fatfs_context* restrict context, uint32_t current)
{
    uint16_t value_fat1X;
    uint32_t value_fat32;
    size_t offset;

    if(context->filesystem_type == FATFS_TYPE_12) {
        offset = current + (current >> 1);

        if(offset + sizeof(uint16_t) > context->fat_cache_sz) {
            return UINT32_MAX;
        }

        memcpy(&value_fat1X, context->fat_cache + offset, sizeof(uint16_t));

        value_fat1X = LE16_to_host(value_fat1X);

        if(current & 1) {
            value_fat1X >>= 4;
        }
        else {
            value_fat1X &= 0x0FFF;
        }

        if(value_fat1X >= 0xFF8) {
            return UINT32_MAX;
        }

        return value_fat1X;
    }

    if(context->filesystem_type == FATFS_TYPE_16) {
        offset = current * sizeof(uint16_t);

        if(offset + sizeof(uint16_t) > context->fat_cache_sz) {
            return UINT32_MAX;
        }

        memcpy(&value_fat1X, context->fat_cache + offset, sizeof(uint16_t));

        value_fat1X = LE16_to_host(value_fat1X);

        if(value_fat1X >= 0xFFF8) {
            return UINT32_MAX;
        }

        return value_fat1X;
    }

    if(context->filesystem_type == FATFS_TYPE_32) {
        offset = current * sizeof(uint32_t);

        if(offset + sizeof(uint32_t) > context->fat_cache_sz) {
            return UINT32_MAX;
        }

        memcpy(&value_fat32, context->fat_cache + offset, sizeof(uint32_t));

        value_fat32 = LE32_to_host(value_fat32) & UINT32_C(0x0FFFFFFF);

        if(value_fat32 >= 0x0FFFFFF8) {
            return UINT32_MAX;
        }

        return value_fat32;
    }

    return UINT32_MAX;
}

static int read_entry(const struct fatfs_context* restrict context, uint32_t first_cluster, size_t offset,
    union fatfs_table_entry* restrict out)
{
    int retval;
    size_t cluster_index;
    size_t entries_count;
    size_t entry_index_cls;
    size_t entry_index_sec;
    size_t i;
    size_t sector_index;
    uint32_t cluster;
    uintptr_t lba;

    if(first_cluster == 0) {
        entries_count = context->sector_size / sizeof(union fatfs_table_entry);
        sector_index = offset / entries_count;

        if(sector_index >= context->root_directory_sectors) {
            return -ENOENT;
        }

        lba = context->data_start_lba;
        lba -= context->root_directory_sectors;
        lba += sector_index;

        retval = blk_device_read(context->device, lba, 1, context->bounce);

        if(retval < 0) {
            return retval;
        }

        i = offset % entries_count;
        i *= sizeof(union fatfs_table_entry);

        memcpy(out, context->bounce + i, sizeof(union fatfs_table_entry));
    }
    else {
        entries_count = context->cluster_size_bytes / sizeof(union fatfs_table_entry);
        entry_index_cls = offset % entries_count;

        entry_index_sec = entry_index_cls;
        entry_index_sec %= context->sector_size / sizeof(union fatfs_table_entry);

        cluster_index = offset / entries_count;

        sector_index = entry_index_cls * sizeof(union fatfs_table_entry);
        sector_index /= context->sector_size;

        cluster = first_cluster;

        for(i = 0; i < cluster_index; ++i) {
            cluster = iterate_cluster(context, cluster);

            if(cluster == UINT32_MAX) {
                return -ENOENT;
            }
        }

        lba = cluster_lba(context, cluster);
        lba += sector_index;

        retval = blk_device_read(context->device, lba, 1, context->bounce);

        if(retval < 0) {
            return retval;
        }

        i = entry_index_sec * sizeof(union fatfs_table_entry);

        memcpy(out, &context->bounce[i], sizeof(union fatfs_table_entry));
    }

    return 0;
}

static int read_dirent(const struct fatfs_context* restrict context, uint32_t first_cluster, size_t* restrict offset,
    struct fatfs_dirent* restrict out)
{
    int retval;
    bool lfn_done;
    struct fatfs_table_entry_long lfn[FATFS_LFN_MAX_SEGMENTS];
    struct fatfs_table_entry_long* lfn_it;
    union fatfs_table_entry entry;
    unsigned i;
    unsigned j;
    unsigned num_lfn = 0;
    unsigned name_pos;

    for(;;) {
        retval = read_entry(context, first_cluster, offset[0], &entry);
        offset[0] += 1;

        if(retval < 0) {
            return retval;
        }

        if(entry.basic.name[0] == 0x00) {
            return -ENOENT;
        }

        if(entry.basic.name[0] == 0xE5) {
            num_lfn = 0;
            continue;
        }

        if(FATFS_ATTR_LONG_NAME == (entry.basic.attribute & FATFS_ATTR_LONG_NAME)) {
            if(num_lfn < FATFS_LFN_MAX_SEGMENTS) {
                memcpy(&lfn[num_lfn], &entry.lfn, sizeof(struct fatfs_table_entry_long));
                num_lfn += 1;
            }
        }
        else if(entry.basic.attribute & FATFS_ATTR_VOLUME_ID) {
            num_lfn = 0;
        }
        else {
            memcpy(&out->basic, &entry.basic, sizeof(struct fatfs_table_entry_basic));
            break;
        }
    }

    name_pos = 0;

    if(num_lfn) {
        lfn_done = false;

        for(i = num_lfn; i > 0 && !lfn_done; --i) {
            lfn_it = &lfn[i - 1];

            for(j = 0; j < 5 && name_pos < PATH_NAME_SIZE && !lfn_done; ++j) {
                if(lfn_it->name_1[j] == 0x0000 || lfn_it->name_1[j] == 0xFFFF) {
                    lfn_done = true;
                }
                else {
                    out->full_name[name_pos++] = lfn_it->name_1[j] & 0xFF; // FIXME: multibyte convert
                }
            }

            for(j = 0; j < 6 && name_pos < PATH_NAME_SIZE && !lfn_done; ++j) {
                if(lfn_it->name_2[j] == 0x0000 || lfn_it->name_2[j] == 0xFFFF) {
                    lfn_done = true;
                }
                else {
                    out->full_name[name_pos++] = lfn_it->name_2[j] & 0xFF; // FIXME: multibyte convert
                }
            }

            for(j = 0; j < 2 && name_pos < PATH_NAME_SIZE && !lfn_done; ++j) {
                if(lfn_it->name_3[j] == 0x0000 || lfn_it->name_3[j] == 0xFFFF) {
                    lfn_done = true;
                }
                else {
                    out->full_name[name_pos++] = lfn_it->name_3[j] & 0xFF; // FIXME: multibyte convert
                }
            }
        }
    }
    else {
        for(i = 0; i < 8 && !isspace(entry.basic.name[i]); ++i) {
            out->full_name[name_pos] = entry.basic.name[i];
            name_pos += 1;
        }

        if(!isspace(entry.basic.name[8])) {
            out->full_name[name_pos] = 0x2E;
            name_pos += 1;

            for(i = 8; i < 11 && !isspace(entry.basic.name[i]); ++i) {
                out->full_name[name_pos] = entry.basic.name[i];
                name_pos += 1;
            }
        }
    }

    out->full_name[name_pos] = 0x00;

    return 0;
}

static void fatfs_vnode_release(struct vnode* restrict node);
static int fatfs_vnode_lookup(struct vnode* restrict node, const char* restrict name, struct vnode** restrict out);
static int fatfs_vnode_iterate(struct vnode* restrict node, struct dirent* out_dirent, size_t index);
static ssize_t fatfs_vnode_read(struct vnode* restrict node, void* restrict buffer, size_t sz, uintptr_t offset);
static int fatfs_vnode_stat(struct vnode* restrict node, struct __k_stat* restrict out_stat);

static const struct vnode_ops fatfs_vnode_ops = {
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

        if(visible < index) {
            visible += 1;
            continue;
        }

        if(entry.basic.attribute & FATFS_ATTR_VOLUME_ID) {
            continue;
        }

        visible += 1;

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
    size_t done;
    size_t offset_cls;
    size_t remaining;
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

    cluster = context->first_cluster;
    offset_cls = offset % context->fs_ctx->cluster_size_bytes;

    while(remaining > 0 && cluster < UINT32_MAX) {
        chunk = context->fs_ctx->cluster_size_bytes - offset_cls;

        if(chunk > remaining) {
            chunk = remaining;
        }

        lba = cluster_lba(context->fs_ctx, cluster);
        retval = blk_device_read(context->fs_ctx->device, lba, context->fs_ctx->cluster_size_sectors, context->fs_ctx->bounce);

        if(retval < 0) {
            if(done > 0)
                return (ssize_t)(done);
            return retval;
        }

        memcpy(&((uint8_t*)(buffer))[done], &context->fs_ctx->bounce[offset_cls], chunk);

        done += chunk;
        remaining -= chunk;
        offset_cls = 0;

        cluster = iterate_cluster(context->fs_ctx, cluster);
    }

    return (ssize_t)(done);
}

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

static const char* fatfs_identifiers[] = { "fat", "vfat", "msdos", nullptr };
static struct vfs_driver fatfs_vfs_driver = {
    .identifiers = fatfs_identifiers,
    .ops = &fatfs_vfs_ops,
};

define_initcall(fatfs_init, INITCALL_STAGE_BSP_VFS, INITCALL_ORDER_EARLY)
{
    vfs_driver_register(&fatfs_vfs_driver);
}
