#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

typedef struct blk_device_s blk_device_t;

typedef struct blk_partition_table_info_s blk_partition_table_info_t;
typedef struct blk_partition_table_ops_s blk_partition_table_ops_t;
typedef struct blk_partition_table_s blk_partition_table_t;
typedef struct blk_partition_table_private_s blk_partition_table_private_t;

struct blk_partition_table_info_s {
    char part_label[64];
    unsigned part_index;
    uintptr_t lba_start;
    size_t lba_count;
    uint64_t type;
};

struct blk_partition_table_ops_s {
    int (*detect)(const blk_device_t* restrict device, void** restrict ptdata);
    int (*read_table)(const blk_device_t* restrict device, void* restrict ptdata, unsigned index, blk_partition_table_info_t* restrict out);
    void (*release)(void* restrict ptdata);
};

struct blk_partition_table_private_s {
    blk_partition_table_t* next;
};

struct blk_partition_table_s {
    blk_partition_table_private_t priv;
    const blk_partition_table_ops_t* ops;
    const char* name;
};

int blk_partition_table_add(blk_partition_table_t* restrict ptable);
int blk_partition_table_remove(blk_partition_table_t* restrict ptable);
int rescan_partitions(const blk_device_t* restrict device);
