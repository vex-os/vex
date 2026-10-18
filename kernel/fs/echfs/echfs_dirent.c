#include <vex/errno.h>
#include <vex/pcache.h>
#include <vex/string.h>

#include "echfs.h"

static uint64_t dirent_offset(const struct echfs_vfs_context* restrict context, uint64_t index)
{
    return context->dir_start * context->block_size + index * sizeof(struct echfs_dirent);
}

int echfs_dirent_find(const struct echfs_vfs_context* restrict context, uint64_t parent_id, const char* restrict name,
    uint64_t* restrict out_index)
{
    struct echfs_dirent entry;

    for(uint64_t i = 0; i < context->dir_entries; ++i) {
        int retval = echfs_dirent_read(context, i, &entry);

        if(retval < 0) {
            return retval;
        }

        if(entry.directory_id == ECHFS_DIR_ID_END) {
            return -ENOENT;
        }

        if(entry.directory_id == ECHFS_DIR_ID_DELETED) {
            continue;
        }

        if(entry.directory_id != parent_id) {
            continue;
        }

        if(0 == strncmp(entry.name, name, sizeof(entry.name))) {
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

    return pcache_meta_read(context->meta, dirent_offset(context, index), out, sizeof(struct echfs_dirent));
}

int echfs_dirent_write(struct echfs_vfs_context* restrict context, uint64_t index, const struct echfs_dirent* restrict entry)
{
    if(index >= context->dir_entries) {
        return -EINVAL;
    }

    return pcache_meta_write(context->meta, dirent_offset(context, index), entry, sizeof(struct echfs_dirent));
}

int echfs_dirent_alloc(struct echfs_vfs_context* restrict context, uint64_t* restrict out_index)
{
    struct echfs_dirent entry;

    for(uint64_t i = 0; i < context->dir_entries; ++i) {
        int retval = echfs_dirent_read(context, i, &entry);

        if(retval < 0) {
            return retval;
        }

        if(entry.directory_id == ECHFS_DIR_ID_END || entry.directory_id == ECHFS_DIR_ID_DELETED) {
            *out_index = i;
            return 0;
        }
    }

    return -ENOSPC;
}
