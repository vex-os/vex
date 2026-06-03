#ifndef CA60AB1B_7282_4E08_961E_2618B7DA1474
#define CA60AB1B_7282_4E08_961E_2618B7DA1474
#pragma once

#include <core/cdefs.h>
#include <core/types.h>

enum {
    FATFS_TYPE_12 = 12,
    FATFS_TYPE_16 = 16,
    FATFS_TYPE_32 = 32,
};

struct fatfs_bpb {
    uint8_t jump_boot[3];
    uint8_t oem_name[8];
    uint16_t sector_size;
    uint8_t cluster_size; // in sectors
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t media_descriptor;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
} __packed;

#ifndef __INTELLISENSE__
static_assert(36 == sizeof(struct fatfs_boot_sector));
#endif

struct fatfs_ebr1x {
    struct fatfs_bpb bpb;
    uint8_t drive_number;
    uint8_t reserved_1;
    uint8_t boot_signature;
    uint32_t volume_id;
    uint8_t volume_label[11];
    uint8_t filesystem_type[8];
    uint8_t reserved2[448];
    uint8_t mbr_signature[2];
} __packed;

struct fatfs_ebr32 {
    struct fatfs_bpb bpb;
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info_sector;
    uint16_t backup_boot_sector;
    uint8_t reserved[12];
    uint8_t drive_number;
    uint8_t reserved_1;
    uint8_t boot_signature;
    uint32_t volume_id;
    uint8_t volume_label[11];
    uint8_t filesystem_type[8];
    uint8_t reserved2[420];
    uint8_t mbr_signature[2];
} __packed;

#ifndef __INTELLISENSE__
static_assert(512 == sizeof(struct fatfs_ebr1x));
static_assert(512 == sizeof(struct fatfs_ebr32));
#endif

union fatfs_ebr {
    struct fatfs_ebr1x ebr1x;
    struct fatfs_ebr32 ebr32;
};

struct fatfs_fsinfo {
    uint32_t lead_signature;
    uint8_t reserved1[480];
    uint32_t struct_signature;
    uint32_t free_cluster_count;
    uint32_t next_free_cluster;
    uint8_t reserved2[12];
    uint32_t trail_signature;
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

struct fatfs_entry_dir {
    uint8_t name[11];
    uint8_t attribute;
    uint8_t nt_reserved;
    uint8_t create_time_tenth;
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t first_cluster_hi;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_lo;
    uint32_t file_size;
} __packed;

struct fatfs_entry_long {
    uint8_t order;
    uint16_t name_1[5];
    uint8_t attr;
    uint8_t type;
    uint8_t checksum;
    uint16_t name_2[6];
    uint16_t reserved;
    uint16_t name_3[2];
} __packed;

#ifndef __INTELLISENSE__
static_assert(32 == sizeof(struct fatfs_entry_dir));
static_assert(32 == sizeof(struct fatfs_entry_long));
#endif

#endif /* CA60AB1B_7282_4E08_961E_2618B7DA1474 */
