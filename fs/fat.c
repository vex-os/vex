#include <core/errno.h>
#include <fs/vfs_mount.h>
#include <fs/vfs_node.h>
#include <fs/vfs_types.h>
#include <init/initcall.h>
#include <lib/string.h>
#include <lib/strings.h>

struct fat_bpb {
    uint8_t jmp_boot[3];
    uint8_t oem_name[8];
    uint16_t sector_size; // should be equal to blk_device::sector_size, probably...
    uint8_t cluster_size;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entry_count;
    uint16_t total_sectors;
    uint8_t media_descriptor;
    uint16_t sectors_short;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t sectors_long;
} __packed;

#ifndef __INTELLISENSE__
static_assert(36 == sizeof(struct fat_bpb));
#endif

struct fat12_ebr {
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
static_assert(512 == sizeof(struct fat12_ebr));
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
