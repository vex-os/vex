#include <vex/blk_device.h>
#include <vex/dirent.h>
#include <vex/errno.h>
#include <vex/slab.h>
#include <vex/stat.h>
#include <vex/string.h>
#include <vex/strings.h>
#include <vex/vcache.h>
#include <vex/vnode.h>

#include "echfs.h"

static_assert(sizeof(struct echfs_vnode_context) <= (PAGE_SIZE / 2));

static void free_chain(struct echfs_vfs_context* restrict fs, uint64_t block)
{
    while(block != ECHFS_TABLE_EOC && block != ECHFS_TABLE_FREE && block < ECHFS_TABLE_RESERVED) {
        uint64_t next = fs->fat[block];
        echfs_block_free(fs, block);
        block = next;
    }
}

static void echfs_vnode_release(struct vnode* restrict node)
{
    struct echfs_vnode_context* ctx = node->ctxdata;

    // Held open past unlink, the blocks stay until the last user is gone
    if(ctx->unlinked && node->nodetype != VNODE_DIRECTORY) {
        mutex_acquire(&ctx->fs_ctx->lock);
        free_chain(ctx->fs_ctx, ctx->start_block);
        mutex_release(&ctx->fs_ctx->lock);
    }

    slab_free(ctx);
}

static int lookup_locked(struct vnode* restrict node, const char* restrict name, ino_t* restrict out)
{
    struct echfs_vnode_context* ctx = node->ctxdata;

    uint64_t index;
    int retval = echfs_dirent_find(ctx->fs_ctx, ctx->start_block, name, &index);

    if(retval < 0) {
        return retval;
    }

    *out = index;

    return 0;
}

static int iterate_locked(struct vnode* restrict node, struct k_dirent* restrict out, size_t index)
{
    struct echfs_vnode_context* ctx = node->ctxdata;
    struct echfs_vfs_context* fs = ctx->fs_ctx;
    size_t visible = 0;

    for(uint64_t i = 0; i < fs->dir_entries; ++i) {
        const struct echfs_dirent* entry = &fs->dir_table[i];

        if(entry->directory_id == ECHFS_DIR_ID_END) {
            return -ENOENT;
        }

        if(entry->directory_id == ECHFS_DIR_ID_DELETED) {
            continue;
        }

        if(entry->directory_id != ctx->start_block) {
            continue;
        }

        if(visible < index) {
            visible += 1;
            continue;
        }

        if(entry->object_type == 1) {
            out->d_nodetype = VNODE_DIRECTORY;
        }
        else {
            out->d_nodetype = VNODE_FILE;
        }

        memset(out->d_name, 0, NAME_SIZE);
        kstrncpy(out->d_name, entry->name, NAME_SIZE);

        return 0;
    }

    return -ENOENT;
}

static uint64_t alloc_dir_id(const struct echfs_vfs_context* restrict context)
{
    uint64_t max_id = 0;

    for(uint64_t i = 0; i < context->dir_entries; ++i) {
        const struct echfs_dirent* entry = &context->dir_table[i];

        if(entry->directory_id == ECHFS_DIR_ID_END) {
            break;
        }

        if(entry->directory_id == ECHFS_DIR_ID_DELETED) {
            continue;
        }

        if(entry->object_type == 1 && entry->start_block > max_id && entry->start_block < ECHFS_DIR_ID_DELETED) {
            max_id = entry->start_block;
        }
    }

    return max_id + 1;
}

static int create_locked(struct vnode* restrict node, const char* restrict name, unsigned type, struct vnode** restrict out)
{
    struct echfs_vnode_context* ctx = node->ctxdata;
    struct echfs_vfs_context* fs = ctx->fs_ctx;

    ino_t existing;

    if(0 == lookup_locked(node, name, &existing)) {
        return -EEXIST;
    }

    uint64_t free_index;

    int retval = echfs_dirent_alloc(fs, &free_index);

    if(retval < 0) {
        return retval;
    }

    struct echfs_dirent entry;
    memset(&entry, 0, sizeof(entry));

    entry.directory_id = ctx->start_block;

    if(type == VNODE_DIRECTORY) {
        entry.object_type = 1;
    }
    else {
        entry.object_type = 0;
    }

    kstrncpy(entry.name, name, sizeof(entry.name));

    uint64_t start_block;

    if(type == VNODE_DIRECTORY) {
        start_block = alloc_dir_id(fs);
        entry.start_block = start_block;
        entry.file_size = 0;
    }
    else {
        start_block = ECHFS_TABLE_EOC;
        entry.start_block = ECHFS_TABLE_EOC;
        entry.file_size = 0;
    }

    retval = echfs_dirent_write(fs, free_index, &entry);

    if(retval < 0) {
        return retval;
    }

    struct echfs_vnode_context* child_ctx = slab_alloc(sizeof(struct echfs_vnode_context));

    if(child_ctx == nullptr) {
        return -ENOMEM;
    }

    child_ctx->fs_ctx = fs;
    child_ctx->dir_entry = free_index;
    child_ctx->start_block = start_block;
    child_ctx->size = 0;
    child_ctx->cached_block = start_block;
    child_ctx->cached_block_idx = 0;

    struct vnode* child = vnode_alloc(type, &echfs_vnode_ops, child_ctx);

    if(child == nullptr) {
        slab_free(child_ctx);
        return -ENOMEM;
    }

    child->fs = node->fs;
    child->nodenumber = free_index;

    *out = child;
    return 0;
}

static int unlink_locked(struct vnode* restrict node, const char* restrict name, struct vnode* restrict target)
{
    struct echfs_vnode_context* ctx = node->ctxdata;
    struct echfs_vnode_context* target_ctx = target->ctxdata;
    struct echfs_vfs_context* fs = ctx->fs_ctx;

    uint64_t index;
    int retval = echfs_dirent_find(fs, ctx->start_block, name, &index);

    if(retval < 0) {
        return retval;
    }

    // The name was looked up without the lock
    // held, so it may have been replaced since
    if(index != target_ctx->dir_entry) {
        return -ENOENT;
    }

    struct echfs_dirent entry;
    retval = echfs_dirent_read(fs, index, &entry);

    if(retval < 0) {
        return retval;
    }

    if(entry.object_type == 1) {
        for(uint64_t i = 0; i < fs->dir_entries; ++i) {
            const struct echfs_dirent* check_entry = &fs->dir_table[i];

            if(check_entry->directory_id == ECHFS_DIR_ID_END) {
                break;
            }

            if(check_entry->directory_id == entry.start_block) {
                return -ENOTEMPTY;
            }
        }
    }

    struct echfs_dirent deleted;
    memset(&deleted, 0, sizeof(deleted));
    deleted.directory_id = ECHFS_DIR_ID_DELETED;

    retval = echfs_dirent_write(fs, index, &deleted);

    if(retval < 0) {
        return retval;
    }

    target_ctx->unlinked = true;
    vcache_remove(target);

    return 0;
}

static ssize_t read_locked(struct vnode* restrict node, void* restrict buffer, size_t sz, off_t offset)
{
    struct echfs_vnode_context* ctx = node->ctxdata;
    struct echfs_vfs_context* fs = ctx->fs_ctx;

    if(offset >= ctx->size) {
        return 0;
    }

    size_t remaining = sz;

    if(offset + sz > ctx->size) {
        remaining = ctx->size - offset;
    }

    uint64_t block_index = offset / fs->block_size;
    uintptr_t block;

    if(block_index >= ctx->cached_block_idx) {
        block = ctx->cached_block;

        for(uint64_t i = ctx->cached_block_idx; i < block_index; ++i) {
            block = echfs_block_next(fs, block);

            if(block == UINTPTR_MAX) {
                return -EIO;
            }
        }
    }
    else {
        block = ctx->start_block;

        for(uint64_t i = 0; i < block_index; ++i) {
            block = echfs_block_next(fs, block);

            if(block == UINTPTR_MAX) {
                return -EIO;
            }
        }
    }

    ctx->cached_block_idx = block_index;
    ctx->cached_block = block;

    size_t offset_in_block = offset % fs->block_size;
    size_t sector_size = fs->device->sector_size;
    size_t done = 0;

    while(remaining > 0 && block != UINTPTR_MAX) {
        size_t sector_index = offset_in_block / sector_size;
        size_t offset_in_sector = offset_in_block % sector_size;

        while(remaining > 0 && sector_index < fs->sectors_per_block) {
            uintptr_t lba = echfs_block_lba(fs, block) + sector_index;

            if(offset_in_sector == 0 && remaining >= sector_size) {
                size_t full_sectors = remaining / sector_size;
                size_t avail = fs->sectors_per_block - sector_index;

                if(full_sectors > avail) {
                    full_sectors = avail;
                }

                int retval = blk_read(fs->device, lba, full_sectors, (uint8_t*)buffer + done);

                if(retval < 0) {
                    if(done > 0)
                        return done;
                    return retval;
                }

                size_t bytes = full_sectors * sector_size;

                done += bytes;
                remaining -= bytes;
                sector_index += full_sectors;
            }
            else {
                int retval = blk_read(fs->device, lba, 1, fs->bounce);

                if(retval < 0) {
                    if(done > 0)
                        return done;
                    return retval;
                }

                size_t chunk = sector_size - offset_in_sector;

                if(chunk > remaining) {
                    chunk = remaining;
                }

                memcpy((uint8_t*)buffer + done, fs->bounce + offset_in_sector, chunk);

                done += chunk;
                remaining -= chunk;
                offset_in_sector = 0;
                sector_index += 1;
            }
        }

        offset_in_block = 0;
        block_index += 1;
        block = echfs_block_next(fs, block);
        ctx->cached_block_idx = block_index;
        ctx->cached_block = block;
    }

    return done;
}

static ssize_t write_locked(struct vnode* restrict node, const void* restrict buffer, size_t sz, off_t offset)
{
    struct echfs_vnode_context* ctx = node->ctxdata;
    struct echfs_vfs_context* fs = ctx->fs_ctx;

    if(ctx->start_block == ECHFS_TABLE_EOC) {
        uintptr_t new_block = echfs_block_alloc(fs);

        if(new_block == UINTPTR_MAX) {
            return -ENOSPC;
        }

        ctx->start_block = new_block;
        ctx->cached_block = new_block;
        ctx->cached_block_idx = 0;

        // An unlinked file's entry may
        // belong to another file by now
        if(!ctx->unlinked) {
            struct echfs_dirent entry;
            int retval = echfs_dirent_read(fs, ctx->dir_entry, &entry);

            if(retval < 0) {
                echfs_block_free(fs, new_block);
                ctx->start_block = ECHFS_TABLE_EOC;
                return retval;
            }

            entry.start_block = new_block;
            echfs_dirent_write(fs, ctx->dir_entry, &entry);
        }
    }

    uint64_t block_index = offset / fs->block_size;
    uintptr_t block;

    if(block_index >= ctx->cached_block_idx) {
        block = ctx->cached_block;

        for(uint64_t i = ctx->cached_block_idx; i < block_index; ++i) {
            uintptr_t next = echfs_block_next(fs, block);

            if(next == UINTPTR_MAX) {
                next = echfs_block_alloc(fs);

                if(next == UINTPTR_MAX) {
                    return -ENOSPC;
                }

                echfs_block_link(fs, block, next);
            }

            block = next;
        }
    }
    else {
        block = ctx->start_block;

        for(uint64_t i = 0; i < block_index; ++i) {
            uintptr_t next = echfs_block_next(fs, block);
            if(next == UINTPTR_MAX) {
                next = echfs_block_alloc(fs);

                if(next == UINTPTR_MAX) {
                    return -ENOSPC;
                }

                echfs_block_link(fs, block, next);
            }

            block = next;
        }
    }

    ctx->cached_block_idx = block_index;
    ctx->cached_block = block;

    size_t offset_in_block = offset % fs->block_size;
    size_t sector_size = fs->device->sector_size;
    size_t remaining = sz;
    size_t done = 0;

    while(remaining > 0) {
        size_t sector_index = offset_in_block / sector_size;
        size_t offset_in_sector = offset_in_block % sector_size;

        while(remaining > 0 && sector_index < fs->sectors_per_block) {
            uintptr_t lba = echfs_block_lba(fs, block) + sector_index;

            if(offset_in_sector == 0 && remaining >= sector_size) {
                size_t full_sectors = remaining / sector_size;
                size_t avail = fs->sectors_per_block - sector_index;

                if(full_sectors > avail) {
                    full_sectors = avail;
                }

                int retval = blk_write(fs->device, lba, full_sectors, (const uint8_t*)buffer + done);

                if(retval < 0) {
                    if(done > 0)
                        return done;
                    return retval;
                }

                size_t bytes = full_sectors * sector_size;

                done += bytes;
                remaining -= bytes;
                sector_index += full_sectors;
            }
            else {
                size_t chunk = sector_size - offset_in_sector;

                if(chunk > remaining) {
                    chunk = remaining;
                }

                int retval;

                if(offset_in_sector > 0 || chunk < sector_size) {
                    retval = blk_read(fs->device, lba, 1, fs->bounce);

                    if(retval < 0) {
                        if(done > 0)
                            return done;
                        return retval;
                    }
                }

                memcpy(fs->bounce + offset_in_sector, (const uint8_t*)buffer + done, chunk);

                retval = blk_write(fs->device, lba, 1, fs->bounce);

                if(retval < 0) {
                    if(done > 0)
                        return done;
                    return retval;
                }

                done += chunk;
                remaining -= chunk;
                offset_in_sector = 0;
                sector_index += 1;
            }
        }

        if(remaining > 0) {
            uintptr_t next = echfs_block_next(fs, block);

            if(next == UINTPTR_MAX) {
                next = echfs_block_alloc(fs);

                if(next == UINTPTR_MAX) {
                    if(done > 0)
                        return done;
                    return -ENOSPC;
                }

                echfs_block_link(fs, block, next);
            }

            block = next;
            block_index += 1;
            offset_in_block = 0;
            ctx->cached_block_idx = block_index;
            ctx->cached_block = block;
        }
    }

    if(offset + sz > ctx->size) {
        ctx->size = offset + sz;

        struct echfs_dirent entry;

        if(!ctx->unlinked && 0 == echfs_dirent_read(fs, ctx->dir_entry, &entry)) {
            entry.file_size = ctx->size;

            echfs_dirent_write(fs, ctx->dir_entry, &entry);
        }
    }

    return done;
}

static int stat_locked(struct vnode* restrict node, struct k_stat* restrict out)
{
    struct echfs_vnode_context* ctx = node->ctxdata;
    struct echfs_vfs_context* fs = ctx->fs_ctx;

    memset(out, 0, sizeof(struct k_stat));

    out->st_ino = node->nodenumber;
    out->st_size = ctx->size;
    out->st_blksize = fs->block_size;
    out->st_blocks = (ctx->size + fs->device->sector_size - 1) / fs->device->sector_size;

    if(ctx->dir_entry != UINT64_MAX && !ctx->unlinked) {
        const struct echfs_dirent* entry = &fs->dir_table[ctx->dir_entry];

        out->st_uid = entry->owner_uid;
        out->st_gid = entry->owner_gid;
        out->st_mode = entry->permissions;
        out->st_atime = entry->access_time;
        out->st_mtime = entry->modify_time;
        out->st_ctime = entry->create_time;
    }

    if(node->nodetype == VNODE_DIRECTORY) {
        out->st_mode = (out->st_mode & ~S_IFMT) | S_IFDIR;
    }
    else {
        out->st_mode = (out->st_mode & ~S_IFMT) | S_IFREG;
    }

    out->st_nlink = 1;

    return 0;
}

static struct mutex* fs_lock(struct vnode* restrict node)
{
    struct echfs_vnode_context* context = node->ctxdata;
    return &context->fs_ctx->lock;
}

static int echfs_vnode_lookup(struct vnode* restrict node, const char* restrict name, ino_t* restrict out)
{
    mutex_acquire(fs_lock(node));
    int retval = lookup_locked(node, name, out);
    mutex_release(fs_lock(node));
    return retval;
}

static int echfs_vnode_iterate(struct vnode* restrict node, struct k_dirent* restrict out, size_t index)
{
    mutex_acquire(fs_lock(node));
    int retval = iterate_locked(node, out, index);
    mutex_release(fs_lock(node));
    return retval;
}

static int echfs_vnode_create(struct vnode* restrict node, const char* restrict name, unsigned type, struct vnode** restrict out)
{
    mutex_acquire(fs_lock(node));
    int retval = create_locked(node, name, type, out);
    mutex_release(fs_lock(node));
    return retval;
}

static int echfs_vnode_unlink(struct vnode* restrict node, const char* restrict name, struct vnode* restrict target)
{
    mutex_acquire(fs_lock(node));
    int retval = unlink_locked(node, name, target);
    mutex_release(fs_lock(node));
    return retval;
}

static ssize_t echfs_vnode_read(struct vnode* restrict node, void* restrict buffer, size_t sz, off_t offset)
{
    mutex_acquire(fs_lock(node));
    ssize_t retval = read_locked(node, buffer, sz, offset);
    mutex_release(fs_lock(node));
    return retval;
}

static ssize_t echfs_vnode_write(struct vnode* restrict node, const void* restrict buffer, size_t sz, off_t offset)
{
    mutex_acquire(fs_lock(node));
    ssize_t retval = write_locked(node, buffer, sz, offset);
    mutex_release(fs_lock(node));
    return retval;
}

static int echfs_vnode_stat(struct vnode* restrict node, struct k_stat* restrict out)
{
    mutex_acquire(fs_lock(node));
    int retval = stat_locked(node, out);
    mutex_release(fs_lock(node));
    return retval;
}

const struct vnode_ops echfs_vnode_ops = {
    .release = &echfs_vnode_release,
    .lookup = &echfs_vnode_lookup,
    .iterate = &echfs_vnode_iterate,
    .create = &echfs_vnode_create,
    .unlink = &echfs_vnode_unlink,
    .read = &echfs_vnode_read,
    .write = &echfs_vnode_write,
    .stat = &echfs_vnode_stat,
};
