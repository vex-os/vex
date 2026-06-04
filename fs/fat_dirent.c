#include "fat.h"

#include <core/errno.h>
#include <dev/blk_device.h>
#include <lib/ctype.h>
#include <lib/string.h>

static int fat_entry_read(const struct fat_vfs_context* restrict context, uintptr_t cluster, size_t offset,
    union fat_entry* restrict out)
{
    size_t entries_per_sector = context->sector_size / sizeof(union fat_entry);
    size_t sector_index;
    size_t entry_in_sector;
    uintptr_t lba;

    if(cluster == 0) {
        sector_index = offset / entries_per_sector;

        if(sector_index >= context->rootdir_sectors) {
            return -ENOENT;
        }

        lba = context->data_start - context->rootdir_sectors + sector_index;
    }
    else {
        size_t entries_per_cluster = context->cluster_size_b / sizeof(union fat_entry);
        size_t cluster_index = offset / entries_per_cluster;
        size_t entry_in_cluster = offset % entries_per_cluster;

        sector_index = entry_in_cluster / entries_per_sector;

        for(size_t i = 0; i < cluster_index; i++) {
            cluster = fat_cluster_next(context, cluster);

            if(cluster == UINTPTR_MAX) {
                return -ENOENT;
            }
        }

        lba = fat_cluster_lba(context, cluster) + sector_index;
    }

    int retval = blk_device_read(context->device, lba, 1, context->bounce);

    if(retval < 0) {
        return retval;
    }

    entry_in_sector = offset % entries_per_sector;
    memcpy(out, context->bounce + entry_in_sector * sizeof(union fat_entry), sizeof(union fat_entry));

    return 0;
}

static int fat_entry_write(const struct fat_vfs_context* restrict context, uintptr_t cluster, size_t offset,
    const union fat_entry* restrict entry)
{
    size_t entries_per_sector = context->sector_size / sizeof(union fat_entry);
    size_t sector_index;
    size_t entry_in_sector;
    uintptr_t lba;

    if(cluster == 0) {
        sector_index = offset / entries_per_sector;

        if(sector_index >= context->rootdir_sectors) {
            return -ENOSPC;
        }

        lba = context->data_start - context->rootdir_sectors + sector_index;
    }
    else {
        size_t entries_per_cluster = context->cluster_size_b / sizeof(union fat_entry);
        size_t cluster_index = offset / entries_per_cluster;
        size_t entry_in_cluster = offset % entries_per_cluster;

        sector_index = entry_in_cluster / entries_per_sector;

        for(size_t i = 0; i < cluster_index; i++) {
            cluster = fat_cluster_next(context, cluster);

            if(cluster == UINTPTR_MAX) {
                return -ENOENT;
            }
        }

        lba = fat_cluster_lba(context, cluster) + sector_index;
    }

    int retval = blk_device_read(context->device, lba, 1, context->bounce);

    if(retval < 0) {
        return retval;
    }

    entry_in_sector = offset % entries_per_sector;
    memcpy(context->bounce + entry_in_sector * sizeof(union fat_entry), entry, sizeof(union fat_entry));

    return blk_device_write(context->device, lba, 1, context->bounce);
}

static uint8_t lfn_checksum(const uint8_t name[11])
{
    uint8_t sum = 0;

    for(int i = 0; i < 11; i++) {
        sum = ((sum & 1) ? 0x80 : 0x00) + (sum >> 1) + name[i];
    }

    return sum;
}

int fat_dirent_read(const struct fat_vfs_context* restrict context, uintptr_t cluster, size_t* restrict offset,
    struct fat_dirent* restrict entry)
{
    union fat_entry raw;
    struct fat_ldir_entry lfn[FAT_LFN_MAX_SEGMENTS];
    unsigned num_lfn = 0;

    for(;;) {
        int retval = fat_entry_read(context, cluster, offset[0], &raw);
        offset[0] += 1;

        if(retval < 0) {
            return retval;
        }

        if(raw.dir.name[0] == 0x00) {
            return -ENOENT;
        }

        if(raw.dir.name[0] == 0xE5) {
            num_lfn = 0;
            continue;
        }

        if((raw.dir.attribute & FAT_ATTR_LONG_NAME) == FAT_ATTR_LONG_NAME) {
            if(num_lfn < FAT_LFN_MAX_SEGMENTS) {
                memcpy(&lfn[num_lfn++], &raw.ldir, sizeof(struct fat_ldir_entry));
            }
        }
        else if(raw.dir.attribute & FAT_ATTR_VOLUME_ID) {
            num_lfn = 0;
        }
        else {
            memcpy(&entry->base, &raw.dir, sizeof(struct fat_dir_entry));
            break;
        }
    }

    size_t name_pos = 0;

    if(num_lfn) {
        bool done = false;

        for(unsigned i = num_lfn; i > 0 && !done; i--) {
            struct fat_ldir_entry* seg = &lfn[i - 1];

            for(unsigned j = 0; j < 5 && name_pos < PATH_NAME_SIZE && !done; j++) {
                if(seg->name_1[j] == 0x0000 || seg->name_1[j] == 0xFFFF) { done = true; break; }
                entry->full_name[name_pos++] = seg->name_1[j] & 0xFF;
            }

            for(unsigned j = 0; j < 6 && name_pos < PATH_NAME_SIZE && !done; j++) {
                if(seg->name_2[j] == 0x0000 || seg->name_2[j] == 0xFFFF) { done = true; break; }
                entry->full_name[name_pos++] = seg->name_2[j] & 0xFF;
            }

            for(unsigned j = 0; j < 2 && name_pos < PATH_NAME_SIZE && !done; j++) {
                if(seg->name_3[j] == 0x0000 || seg->name_3[j] == 0xFFFF) { done = true; break; }
                entry->full_name[name_pos++] = seg->name_3[j] & 0xFF;
            }
        }
    }
    else {
        for(unsigned i = 0; i < 8 && !isspace(entry->base.name[i]); i++) {
            entry->full_name[name_pos++] = entry->base.name[i];
        }

        if(!isspace(entry->base.name[8])) {
            entry->full_name[name_pos++] = '.';

            for(unsigned i = 8; i < 11 && !isspace(entry->base.name[i]); i++) {
                entry->full_name[name_pos++] = entry->base.name[i];
            }
        }
    }

    entry->full_name[name_pos] = '\0';

    return 0;
}

int fat_dirent_write(const struct fat_vfs_context* restrict context, uintptr_t cluster, size_t offset,
    const struct fat_dirent* restrict entry)
{
    union fat_entry raw;
    size_t name_len = kstrnlen(entry->full_name, PATH_NAME_SIZE);
    unsigned num_lfn = (name_len + 12) / 13;
    uint8_t checksum = lfn_checksum(entry->base.name);

    for(unsigned seg = num_lfn; seg >= 1; seg--) {
        size_t base = (seg - 1) * 13;
        struct fat_ldir_entry* ldir = &raw.ldir;

        memset(ldir, 0, sizeof(struct fat_ldir_entry));

        ldir->order = (seg == num_lfn) ? (0x40 | seg) : seg;
        ldir->attr = FAT_ATTR_LONG_NAME;
        ldir->checksum = checksum;
        ldir->first_cluster_lo = 0;

        for(unsigned j = 0; j < 5; j++) {
            size_t pos = base + j;
            ldir->name_1[j] = pos < name_len ? (uint16_t)(uint8_t)entry->full_name[pos] : (pos == name_len ? 0x0000 : 0xFFFF);
        }

        for(unsigned j = 0; j < 6; j++) {
            size_t pos = base + 5 + j;
            ldir->name_2[j] = pos < name_len ? (uint16_t)(uint8_t)entry->full_name[pos] : (pos == name_len ? 0x0000 : 0xFFFF);
        }

        for(unsigned j = 0; j < 2; j++) {
            size_t pos = base + 11 + j;
            ldir->name_3[j] = pos < name_len ? (uint16_t)(uint8_t)entry->full_name[pos] : (pos == name_len ? 0x0000 : 0xFFFF);
        }

        int retval = fat_entry_write(context, cluster, offset - seg, &raw);

        if(retval < 0) {
            return retval;
        }
    }

    memcpy(&raw.dir, &entry->base, sizeof(struct fat_dir_entry));

    return fat_entry_write(context, cluster, offset, &raw);
}
