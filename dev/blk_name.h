#ifndef DDF5F45D_960D_439C_8511_CE0CBA3BAEAF
#define DDF5F45D_960D_439C_8511_CE0CBA3BAEAF
#pragma once

#include <core/cdefs.h>
#include <stddef.h>

#define BLK_NAME_PREFIX_ATA    "ada"
#define BLK_NAME_PREFIX_ATAPI  "acd"
#define BLK_NAME_PREFIX_NVME   "nvd"
#define BLK_NAME_PREFIX_VIRTIO "vbd"

struct blk_name;
struct blk_name_private {
    struct blk_name* next;
    unsigned next_iterator;
};

struct blk_name {
    struct blk_name_private priv;
    char device_prefix[32];
};

extern struct blk_name* blk_name_ata;    ///< Pre-defined blk_name for ATA devices. Example: ada0n1, ada1n3, etc.
extern struct blk_name* blk_name_atapi;  ///< Pre-defined blk_name for ATAPI devices. Example: asc0n1, asc1n3, etc.
extern struct blk_name* blk_name_nvme;   ///< Pre-defined blk_name for NVMe devices. Example: nvd0n1, nvd1n3, etc.
extern struct blk_name* blk_name_virtio; ///< Pre-defined blk_name for VirtIO block devices. Example: vbd0n1, vbd1n3, etc.

extern struct blk_name* blk_name_query(const char* restrict prefix) __nodiscard;
extern unsigned blk_name_increment(struct blk_name* restrict name) __nodiscard;
extern unsigned blk_name_count(const struct blk_name* restrict name) __nodiscard;

/// Format a block device name into the provided buffer;
/// @param buffer The buffer to write the name into
/// @param sz The size of the buffer
/// @param name The block name to use for formatting
/// @param iterator The iterator value; this corresponds to the succesful probe count for a driver
/// @param index The index value; this corresponds to the actual block device produced by a driver instance
extern int blk_name_format(char* restrict buffer, size_t sz, const struct blk_name* restrict name, unsigned iterator, unsigned index);

#endif /* DDF5F45D_960D_439C_8511_CE0CBA3BAEAF */
