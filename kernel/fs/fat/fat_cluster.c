#include <vex/blk_device.h>
#include <vex/endian.h>
#include <vex/errno.h>
#include <vex/string.h>

#include "fat.h"

#define FAT12_EOC  UINT16_C(0xFF8)
#define FAT16_EOC  UINT16_C(0xFFF8)
#define FAT32_EOC  UINT32_C(0x0FFFFFF8)
#define FAT32_MASK UINT32_C(0x0FFFFFFF)

static uint16_t fat12_get(const struct fat_vfs_context* restrict context, uintptr_t cluster)
{
    size_t offset = cluster + (cluster >> 1);
    uint16_t data;

    memcpy(&data, context->cache + offset, sizeof(uint16_t));

    data = le16toh(data);

    if(cluster & 1) {
        return data >> 4;
    }
    else {
        return data & UINT16_C(0x0FFF);
    }
}

static void fat12_set(struct fat_vfs_context* restrict context, uintptr_t cluster, uint16_t value)
{
    size_t offset = cluster + (cluster >> 1);
    uint16_t data;

    memcpy(&data, context->cache + offset, sizeof(uint16_t));

    data = le16toh(data);

    if(cluster & 1) {
        data = (data & 0x000F) | ((value & 0x0FFF) << 4);
    }
    else {
        data = (data & 0xF000) | (value & 0x0FFF);
    }

    data = htole16(data);

    memcpy(context->cache + offset, &data, sizeof(uint16_t));
}

static uintptr_t cluster_next_raw(const struct fat_vfs_context* restrict context, uintptr_t cluster)
{
    if(context->filesystem_type == FAT_TYPE_12) {
        size_t offset = cluster + (cluster >> 1);

        if(offset + sizeof(uint16_t) > context->cache_size) {
            return UINTPTR_MAX;
        }

        uint16_t data16;
        memcpy(&data16, context->cache + offset, sizeof(uint16_t));
        data16 = le16toh(data16);

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
        data16 = le16toh(data16);

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
        data32 = le32toh(data32) & FAT32_MASK;

        if(data32 >= FAT32_EOC) {
            return UINTPTR_MAX;
        }
        else {
            return data32;
        }
    }

    return UINTPTR_MAX;
}

uintptr_t fat_cluster_next(const struct fat_vfs_context* restrict context, uintptr_t cluster)
{
    if(!fat_cluster_valid(context, cluster)) {
        return UINTPTR_MAX;
    }

    uintptr_t next = cluster_next_raw(context, cluster);

    // A link to a reserved or nonexistent cluster is a broken chain; it
    // ends here rather than letting the caller walk off the volume
    if(!fat_cluster_valid(context, next)) {
        return UINTPTR_MAX;
    }

    return next;
}

uintptr_t fat_cluster_alloc(struct fat_vfs_context* restrict context)
{
    // The tables are often larger than the volume needs; entries past
    // the last data cluster don't describe anything that can be allocated
    size_t limit = context->cluster_count + 2;

    if(context->filesystem_type == FAT_TYPE_12) {
        size_t count = (context->cache_size * 2) / 3;

        if(count > limit) {
            count = limit;
        }
        uintptr_t hint = context->next_free_hint;

        for(size_t pass = 0; pass < 2; pass += 1) {
            size_t start, end;

            if(pass == 0) {
                start = hint;
                end = count;
            }
            else {
                start = 2;
                end = hint;
            }

            for(size_t i = start; i < end; ++i) {
                if(0 == fat12_get(context, i)) {
                    fat12_set(context, i, 0x0FFF);
                    context->next_free_hint = i + 1;
                    context->cache_dirty = true;
                    return i;
                }
            }
        }
    }
    else if(context->filesystem_type == FAT_TYPE_16) {
        size_t count = context->cache_size / sizeof(uint16_t);

        if(count > limit) {
            count = limit;
        }
        uintptr_t hint = context->next_free_hint;

        for(size_t pass = 0; pass < 2; pass += 1) {
            size_t start, end;

            if(pass == 0) {
                start = hint;
                end = count;
            }
            else {
                start = 2;
                end = hint;
            }

            for(size_t i = start; i < end; ++i) {
                uint16_t data16;
                memcpy(&data16, context->cache + i * sizeof(uint16_t), sizeof(uint16_t));

                if(0 == le16toh(data16)) {
                    data16 = htole16(0xFFFF);
                    memcpy(context->cache + i * sizeof(uint16_t), &data16, sizeof(uint16_t));
                    context->next_free_hint = i + 1;
                    context->cache_dirty = true;
                    return i;
                }
            }
        }
    }
    else if(context->filesystem_type == FAT_TYPE_32) {
        size_t count = context->cache_size / sizeof(uint32_t);

        if(count > limit) {
            count = limit;
        }
        uintptr_t hint = context->next_free_hint;

        for(size_t pass = 0; pass < 2; pass += 1) {
            size_t start, end;

            if(pass == 0) {
                start = hint;
                end = count;
            }
            else {
                start = 2;
                end = hint;
            }

            for(size_t i = start; i < end; ++i) {
                uint32_t data32;
                memcpy(&data32, context->cache + i * sizeof(uint32_t), sizeof(uint32_t));

                if(0 == (le32toh(data32) & FAT32_MASK)) {
                    data32 = htole32(FAT32_MASK);
                    memcpy(context->cache + i * sizeof(uint32_t), &data32, sizeof(uint32_t));
                    context->next_free_hint = i + 1;
                    context->cache_dirty = true;
                    return i;
                }
            }
        }
    }

    return UINTPTR_MAX;
}

void fat_cluster_link(struct fat_vfs_context* restrict context, uintptr_t cluster, uintptr_t next)
{
    if(!fat_cluster_valid(context, cluster)) {
        return;
    }

    if(context->filesystem_type == FAT_TYPE_12) {
        fat12_set(context, cluster, next);
    }
    else if(context->filesystem_type == FAT_TYPE_16) {
        uint16_t data16 = htole16(next);
        memcpy(context->cache + cluster * sizeof(uint16_t), &data16, sizeof(uint16_t));
    }
    else if(context->filesystem_type == FAT_TYPE_32) {
        uint32_t data32;
        memcpy(&data32, context->cache + cluster * sizeof(uint32_t), sizeof(uint32_t));
        data32 = (data32 & htole32(~FAT32_MASK)) | htole32(next & FAT32_MASK);
        memcpy(context->cache + cluster * sizeof(uint32_t), &data32, sizeof(uint32_t));
    }

    context->cache_dirty = true;
}

void fat_cluster_free(struct fat_vfs_context* restrict context, uintptr_t cluster)
{
    if(!fat_cluster_valid(context, cluster)) {
        return;
    }

    if(context->filesystem_type == FAT_TYPE_12) {
        fat12_set(context, cluster, 0);
    }
    else if(context->filesystem_type == FAT_TYPE_16) {
        uint16_t zero16 = 0;
        memcpy(context->cache + cluster * sizeof(uint16_t), &zero16, sizeof(uint16_t));
    }
    else if(context->filesystem_type == FAT_TYPE_32) {
        uint32_t data32;
        memcpy(&data32, context->cache + cluster * sizeof(uint32_t), sizeof(uint32_t));
        data32 &= htole32(~FAT32_MASK);
        memcpy(context->cache + cluster * sizeof(uint32_t), &data32, sizeof(uint32_t));
    }

    if(cluster < context->next_free_hint) {
        context->next_free_hint = cluster;
    }

    context->cache_dirty = true;
}

uintptr_t fat_cluster_alloc_zeroed(struct fat_vfs_context* restrict context)
{
    uintptr_t cluster = fat_cluster_alloc(context);

    if(cluster == UINTPTR_MAX) {
        return UINTPTR_MAX;
    }

    uintptr_t lba = fat_cluster_lba(context, cluster);

    memset(context->bounce, 0, context->sector_size);

    for(size_t s = 0; s < context->cluster_size_s; ++s) {
        if(blk_write(context->device, lba + s, 1, context->bounce) < 0) {
            fat_cluster_free(context, cluster);
            return UINTPTR_MAX;
        }
    }

    return cluster;
}
