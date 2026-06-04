#include "fatfs.h"

#include <core/endian.h>

#include <stdint.h>
#include <string.h>

uintptr_t cluster_lba(const struct fatfs_context* restrict context, uint32_t cluster)
{
    return context->data_start_lba + (uintptr_t)(cluster - 2) * context->cluster_size_sectors;
}

uint32_t iterate_cluster(const struct fatfs_context* restrict context, uint32_t current)
{
    uint16_t value_fat1X;
    uint32_t value_fat32;
    size_t offset;

    if(context->filesystem_type == FATFS_TYPE_12) {
        offset = current + (current >> 1);

        if(offset + sizeof(uint16_t) > context->fat_cache_sz) {
            return UINT32_MAX;
        }

        memcpy(&value_fat1X, context->fat_cache + offset, sizeof(uint16_t));

        value_fat1X = LE16_to_host(value_fat1X);

        if(current & 1) {
            value_fat1X >>= 4;
        }
        else {
            value_fat1X &= 0x0FFF;
        }

        if(value_fat1X >= 0xFF8) {
            return UINT32_MAX;
        }

        return value_fat1X;
    }

    if(context->filesystem_type == FATFS_TYPE_16) {
        offset = current * sizeof(uint16_t);

        if(offset + sizeof(uint16_t) > context->fat_cache_sz) {
            return UINT32_MAX;
        }

        memcpy(&value_fat1X, context->fat_cache + offset, sizeof(uint16_t));

        value_fat1X = LE16_to_host(value_fat1X);

        if(value_fat1X >= 0xFFF8) {
            return UINT32_MAX;
        }

        return value_fat1X;
    }

    if(context->filesystem_type == FATFS_TYPE_32) {
        offset = current * sizeof(uint32_t);

        if(offset + sizeof(uint32_t) > context->fat_cache_sz) {
            return UINT32_MAX;
        }

        memcpy(&value_fat32, context->fat_cache + offset, sizeof(uint32_t));

        value_fat32 = LE32_to_host(value_fat32) & UINT32_C(0x0FFFFFFF);

        if(value_fat32 >= 0x0FFFFFF8) {
            return UINT32_MAX;
        }

        return value_fat32;
    }

    return UINT32_MAX;
}
