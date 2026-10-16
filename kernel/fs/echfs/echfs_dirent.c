#include <vex/errno.h>
#include <vex/string.h>

#include "echfs.h"

int echfs_dirent_find(const struct echfs_vfs_context* restrict context, uint64_t parent_id, const char* restrict name,
    uint64_t* restrict out_index)
{
    for(uint64_t i = 0; i < context->dir_entries; ++i) {
        const struct echfs_dirent* entry = &context->dir_table[i];

        if(entry->directory_id == ECHFS_DIR_ID_END) {
            return -ENOENT;
        }

        if(entry->directory_id == ECHFS_DIR_ID_DELETED) {
            continue;
        }

        if(entry->directory_id != parent_id) {
            continue;
        }

        if(0 == strncmp(entry->name, name, sizeof(entry->name))) {
            *out_index = i;
            return 0;
        }
    }

    return -ENOENT;
}

int echfs_dirent_read(const struct echfs_vfs_context* restrict context, uint64_t index, struct echfs_dirent* restrict out)
{
    if(index >= context->dir_entries) {
        return -EINVAL;
    }

    memcpy(out, &context->dir_table[index], sizeof(struct echfs_dirent));
    return 0;
}

int echfs_dirent_write(struct echfs_vfs_context* restrict context, uint64_t index, const struct echfs_dirent* restrict entry)
{
    if(index >= context->dir_entries) {
        return -EINVAL;
    }

    memcpy(&context->dir_table[index], entry, sizeof(struct echfs_dirent));
    echfs_mark_dirty(context, context->dir_dirty, index * sizeof(struct echfs_dirent), sizeof(struct echfs_dirent));
    return 0;
}

int echfs_dirent_alloc(struct echfs_vfs_context* restrict context, uint64_t* restrict out_index)
{
    for(uint64_t i = 0; i < context->dir_entries; ++i) {
        const struct echfs_dirent* entry = &context->dir_table[i];

        if(entry->directory_id == ECHFS_DIR_ID_END || entry->directory_id == ECHFS_DIR_ID_DELETED) {
            *out_index = i;
            return 0;
        }
    }

    return -ENOSPC;
}
