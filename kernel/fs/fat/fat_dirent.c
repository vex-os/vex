#include <vex/assert.h>
#include <vex/bitmap.h>
#include <vex/blk_device.h>
#include <vex/ctype.h>
#include <vex/endian.h>
#include <vex/errno.h>
#include <vex/fcache.h>
#include <vex/string.h>

#include "fat.h"

static int entry_offset(const fat_vfs_context_t* restrict context, uintptr_t cluster, size_t offset, uint64_t* restrict out)
{
    size_t entries_per_sector = context->sector_size / sizeof(fat_entry_t);
    uintptr_t lba;

    if(cluster == 0) {
        size_t sector = offset / entries_per_sector;

        if(sector >= context->rootdir_sectors) {
            return -ENOENT;
        }

        lba = context->data_start - context->rootdir_sectors + sector;
    }
    else {
        size_t entries_per_cluster = context->cluster_size_b / sizeof(fat_entry_t);
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

    *out = lba * context->sector_size + sizeof(fat_entry_t) * (offset % entries_per_sector);

    return 0;
}

static int read_entry(const fat_vfs_context_t* restrict context, uintptr_t cluster, size_t offset, fat_entry_t* restrict entry)
{
    uint64_t position;
    int retval = entry_offset(context, cluster, offset, &position);

    if(retval < 0) {
        return retval;
    }

    return fcache_dev_read(context->device_node, position, entry, sizeof(fat_entry_t));
}

static int write_entry(const fat_vfs_context_t* restrict context, uintptr_t cluster, size_t offset, const fat_entry_t* restrict entry)
{
    uint64_t position;
    int retval = entry_offset(context, cluster, offset, &position);

    if(retval < 0) {
        return retval;
    }

    return fcache_dev_write(context->device_node, position, entry, sizeof(fat_entry_t));
}

static uint8_t ldir_checksum(const fat_dir_entry_t* restrict entry)
{
    uint8_t result = 0;

    for(int i = 0; i < 11; ++i) {
        result = ((result & 1) ? 0x80 : 0) + (result >> 1) + entry->name[i];
    }

    return result;
}

int fat_dirent_read(const fat_vfs_context_t* restrict context, uintptr_t cluster, size_t* restrict offset, fat_dirent_t* restrict entry)
{
    fat_entry_t entry_raw;
    fat_ldir_entry_t ldirs[FAT_LFN_MAX_SEGMENTS];
    unsigned int num_ldir = 0;

    for(;;) {
        int retval = read_entry(context, cluster, *offset, &entry_raw);

        *offset += 1;

        if(retval < 0) {
            return retval;
        }

        if(entry_raw.dir.name[0] == FAT_NAME_END) {
            return -ENOENT;
        }

        if(entry_raw.dir.name[0] == FAT_NAME_DELETED) {
            num_ldir = 0;
            continue;
        }

        if(FAT_ATTR_LONG_NAME == (entry_raw.dir.attribute & FAT_ATTR_LONG_NAME)) {
            const fat_ldir_entry_t* ldir = &entry_raw.ldir;
            uint8_t ord = ldir->order & ~UINT8_C(0x40);

            if(ord == 0 || ord > FAT_LFN_MAX_SEGMENTS || ldir->type) {
                num_ldir = 0;
                continue;
            }

            if(ldir->order & UINT8_C(0x40)) {
                num_ldir = 0;
            }
            else if(num_ldir == 0 || ord != (uint8_t)(ldirs[num_ldir - 1].order & ~UINT8_C(0x40)) - 1) {
                num_ldir = 0;
                continue;
            }

            if(num_ldir < FAT_LFN_MAX_SEGMENTS) {
                memcpy(&ldirs[num_ldir], ldir, sizeof(fat_ldir_entry_t));
                num_ldir += 1;
            }
        }
        else if(entry_raw.dir.attribute & FAT_ATTR_VOLUME_ID) {
            num_ldir = 0;
        }
        else {
            if(entry_raw.dir.name[0] == FAT_NAME_ESCAPED_E5) {
                entry_raw.dir.name[0] = FAT_NAME_DELETED;
            }

            memcpy(&entry->base, &entry_raw.dir, sizeof(fat_dir_entry_t));

            if(num_ldir > 0) {
                uint8_t expected = ldir_checksum(&entry->base);

                for(unsigned i = 0; i < num_ldir; ++i) {
                    if(ldirs[i].checksum != expected) {
                        num_ldir = 0;
                        break;
                    }
                }
            }

            break;
        }
    }

    entry->num_lfn = num_ldir;

    ptrdiff_t name_pos = 0;

    if(num_ldir > 0) {
        bool done = false;

        for(unsigned i = num_ldir; i > 0 && !done; --i) {
            const fat_ldir_entry_t* ldir = &ldirs[i - 1];

            for(unsigned j = 0; j < 5 && name_pos < NAME_SIZE && !done; ++j) {
                uint16_t ch = le16toh(ldir->name_1[j]);
                if(ch == 0x0000 || ch == 0xFFFF) {
                    done = true;
                    break;
                }

                entry->full_name[name_pos] = ch & 0xFF; // FIXME: multibyte unicode conversions
                name_pos += 1;
            }

            for(unsigned j = 0; j < 6 && name_pos < NAME_SIZE && !done; ++j) {
                uint16_t ch = le16toh(ldir->name_2[j]);
                if(ch == 0x0000 || ch == 0xFFFF) {
                    done = true;
                    break;
                }

                entry->full_name[name_pos] = ch & 0xFF; // FIXME: multibyte unicode conversions
                name_pos += 1;
            }

            for(unsigned j = 0; j < 2 && name_pos < NAME_SIZE && !done; ++j) {
                uint16_t ch = le16toh(ldir->name_3[j]);
                if(ch == 0x0000 || ch == 0xFFFF) {
                    done = true;
                    break;
                }

                entry->full_name[name_pos] = ch & 0xFF; // FIXME: multibyte unicode conversions
                name_pos += 1;
            }
        }
    }
    else {
        for(unsigned i = 0; i < 8 && !isspace(entry->base.name[i]); ++i) {
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

int fat_dirent_write(const fat_vfs_context_t* restrict context, uintptr_t cluster, size_t offset, const fat_dirent_t* restrict entry)
{
    size_t name_length = strnlen(entry->full_name, NAME_MAX);
    uint8_t checksum = ldir_checksum(&entry->base);
    unsigned num_ldir = (name_length + 12) / 13;
    fat_entry_t raw_entry;

    for(unsigned int i = num_ldir; i >= 1; --i) {
        size_t base = 13 * (i - 1);
        size_t pos;

        fat_ldir_entry_t* ldir = &raw_entry.ldir;
        memset(ldir, 0, sizeof(fat_ldir_entry_t));

        ldir->order = i;
        ldir->attr = FAT_ATTR_LONG_NAME;
        ldir->checksum = checksum;

        if(i == num_ldir) {
            ldir->order |= UINT8_C(0x40);
        }

        for(unsigned j = 0; j < 5; ++j) {
            pos = base + j;

            if(pos < name_length) {
                ldir->name_1[j] = htole16((uint16_t)(unsigned char)entry->full_name[pos]);
            }
            else if(pos == name_length) {
                ldir->name_1[j] = htole16(0x0000);
            }
            else {
                ldir->name_1[j] = htole16(0xFFFF);
            }
        }

        for(unsigned j = 0; j < 6; ++j) {
            pos = base + j + 5;

            if(pos < name_length) {
                ldir->name_2[j] = htole16((uint16_t)(unsigned char)entry->full_name[pos]);
            }
            else if(pos == name_length) {
                ldir->name_2[j] = htole16(0x0000);
            }
            else {
                ldir->name_2[j] = htole16(0xFFFF);
            }
        }

        for(unsigned j = 0; j < 2; ++j) {
            pos = base + j + 11;

            if(pos < name_length) {
                ldir->name_3[j] = htole16((uint16_t)(unsigned char)entry->full_name[pos]);
            }
            else if(pos == name_length) {
                ldir->name_3[j] = htole16(0x0000);
            }
            else {
                ldir->name_3[j] = htole16(0xFFFF);
            }
        }

        int retval = write_entry(context, cluster, offset - i, &raw_entry);

        if(retval < 0) {
            return retval;
        }
    }

    memcpy(&raw_entry.dir, &entry->base, sizeof(fat_dir_entry_t));

    return write_entry(context, cluster, offset, &raw_entry);
}

int fat_dirent_update(const fat_vfs_context_t* restrict context, uintptr_t cluster, size_t offset, const fat_dir_entry_t* restrict entry)
{
    fat_entry_t raw;
    memcpy(&raw.dir, entry, sizeof(fat_dir_entry_t));
    return write_entry(context, cluster, offset, &raw);
}

int fat_dirent_remove(const fat_vfs_context_t* restrict context, uintptr_t cluster, size_t offset, unsigned num_lfn)
{
    kassert(offset >= num_lfn);

    // The order byte of a long name entry sits
    // where the first name byte of a short one does
    for(size_t slot = offset - num_lfn; slot <= offset; slot += 1) {
        fat_entry_t raw;
        int retval = read_entry(context, cluster, slot, &raw);

        if(retval < 0) {
            return retval;
        }

        raw.dir.name[0] = FAT_NAME_DELETED;

        retval = write_entry(context, cluster, slot, &raw);

        if(retval < 0) {
            return retval;
        }
    }

    return 0;
}

int fat_dirent_alloc(fat_vfs_context_t* restrict context, uintptr_t cluster, unsigned count, size_t* restrict out_offset)
{
    fat_entry_t raw;
    unsigned run = 0;
    size_t run_start = 0;
    size_t offset = 0;

    for(;;) {
        int retval = read_entry(context, cluster, offset, &raw);

        if(retval == -ENOENT) {
            // The fixed FAT12/FAT16 root area cannot be extended
            if(cluster == 0) {
                return -ENOSPC;
            }

            uintptr_t tail = cluster;

            for(;;) {
                uintptr_t next = fat_cluster_next(context, tail);

                if(next == UINTPTR_MAX) {
                    break;
                }

                tail = next;
            }

            uintptr_t new_cluster = fat_cluster_alloc_dir(context);

            if(new_cluster == UINTPTR_MAX) {
                return -ENOSPC;
            }

            fat_cluster_link(context, tail, new_cluster);

            retval = read_entry(context, cluster, offset, &raw);

            if(retval < 0) {
                return retval;
            }
        }
        else if(retval < 0) {
            return retval;
        }

        uint8_t first = raw.dir.name[0];

        if(first == FAT_NAME_END || first == FAT_NAME_DELETED) {
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
