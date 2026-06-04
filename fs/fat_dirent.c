#include <core/errno.h>
#include <dev/blk_device.h>
#include <fs/fat.h>
#include <lib/ctype.h>
#include <lib/string.h>

static int read_entry(const struct fat_vfs_context* restrict context, uintptr_t cluster, size_t offset, union fat_entry* restrict entry)
{
    size_t entries_per_sector = context->sector_size / sizeof(union fat_entry);
    uintptr_t lba;

    if(cluster == 0) {
        size_t sector = offset / entries_per_sector;

        if(sector >= context->rootdir_sectors) {
            return -ENOENT;
        }

        lba = context->data_start - context->rootdir_sectors + sector;
    }
    else {
        size_t entries_per_cluster = context->cluster_size_b / sizeof(union fat_entry);
        size_t cluster_idx = offset / entries_per_cluster;
        size_t entry_idx = offset % entries_per_cluster;
        size_t sector = entry_idx / entries_per_sector;

        for(size_t i = 0; i < cluster_idx; ++i) {
            cluster = fat_cluster_next(context, cluster);

            if(cluster == UINTPTR_MAX) {
                return -ENOENT;
            }
        }

        lba = sector + fat_cluster_lba(context, cluster);
    }

    int retval = blk_device_read(context->device, lba, 1, context->bounce);

    if(retval < 0) {
        return retval;
    }

    memcpy(entry, context->bounce + sizeof(union fat_entry) * (offset % entries_per_sector), sizeof(union fat_entry));

    return 0;
}

static int write_entry(const struct fat_vfs_context* restrict context, uintptr_t cluster, size_t offset,
    const union fat_entry* restrict entry)
{
    size_t entries_per_sector = context->sector_size / sizeof(union fat_entry);
    uintptr_t lba;

    if(cluster == 0) {
        size_t sector = offset / entries_per_sector;

        if(sector >= context->rootdir_sectors) {
            return -ENOENT;
        }

        lba = context->data_start - context->rootdir_sectors + sector;
    }
    else {
        size_t entries_per_cluster = context->cluster_size_b / sizeof(union fat_entry);
        size_t cluster_idx = offset / entries_per_cluster;
        size_t entry_idx = offset % entries_per_cluster;
        size_t sector = entry_idx / entries_per_sector;

        for(size_t i = 0; i < cluster_idx; ++i) {
            cluster = fat_cluster_next(context, cluster);

            if(cluster == UINTPTR_MAX) {
                return -ENOENT;
            }
        }

        lba = sector + fat_cluster_lba(context, cluster);
    }

    int retval = blk_device_read(context->device, lba, 1, context->bounce);

    if(retval < 0) {
        return retval;
    }

    memcpy(context->bounce + sizeof(union fat_entry) * (offset % entries_per_sector), entry, sizeof(union fat_entry));

    return blk_device_write(context->device, lba, 1, context->bounce);
}

static uint8_t ldir_checksum(const struct fat_dir_entry* restrict entry)
{
    uint8_t result = 0;
    uint8_t store;

    for(int i = 0; i < 11; ++i) {
        store = result;

        result += entry->name[i];
        result += store >> 1;

        if(store & 1) {
            result += 0x80;
        }
    }

    return result;
}

int fat_dirent_read(const struct fat_vfs_context* restrict context, uintptr_t cluster, size_t* restrict offset,
    struct fat_dirent* restrict entry)
{
    union fat_entry entry_raw;
    struct fat_ldir_entry ldirs[FAT_LFN_MAX_SEGMENTS];
    unsigned int num_ldir = 0;

    for(;;) {
        int retval = read_entry(context, cluster, *offset, &entry_raw);

        *offset += 1;

        if(retval < 0) {
            return retval;
        }

        if(entry_raw.dir.name[0] == 0x00) {
            return -ENOENT;
        }

        if(entry_raw.dir.name[0] == 0xE5) {
            num_ldir = 0;
            continue;
        }

        if(FAT_ATTR_LONG_NAME == (entry_raw.dir.attribute & FAT_ATTR_LONG_NAME)) {
            if(num_ldir < FAT_LFN_MAX_SEGMENTS) {
                memcpy(&ldirs[num_ldir], &entry_raw.ldir, sizeof(struct fat_ldir_entry));
                num_ldir += 1;
            }
        }
        else if(entry_raw.dir.attribute & FAT_ATTR_VOLUME_ID) {
            num_ldir = 0;
        }
        else {
            memcpy(&entry->base, &entry_raw.dir, sizeof(struct fat_dir_entry));
            break;
        }
    }

    ptrdiff_t name_pos = 0;

    if(num_ldir > 0) {
        bool done = false;

        for(unsigned i = num_ldir; i > 0 && !done; --i) {
            const struct fat_ldir_entry* ldir = &ldirs[i - 1];

            for(unsigned j = 0; j < 5 && name_pos < PATH_NAME_SIZE && !done; ++j) {
                if(ldir->name_1[j] == 0x0000 || ldir->name_1[j] == 0xFFFF) {
                    done = true;
                    break;
                }

                entry->full_name[name_pos] = ldir->name_1[j] & 0xFF; // FIXME: multibyte unicode conversions
                name_pos += 1;
            }

            for(unsigned j = 0; j < 6 && name_pos < PATH_NAME_SIZE && !done; ++j) {
                if(ldir->name_2[j] == 0x0000 || ldir->name_2[j] == 0xFFFF) {
                    done = true;
                    break;
                }

                entry->full_name[name_pos] = ldir->name_2[j] & 0xFF; // FIXME: multibyte unicode conversions
                name_pos += 1;
            }

            for(unsigned j = 0; j < 2 && name_pos < PATH_NAME_SIZE && !done; ++j) {
                if(ldir->name_3[j] == 0x0000 || ldir->name_3[j] == 0xFFFF) {
                    done = true;
                    break;
                }

                entry->full_name[name_pos] = ldir->name_3[j] & 0xFF; // FIXME: multibyte unicode conversions
                name_pos += 1;
            }
        }
    }
    else {
        for(unsigned i = 0; i < 8 && !isspace(entry->base.name[i]); i++) {
            entry->full_name[name_pos] = entry->base.name[i];
            name_pos += 1;
        }

        if(!isspace(entry->base.name[8])) {
            entry->full_name[name_pos] = 0x2E;
            name_pos += 1;

            for(unsigned i = 8; i < 11 && !isspace(entry->base.name[i]); ++i) {
                entry->full_name[name_pos] = entry->base.name[i];
                name_pos += 1;
            }
        }
    }

    entry->full_name[name_pos] = 0;

    return 0;
}

int fat_dirent_write(const struct fat_vfs_context* restrict context, uintptr_t cluster, size_t offset,
    const struct fat_dirent* restrict entry)
{
    size_t name_length = strnlen(entry->full_name, PATH_NAME_MAX);
    uint8_t checksum = ldir_checksum(&entry->base);
    unsigned num_ldir = (name_length + 12) / 13;
    union fat_entry raw_entry;

    for(unsigned int i = num_ldir; i >= 1; --i) {
        size_t base = 13 * (i - 1);
        size_t pos;

        struct fat_ldir_entry* ldir = &raw_entry.ldir;
        memset(ldir, 0, sizeof(struct fat_ldir_entry));

        ldir->order = i;
        ldir->attr = FAT_ATTR_LONG_NAME;
        ldir->checksum = checksum;

        if(i == num_ldir) {
            ldir->order |= UINT8_C(0x40);
        }

        for(unsigned j = 0; j < 5; ++j) {
            pos = base + j;

            if(pos < name_length) {
                ldir->name_1[j] = entry->full_name[pos];
            }
            else if(pos == name_length) {
                ldir->name_1[j] = 0x0000;
            }
            else {
                ldir->name_1[j] = 0xFFFF;
            }
        }

        for(unsigned j = 0; j < 6; ++j) {
            pos = base + j + 5;

            if(pos < name_length) {
                ldir->name_2[j] = entry->full_name[pos];
            }
            else if(pos == name_length) {
                ldir->name_2[j] = 0x0000;
            }
            else {
                ldir->name_2[j] = 0xFFFF;
            }
        }

        for(unsigned j = 0; j < 2; ++j) {
            pos = base + j + 11;

            if(pos < name_length) {
                ldir->name_3[j] = entry->full_name[pos];
            }
            else if(pos == name_length) {
                ldir->name_3[j] = 0x0000;
            }
            else {
                ldir->name_3[j] = 0xFFFF;
            }
        }

        int retval = write_entry(context, cluster, offset - i, &raw_entry);

        if(retval < 0) {
            return retval;
        }
    }

    memcpy(&raw_entry.dir, &entry->base, sizeof(struct fat_dir_entry));

    return write_entry(context, cluster, offset, &raw_entry);
}

int fat_dirent_update(const struct fat_vfs_context* restrict context, uintptr_t cluster, size_t offset,
    const struct fat_dir_entry* restrict entry)
{
    union fat_entry raw;
    memcpy(&raw.dir, entry, sizeof(struct fat_dir_entry));
    return write_entry(context, cluster, offset, &raw);
}

int fat_dirent_find_free(const struct fat_vfs_context* restrict context, uintptr_t cluster, unsigned count,
    size_t* restrict out_offset)
{
    union fat_entry raw;
    unsigned run = 0;
    size_t run_start = 0;
    size_t offset = 0;

    for(;;) {
        int retval = read_entry(context, cluster, offset, &raw);

        if(retval < 0) {
            return retval;
        }

        uint8_t first = raw.dir.name[0];

        if(first == 0x00 || first == 0xE5) {
            if(run == 0) {
                run_start = offset;
            }

            run += 1;

            if(run >= count) {
                *out_offset = run_start;
                return 0;
            }
        }
        else {
            run = 0;
        }

        offset += 1;
    }
}
