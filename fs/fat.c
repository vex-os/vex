#include <core/errno.h>
#include <fs/vfs_mount.h>
#include <fs/vfs_node.h>
#include <fs/vfs_types.h>
#include <init/initcall.h>
#include <lib/string.h>
#include <lib/strings.h>

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
    struct blk_device* device;
    unsigned fat_type;
    uintptr_t fat_start_lba;
    uintptr_t data_start_lba;
    uintptr_t root_cluster; // FAT1X-only
    size_t cluster_size_bytes;
    size_t cluster_size_sectors;
    size_t sector_size; // probably equal to device->sector_size (fails to mount if not lmao)
};
