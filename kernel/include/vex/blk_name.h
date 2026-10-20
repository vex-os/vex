#pragma once

#include <vex/compiler.h>

#define BLK_NAME_PREFIX_ATA    "ada"
#define BLK_NAME_PREFIX_ATAPI  "acd"
#define BLK_NAME_PREFIX_NVME   "nvd"
#define BLK_NAME_PREFIX_VIRTIO "vbd"

typedef struct blk_name_s blk_name_t;
typedef struct blk_name_private_s blk_name_private_t;

struct blk_name_private_s {
    blk_name_t* next;
    unsigned next_iterator;
};

struct blk_name_s {
    blk_name_private_t priv;
    char device_prefix[32];
};

extern blk_name_t* blk_name_ata;
extern blk_name_t* blk_name_atapi;
extern blk_name_t* blk_name_nvme;
extern blk_name_t* blk_name_virtio;

blk_name_t* blk_name_query(const char* restrict prefix) __nodiscard;
unsigned blk_name_increment(blk_name_t* restrict name) __nodiscard;
unsigned blk_name_count(const blk_name_t* restrict name) __nodiscard;
