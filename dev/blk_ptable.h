#ifndef B33CA66F_97D1_4EA5_BFD0_1B04FED25014
#define B33CA66F_97D1_4EA5_BFD0_1B04FED25014
#pragma once

#include <core/cdefs.h>
#include <core/types.h>

struct blk_ptable_info {
    char part_label[64];
    unsigned part_index;
    uintptr_t lba_start;
    size_t lba_count;
    uint64_t type;
};

struct blk_device;
struct blk_ptable_ops {
    int (*detect)(const struct blk_device* restrict device, void** restrict ptdata);
    int (*readpt)(const struct blk_device* restrict device, void* restrict ptdata, unsigned index, struct blk_ptable_info* restrict out);
    void (*release)(void* restrict ptdata);
};

struct blk_ptable;
struct blk_ptable_private {
    struct blk_ptable* next;
};

struct blk_ptable {
    struct blk_ptable_private priv;
    const struct blk_ptable_ops* ops;
    const char* identifier;
};

int blk_ptable_register(struct blk_ptable* restrict ptable);
int blk_ptable_unregister(struct blk_ptable* restrict ptable);
int blk_ptable_rescan(const struct blk_device* restrict device);

#endif /* B33CA66F_97D1_4EA5_BFD0_1B04FED25014 */
