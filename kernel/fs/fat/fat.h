#ifndef D64336B8_2B5C_45FB_80BF_BE55A7E153AA
#define D64336B8_2B5C_45FB_80BF_BE55A7E153AA
#pragma once

#include <vex/compiler.h>
#include <vex/path.h>
#include <vex/types.h>

#define FAT_TYPE_12 12
#define FAT_TYPE_16 16
#define FAT_TYPE_32 32

struct blk_device;

struct fat_bootparams {
    uint8_t jmp_boot[3];        // BS_jmpBoot
    uint8_t oem_name[8];        // BS_OEMName
    uint16_t sector_size;       // BPB_BytsPerSec
    uint8_t cluster_size;       // BPB_SecPerClus
    uint16_t reserved_sectors;  // BPB_RsvdSecCnt
    uint8_t tables_count;       // BPB_NumFATs
    uint16_t root_entries;      // BPB_RootEntCnt
    uint16_t total_sectors_16;  // BPB_TotSec16
    uint8_t media_descriptor;   // BPB_Media
    uint16_t table_size_16;     // BPB_FATSz16
    uint16_t sectors_per_track; // BPB_SecPerTrk
    uint16_t heads_count;       // BPB_NumHeads
    uint32_t hidden_sectors;    // BPB_HiddSec
    uint32_t total_sectors_32;  // BPB_TotSec32
} __packed;

#ifndef __INTELLISENSE__
static_assert(36 == sizeof(struct fat_bootparams));
#endif

struct fat_extparams_fat1X {
    struct fat_bootparams base;
    uint8_t drive_number;     // BS_DrvNum
    uint8_t reserved_1;       // BS_Reserved1
    uint8_t boot_signature;   // BS_BootSig
    uint32_t volume_id;       // BS_VolID
    uint8_t volume_label[11]; // BS_VolLab
    uint8_t fs_type[8];       // BS_FilSysType
    uint8_t boot_code[448];
    uint8_t mbr_signature[2];
} __packed;

struct fat_extparams_fat32 {
    struct fat_bootparams base;
    uint32_t table_size_32;     // BPB_FATSz32
    uint16_t flags;             // BPB_ExtFlags
    uint16_t version;           // BPB_FSVer
    uint32_t root_cluster;      // BPB_RootClus
    uint16_t fs_info;           // BPB_FSInfo
    uint16_t backup_bootsector; // BPB_BkBootSec
    uint8_t reserved[12];       // BPB_Reserved
    uint8_t drive_number;       // BS_DrvNum
    uint8_t reserved_1;         // BS_Reserved1
    uint8_t boot_signature;     // BS_BootSig
    uint32_t volume_id;         // BS_VolID
    uint8_t volume_label[11];   // BS_VolLab
    uint8_t fs_type[8];         // BS_FilSysType
    uint8_t boot_code[420];
    uint8_t mbr_signature[2];
} __packed;

#ifndef __INTELLISENSE__
static_assert(512 == sizeof(struct fat_extparams_fat1X));
static_assert(512 == sizeof(struct fat_extparams_fat32));
#endif

union fat_extparams {
    struct fat_extparams_fat1X fat1X;
    struct fat_extparams_fat32 fat32;
};

struct fat_fsinfo_fat32 {
    uint32_t lead_signature;   // FSI_LeadSig
    uint8_t reserved_1[480];   // FSI_Reserved1
    uint32_t struct_signature; // FSI_StrucSig
    uint32_t free_count;       // FSI_Free_Count
    uint32_t next_free;        // FSI_Nxt_Free
    uint8_t reserved_2[12];    // FSI_Reserved2
    uint32_t trail_signature;  // FSI_TrailSig
} __packed;

#ifndef __INTELLISENSE__
static_assert(512 == sizeof(struct fat_fsinfo_fat32));
#endif

#define FAT_ATTR_READ_ONLY UINT8_C(0x01)
#define FAT_ATTR_HIDDEN    UINT8_C(0x02)
#define FAT_ATTR_SYSTEM    UINT8_C(0x04)
#define FAT_ATTR_VOLUME_ID UINT8_C(0x08)
#define FAT_ATTR_DIRECTORY UINT8_C(0x10)
#define FAT_ATTR_ARCHIVE   UINT8_C(0x20)
#define FAT_ATTR_LONG_NAME UINT8_C(0x0F)

struct fat_dir_entry {
    uint8_t name[11];          // DIR_Name
    uint8_t attribute;         // DIR_Attr
    uint8_t nt_reserved;       // DIR_NTRes
    uint8_t create_time_tenth; // DIR_CrtTimeTenth
    uint16_t create_time;      // DIR_CrtTime
    uint16_t create_date;      // DIR_CrtDate
    uint16_t access_date;      // DIR_LstAccDate
    uint16_t first_cluster_hi; // DIR_FstClusHI
    uint16_t write_time;       // DIR_WrtTime
    uint16_t write_date;       // DIR_WrtDate
    uint16_t first_cluster_lo; // DIR_FstClusLO
    uint32_t file_size;        // DIR_FileSize
} __packed;

#define FAT_LFN_MAX_SEGMENTS 20

struct fat_ldir_entry {
    uint8_t order;             // LDIR_Ord
    uint16_t name_1[5];        // LDIR_Name1
    uint8_t attr;              // LDIR_Attr
    uint8_t type;              // LDIR_Type
    uint8_t checksum;          // LDIR_Chksum
    uint16_t name_2[6];        // LDIR_Name2
    uint16_t first_cluster_lo; // LDIR_FstClusLO
    uint16_t name_3[2];        // LDIR_Name3
} __packed;

#ifndef __INTELLISENSE__
static_assert(32 == sizeof(struct fat_dir_entry));
static_assert(32 == sizeof(struct fat_ldir_entry));
#endif

union fat_entry {
    struct fat_dir_entry dir;
    struct fat_ldir_entry ldir;
};

struct fat_dirent {
    struct fat_dir_entry base;
    char full_name[NAME_SIZE];
};

struct fat_vfs_context {
    const struct blk_device* device;
    union fat_extparams extparams;
    unsigned filesystem_type;

    gid_t param_gid;
    uid_t param_uid;
    bool param_readonly;

    uintptr_t table_start;
    uintptr_t data_start;
    uintptr_t root_cluster;
    size_t rootdir_sectors;
    size_t cluster_size_s;
    size_t cluster_size_b;
    size_t sector_size;

    size_t table_size_s;
    unsigned num_tables;
    uintptr_t fsinfo_lba;
    size_t cluster_count;

    uint8_t* cache;
    size_t cache_size;
    bool cache_dirty;

    uintptr_t next_free_hint;

    uint8_t* bounce;
};

struct fat_vnode_context {
    struct fat_vfs_context* fs_ctx;
    uintptr_t first_cluster;
    size_t size_bytes;
    uintptr_t dir_cluster;
    size_t dir_offset;
    uintptr_t cached_cluster;
    size_t cached_cluster_idx;
};

extern const struct vnode_ops fat_vnode_ops;

void fat_cluster_link(struct fat_vfs_context* restrict context, uintptr_t cluster, uintptr_t next);
void fat_cluster_free(struct fat_vfs_context* restrict context, uintptr_t cluster);

uintptr_t fat_cluster_next(const struct fat_vfs_context* restrict context, uintptr_t cluster) __nodiscard;
uintptr_t fat_cluster_alloc(struct fat_vfs_context* restrict context) __nodiscard;

int fat_dirent_read(const struct fat_vfs_context* restrict context, uintptr_t cluster, size_t* restrict offset,
    struct fat_dirent* restrict entry);
int fat_dirent_write(const struct fat_vfs_context* restrict context, uintptr_t cluster, size_t offset,
    const struct fat_dirent* restrict entry);
int fat_dirent_update(const struct fat_vfs_context* restrict context, uintptr_t cluster, size_t offset,
    const struct fat_dir_entry* restrict entry);
int fat_dirent_alloc(struct fat_vfs_context* restrict context, uintptr_t cluster, unsigned count, size_t* restrict out_offset);

static __always_inline uintptr_t fat_cluster_lba(const struct fat_vfs_context* restrict context, uintptr_t cluster)
{
    return context->data_start + (cluster - 2) * context->cluster_size_s;
}

#endif /* D64336B8_2B5C_45FB_80BF_BE55A7E153AA */
