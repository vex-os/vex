#include <vex/errno.h>

#include "echfs.h"

uintptr_t echfs_block_next(const struct echfs_vfs_context* restrict context, uintptr_t block)
{
    if(block >= context->total_blocks) {
        return UINTPTR_MAX;
    }

    uint64_t value = context->fat[block];

    if(value == ECHFS_TABLE_FREE || value == ECHFS_TABLE_EOC || value >= ECHFS_TABLE_RESERVED) {
        return UINTPTR_MAX;
    }

    return value;
}

uintptr_t echfs_block_alloc(struct echfs_vfs_context* restrict context)
{
    for(uint64_t i = context->data_start; i < context->total_blocks; ++i) {
        if(context->fat[i] == ECHFS_TABLE_FREE) {
            context->fat[i] = ECHFS_TABLE_EOC;
            context->fat_dirty = true;
            return i;
        }
    }

    return UINTPTR_MAX;
}

void echfs_block_free(struct echfs_vfs_context* restrict context, uintptr_t block)
{
    if(block >= context->total_blocks) {
        return;
    }

    context->fat[block] = ECHFS_TABLE_FREE;
    context->fat_dirty = true;
}
