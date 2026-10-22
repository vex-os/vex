#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

typedef struct block_interface_s block_interface_t;

typedef struct block_partition_table_info_s block_partition_table_info_t;
typedef struct block_partition_table_ops_s block_partition_table_ops_t;
typedef struct block_partition_table_s block_partition_table_t;
typedef struct block_partition_table_private_s block_partition_table_private_t;

struct block_partition_table_info_s {
    char part_label[64];
    unsigned part_index;
    uintptr_t lba_start;
    size_t lba_count;
    uint64_t type;
};

struct block_partition_table_ops_s {
    int (*detect)(const block_interface_t* restrict interface, void** restrict ptdata);
    int (*read_table)(const block_interface_t* restrict interface, void* restrict ptdata, unsigned index,
        block_partition_table_info_t* restrict out);
    void (*release)(void* restrict ptdata);
};

struct block_partition_table_private_s {
    block_partition_table_t* next;
};

struct block_partition_table_s {
    block_partition_table_private_t priv;
    const block_partition_table_ops_t* ops;
    const char* name;
};

int block_partition_table_add(block_partition_table_t* restrict ptable);
int block_partition_table_remove(block_partition_table_t* restrict ptable);
int rescan_partitions(const block_interface_t* restrict interface);
