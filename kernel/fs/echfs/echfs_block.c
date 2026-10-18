#include <vex/errno.h>
#include <vex/pcache.h>

#include "echfs.h"

#define SCAN_BATCH 128 // Table entries read at a time when looking for a free block

static uint64_t entry_offset(const struct echfs_vfs_context* restrict context, uintptr_t block)
{
    return context->fat_start * context->block_size + block * sizeof(uint64_t);
}

static void set_entry(struct echfs_vfs_context* restrict context, uintptr_t block, uint64_t value)
{
    pcache_meta_write(context->meta, entry_offset(context, block), &value, sizeof(value));
}

// An entry that can't be read looks reserved, which ends any chain through it
uint64_t echfs_block_entry(const struct echfs_vfs_context* restrict context, uintptr_t block)
{
    uint64_t value;

    if(block >= context->total_blocks || pcache_meta_read(context->meta, entry_offset(context, block), &value, sizeof(value)) < 0) {
        return ECHFS_TABLE_RESERVED;
    }

    return value;
}

uintptr_t echfs_block_next(const struct echfs_vfs_context* restrict context, uintptr_t block)
{
    uint64_t value = echfs_block_entry(context, block);

    if(value == ECHFS_TABLE_FREE || value == ECHFS_TABLE_EOC || value >= ECHFS_TABLE_RESERVED) {
        return UINTPTR_MAX;
    }

    return value;
}

uintptr_t echfs_block_alloc(struct echfs_vfs_context* restrict context)
{
    uint64_t batch[SCAN_BATCH];
    uint64_t hint = context->next_free_hint;

    if(hint < context->data_start || hint >= context->total_blocks) {
        hint = context->data_start;
    }

    for(size_t pass = 0; pass < 2; pass += 1) {
        uint64_t start = pass == 0 ? hint : context->data_start;
        uint64_t end = pass == 0 ? context->total_blocks : hint;

        for(uint64_t block = start; block < end; block += SCAN_BATCH) {
            size_t count = SCAN_BATCH;

            if(count > end - block) {
                count = end - block;
            }

            if(pcache_meta_read(context->meta, entry_offset(context, block), batch, count * sizeof(uint64_t)) < 0) {
                return UINTPTR_MAX;
            }

            for(size_t i = 0; i < count; ++i) {
                if(batch[i] == ECHFS_TABLE_FREE) {
                    set_entry(context, block + i, ECHFS_TABLE_EOC);
                    context->next_free_hint = block + i + 1;
                    return block + i;
                }
            }
        }
    }

    return UINTPTR_MAX;
}

void echfs_block_free(struct echfs_vfs_context* restrict context, uintptr_t block)
{
    if(block >= context->total_blocks) {
        return;
    }

    set_entry(context, block, ECHFS_TABLE_FREE);

    if(block < context->next_free_hint) {
        context->next_free_hint = block;
    }
}

void echfs_block_link(struct echfs_vfs_context* restrict context, uintptr_t block, uintptr_t next)
{
    if(block >= context->total_blocks) {
        return;
    }

    set_entry(context, block, next);
}
