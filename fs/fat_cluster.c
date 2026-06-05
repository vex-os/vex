#include <core/endian.h>
#include <core/errno.h>
#include <dev/blk_device.h>
#include <fs/fat.h>
#include <lib/string.h>

#define FAT12_EOC  UINT16_C(0xFF8)
#define FAT16_EOC  UINT16_C(0xFFF8)
#define FAT32_EOC  UINT32_C(0x0FFFFFF8)
#define FAT32_MASK UINT32_C(0x0FFFFFFF)

static uint16_t fat12_read_entry(const struct fat_vfs_context* restrict context, uintptr_t cluster)
{
    size_t offset = cluster + (cluster >> 1);
    uint16_t data;

    memcpy(&data, context->cache + offset, sizeof(uint16_t));

    data = LE16_to_host(data);

    if(cluster & 1) {
        return data >> 4;
    }
    else {
        return data & UINT16_C(0x0FFF);
    }
}

static void fat12_write_entry(struct fat_vfs_context* restrict context, uintptr_t cluster, uint16_t value)
{
    size_t offset = cluster + (cluster >> 1);
    uint16_t data;

    memcpy(&data, context->cache + offset, sizeof(uint16_t));

    data = LE16_to_host(data);

    if(cluster & 1) {
        data = (data & 0x000F) | ((value & 0x0FFF) << 4);
    }
    else {
        data = (data & 0xF000) | (value & 0x0FFF);
    }

    data = host_to_LE16(data);

    memcpy(context->cache + offset, &data, sizeof(uint16_t));
}

uintptr_t fat_cluster_next(const struct fat_vfs_context* restrict context, uintptr_t cluster)
{
    if(context->filesystem_type == FAT_TYPE_12) {
        size_t offset = cluster + (cluster >> 1);

        if(offset + sizeof(uint16_t) > context->cache_size) {
            return UINTPTR_MAX;
        }

        uint16_t data16;
        memcpy(&data16, context->cache + offset, sizeof(uint16_t));
        data16 = LE16_to_host(data16);

        if(cluster & 1) {
            data16 >>= 4;
        }
        else {
            data16 &= 0x0FFF;
        }

        if(data16 >= FAT12_EOC) {
            return UINTPTR_MAX;
        }
        else {
            return data16;
        }
    }

    if(context->filesystem_type == FAT_TYPE_16) {
        size_t offset = cluster * sizeof(uint16_t);

        if(offset + sizeof(uint16_t) > context->cache_size) {
            return UINTPTR_MAX;
        }

        uint16_t data16;
        memcpy(&data16, context->cache + offset, sizeof(uint16_t));
        data16 = LE16_to_host(data16);

        if(data16 >= FAT16_EOC) {
            return UINTPTR_MAX;
        }
        else {
            return data16;
        }
    }

    if(context->filesystem_type == FAT_TYPE_32) {
        size_t offset = cluster * sizeof(uint32_t);

        if(offset + sizeof(uint32_t) > context->cache_size) {
            return UINTPTR_MAX;
        }

        uint32_t data32;
        memcpy(&data32, context->cache + offset, sizeof(uint32_t));
        data32 = LE32_to_host(data32) & FAT32_MASK;

        if(data32 >= FAT32_EOC) {
            return UINTPTR_MAX;
        }
        else {
            return data32;
        }
    }

    return UINTPTR_MAX;
}

uintptr_t fat_cluster_alloc(struct fat_vfs_context* restrict context)
{
    if(context->filesystem_type == FAT_TYPE_12) {
        size_t count = (context->cache_size * 2) / 3;

        for(size_t i = 2; i < count; i++) {
            if(0 == fat12_read_entry(context, i)) {
                fat12_write_entry(context, i, 0x0FFF);
                return i;
            }
        }
    }
    else if(context->filesystem_type == FAT_TYPE_16) {
        size_t count = context->cache_size / sizeof(uint16_t);

        for(size_t i = 2; i < count; i++) {
            uint16_t data16;
            memcpy(&data16, context->cache + i * sizeof(uint16_t), sizeof(uint16_t));

            if(0 == LE16_to_host(data16)) {
                data16 = host_to_LE16(0xFFFF);
                memcpy(context->cache + i * sizeof(uint16_t), &data16, sizeof(uint16_t));
                return i;
            }
        }
    }
    else if(context->filesystem_type == FAT_TYPE_32) {
        size_t count = context->cache_size / sizeof(uint32_t);

        for(size_t i = 2; i < count; i++) {
            uint32_t data32;
            memcpy(&data32, context->cache + i * sizeof(uint32_t), sizeof(uint32_t));

            if(0 == (LE32_to_host(data32) & FAT32_MASK)) {
                data32 = host_to_LE32(FAT32_MASK);
                memcpy(context->cache + i * sizeof(uint32_t), &data32, sizeof(uint32_t));
                return i;
            }
        }
    }

    return UINTPTR_MAX;
}

void fat_cluster_link(struct fat_vfs_context* restrict context, uintptr_t cluster, uintptr_t next)
{
    if(context->filesystem_type == FAT_TYPE_12) {
        fat12_write_entry(context, cluster, next);
    }
    else if(context->filesystem_type == FAT_TYPE_16) {
        uint16_t data16 = host_to_LE16(next);
        memcpy(context->cache + cluster * sizeof(uint16_t), &data16, sizeof(uint16_t));
    }
    else if(context->filesystem_type == FAT_TYPE_32) {
        uint32_t data32;
        memcpy(&data32, context->cache + cluster * sizeof(uint32_t), sizeof(uint32_t));
        data32 = (data32 & host_to_LE32(~FAT32_MASK)) | host_to_LE32(next & FAT32_MASK);
        memcpy(context->cache + cluster * sizeof(uint32_t), &data32, sizeof(uint32_t));
    }
}

void fat_cluster_free(struct fat_vfs_context* restrict context, uintptr_t cluster)
{
    if(context->filesystem_type == FAT_TYPE_12) {
        fat12_write_entry(context, cluster, 0);
    }
    else if(context->filesystem_type == FAT_TYPE_16) {
        uint16_t zero16 = 0;
        memcpy(context->cache + cluster * sizeof(uint16_t), &zero16, sizeof(uint16_t));
    }
    else if(context->filesystem_type == FAT_TYPE_32) {
        uint32_t data32;
        memcpy(&data32, context->cache + cluster * sizeof(uint32_t), sizeof(uint32_t));
        data32 &= host_to_LE32(~FAT32_MASK);
        memcpy(context->cache + cluster * sizeof(uint32_t), &data32, sizeof(uint32_t));
    }
}
