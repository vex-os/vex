#ifndef DDF5F45D_960D_439C_8511_CE0CBA3BAEAF
#define DDF5F45D_960D_439C_8511_CE0CBA3BAEAF
#pragma once

#include <vex/compiler.h>

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

extern struct blk_name* blk_name_ata;
extern struct blk_name* blk_name_atapi;
extern struct blk_name* blk_name_nvme;
extern struct blk_name* blk_name_virtio;

struct blk_name* blk_name_query(const char* restrict prefix) __nodiscard;
unsigned blk_name_increment(struct blk_name* restrict name) __nodiscard;
unsigned blk_name_count(const struct blk_name* restrict name) __nodiscard;

#endif /* DDF5F45D_960D_439C_8511_CE0CBA3BAEAF */
