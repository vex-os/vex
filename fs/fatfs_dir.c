#include "fatfs.h"

#include <core/errno.h>
#include <dev/blk_device.h>
#include <lib/ctype.h>
#include <lib/string.h>

int read_entry(const struct fatfs_context* restrict context, uint32_t first_cluster, size_t offset,
    union fatfs_table_entry* restrict out)
{
    int retval;
    size_t cluster_index;
    size_t entries_count;
    size_t entry_index_cls;
    size_t entry_index_sec;
    size_t i;
    size_t sector_index;
    uint32_t cluster;
    uintptr_t lba;

    if(first_cluster == 0) {
        entries_count = context->sector_size / sizeof(union fatfs_table_entry);
        sector_index = offset / entries_count;

        if(sector_index >= context->root_directory_sectors) {
            return -ENOENT;
        }

        lba = context->data_start_lba;
        lba -= context->root_directory_sectors;
        lba += sector_index;

        retval = blk_device_read(context->device, lba, 1, context->bounce);

        if(retval < 0) {
            return retval;
        }

        i = offset % entries_count;
        i *= sizeof(union fatfs_table_entry);

        memcpy(out, context->bounce + i, sizeof(union fatfs_table_entry));
    }
    else {
        entries_count = context->cluster_size_bytes / sizeof(union fatfs_table_entry);
        entry_index_cls = offset % entries_count;

        entry_index_sec = entry_index_cls;
        entry_index_sec %= context->sector_size / sizeof(union fatfs_table_entry);

        cluster_index = offset / entries_count;

        sector_index = entry_index_cls * sizeof(union fatfs_table_entry);
        sector_index /= context->sector_size;

        cluster = first_cluster;

        for(i = 0; i < cluster_index; ++i) {
            cluster = iterate_cluster(context, cluster);

            if(cluster == UINT32_MAX) {
                return -ENOENT;
            }
        }

        lba = cluster_lba(context, cluster);
        lba += sector_index;

        retval = blk_device_read(context->device, lba, 1, context->bounce);

        if(retval < 0) {
            return retval;
        }

        i = entry_index_sec * sizeof(union fatfs_table_entry);

        memcpy(out, &context->bounce[i], sizeof(union fatfs_table_entry));
    }

    return 0;
}

int read_dirent(const struct fatfs_context* restrict context, uint32_t first_cluster, size_t* restrict offset,
    struct fatfs_dirent* restrict out)
{
    int retval;
    bool lfn_done;
    struct fatfs_table_entry_long lfn[FATFS_LFN_MAX_SEGMENTS];
    struct fatfs_table_entry_long* lfn_it;
    union fatfs_table_entry entry;
    unsigned i;
    unsigned j;
    unsigned num_lfn = 0;
    unsigned name_pos;

    for(;;) {
        retval = read_entry(context, first_cluster, offset[0], &entry);
        offset[0] += 1;

        if(retval < 0) {
            return retval;
        }

        if(entry.basic.name[0] == 0x00) {
            return -ENOENT;
        }

        if(entry.basic.name[0] == 0xE5) {
            num_lfn = 0;
            continue;
        }

        if(FATFS_ATTR_LONG_NAME == (entry.basic.attribute & FATFS_ATTR_LONG_NAME)) {
            if(num_lfn < FATFS_LFN_MAX_SEGMENTS) {
                memcpy(&lfn[num_lfn], &entry.lfn, sizeof(struct fatfs_table_entry_long));
                num_lfn += 1;
            }
        }
        else if(entry.basic.attribute & FATFS_ATTR_VOLUME_ID) {
            num_lfn = 0;
        }
        else {
            memcpy(&out->basic, &entry.basic, sizeof(struct fatfs_table_entry_basic));
            break;
        }
    }

    name_pos = 0;

    if(num_lfn) {
        lfn_done = false;

        for(i = num_lfn; i > 0 && !lfn_done; --i) {
            lfn_it = &lfn[i - 1];

            for(j = 0; j < 5 && name_pos < PATH_NAME_SIZE && !lfn_done; ++j) {
                if(lfn_it->name_1[j] == 0x0000 || lfn_it->name_1[j] == 0xFFFF) {
                    lfn_done = true;
                }
                else {
                    out->full_name[name_pos++] = lfn_it->name_1[j] & 0xFF; // FIXME: multibyte convert
                }
            }

            for(j = 0; j < 6 && name_pos < PATH_NAME_SIZE && !lfn_done; ++j) {
                if(lfn_it->name_2[j] == 0x0000 || lfn_it->name_2[j] == 0xFFFF) {
                    lfn_done = true;
                }
                else {
                    out->full_name[name_pos++] = lfn_it->name_2[j] & 0xFF; // FIXME: multibyte convert
                }
            }

            for(j = 0; j < 2 && name_pos < PATH_NAME_SIZE && !lfn_done; ++j) {
                if(lfn_it->name_3[j] == 0x0000 || lfn_it->name_3[j] == 0xFFFF) {
                    lfn_done = true;
                }
                else {
                    out->full_name[name_pos++] = lfn_it->name_3[j] & 0xFF; // FIXME: multibyte convert
                }
            }
        }
    }
    else {
        for(i = 0; i < 8 && !isspace(entry.basic.name[i]); ++i) {
            out->full_name[name_pos] = entry.basic.name[i];
            name_pos += 1;
        }

        if(!isspace(entry.basic.name[8])) {
            out->full_name[name_pos] = 0x2E;
            name_pos += 1;

            for(i = 8; i < 11 && !isspace(entry.basic.name[i]); ++i) {
                out->full_name[name_pos] = entry.basic.name[i];
                name_pos += 1;
            }
        }
    }

    out->full_name[name_pos] = 0x00;

    return 0;
}
