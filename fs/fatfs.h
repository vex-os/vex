#ifndef FATFS_H
#define FATFS_H
#pragma once

#include <core/cdefs.h>
#include <fs/path.h>
#include <lib/string.h>
#include <stddef.h>
#include <stdint.h>

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

extern const struct vnode_ops fatfs_vnode_ops;

extern uintptr_t cluster_lba(const struct fatfs_context* restrict context, uint32_t cluster);
extern uint32_t iterate_cluster(const struct fatfs_context* restrict context, uint32_t current);

extern int read_entry(const struct fatfs_context* restrict context, uint32_t first_cluster, size_t offset,
    union fatfs_table_entry* restrict out);
extern int read_dirent(const struct fatfs_context* restrict context, uint32_t first_cluster, size_t* restrict offset,
    struct fatfs_dirent* restrict out);

#endif /* FATFS_H */
