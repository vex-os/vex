#include <vex/bitmap.h>
#include <vex/blk_device.h>
#include <vex/ctype.h>
#include <vex/dirent.h>
#include <vex/endian.h>
#include <vex/errno.h>
#include <vex/mount.h>
#include <vex/slab.h>
#include <vex/stat.h>
#include <vex/string.h>
#include <vex/strings.h>
#include <vex/vcache.h>
#include <vex/vnode.h>

#include "fat.h"

static ino_t fat_nodenumber(const struct fat_vfs_context* restrict fs, uintptr_t dir_cluster, size_t index)
{
    size_t num_entries = fs->sector_size / sizeof(union fat_entry);
    uintptr_t entry_sector = index / num_entries;
    uintptr_t entry_offset = index % num_entries * sizeof(union fat_entry);
    uintptr_t lba;

    if(dir_cluster == 0) {
        lba = fs->data_start - fs->rootdir_sectors + entry_sector;
    }
    else {
        lba = entry_sector + fat_cluster_lba(fs, dir_cluster);
    }

    ino_t result = 0;
    result |= lba << 32;
    result |= entry_offset;
    return result;
}

static bool is_shortname_char(char c)
{
    static const char ok[] = "$%-_@~`!(){}^#&";

    if(c >= 128) {
        return true;
    }

    if(c >= 'A' && c <= 'Z') {
        return true;
    }

    if(c >= '0' && c <= '9') {
        return true;
    }

    for(const char* it = ok; *it; it += 1) {
        if(c == *it) {
            return true;
        }
    }

    return false;
}

static void shortname_parts(const char* restrict filename, char stem[6], unsigned* restrict stem_len, char ext[3],
    unsigned* restrict ext_len)
{
    *stem_len = 0;
    *ext_len = 0;

    const char* last_dot = nullptr;
    for(const char* it = filename; *it; it += 1) {
        if(*it == '.') {
            last_dot = it;
        }
    }

    const char* stem_end;

    if(last_dot == nullptr) {
        stem_end = filename + strnlen(filename, NAME_SIZE);
    }
    else {
        stem_end = last_dot;
    }

    for(const char* it = filename; it != stem_end && *stem_len < 6; it += 1) {
        char chr = toupper(*it);

        if(chr == ' ' || chr == '.') {
            continue;
        }

        if(is_shortname_char(chr)) {
            stem[*stem_len] = chr;
            *stem_len += 1;
        }
    }

    if(last_dot) {
        for(const char* it = last_dot + 1; *it && *ext_len < 3; it += 1) {
            char chr = toupper(*it);

            if(chr == ' ') {
                continue;
            }

            if(is_shortname_char(chr)) {
                ext[*ext_len] = chr;
                *ext_len += 1;
            }
        }
    }
}

static void shortname_build(const char* restrict name, unsigned tail_n, uint8_t out[11])
{
    memset(out, 0x20, 11);

    char stem[6], ext[3];
    unsigned stem_len, ext_len;
    shortname_parts(name, stem, &stem_len, ext, &ext_len);

    char tail[5];
    unsigned tail_len = 1;
    tail[0] = '~';

    if(tail_n >= 100) {
        tail[tail_len] = '0' + tail_n / 100;
        tail_len += 1;
    }

    if(tail_n >= 10) {
        tail[tail_len] = '0' + (tail_n / 10) % 10;
        tail_len += 1;
    }

    tail[tail_len] = '0' + tail_n % 10;
    tail_len += 1;

    unsigned stem_max = 8 - tail_len;
    if(stem_len > stem_max) {
        stem_len = stem_max;
    }

    unsigned pos = 0;

    for(unsigned i = 0; i < stem_len; ++i) {
        out[pos] = stem[i];
        pos += 1;
    }

    for(unsigned i = 0; i < tail_len; ++i) {
        out[pos] = tail[i];
        pos += 1;
    }

    for(unsigned i = 0; i < ext_len; ++i) {
        out[8 + i] = ext[i];
    }
}

static void free_chain(struct fat_vfs_context* restrict fs, uintptr_t cluster)
{
    while(cluster != 0 && cluster != UINTPTR_MAX) {
        uintptr_t next = fat_cluster_next(fs, cluster);
        fat_cluster_free(fs, cluster);
        cluster = next;
    }
}

static void fat_vnode_release(struct vnode* restrict node)
{
    struct fat_vnode_context* context = node->ctxdata;

    // Held open past unlink, the clusters
    // stay until the last user is gone
    if(context->unlinked) {
        mutex_acquire(&context->fs_ctx->lock);
        free_chain(context->fs_ctx, context->first_cluster);
        mutex_release(&context->fs_ctx->lock);
    }

    slab_free(context);
}

static int lookup_locked(struct vnode* restrict node, const char* restrict name, ino_t* restrict out)
{
    struct fat_vnode_context* context = node->ctxdata;
    struct fat_dirent dirent;
    size_t offset = 0;

    for(;;) {
        int retval = fat_dirent_read(context->fs_ctx, context->first_cluster, &offset, &dirent);

        if(retval < 0) {
            return retval;
        }

        if(strncasecmp(dirent.full_name, name, NAME_MAX)) {
            continue;
        }

        *out = fat_nodenumber(context->fs_ctx, context->first_cluster, offset - 1); // fat_dirent_read increments the offset

        return 0;
    }

    unreachable();
}

static int iterate_locked(struct vnode* restrict node, struct k_dirent* restrict out, size_t index)
{
    struct fat_vnode_context* context = node->ctxdata;
    struct fat_dirent entry;
    size_t offset = 0;
    size_t visible = 0;

    for(;;) {
        int retval = fat_dirent_read(context->fs_ctx, context->first_cluster, &offset, &entry);

        if(retval < 0) {
            return retval;
        }

        if(entry.base.attribute & FAT_ATTR_VOLUME_ID) {
            continue;
        }

        if(visible < index) {
            visible += 1;
            continue;
        }

        out->d_ino = fat_nodenumber(context->fs_ctx, context->first_cluster, offset - 1); // fat_dirent_read increments the offset

        if(entry.base.attribute & FAT_ATTR_DIRECTORY) {
            out->d_nodetype = VNODE_DIRECTORY;
        }
        else {
            out->d_nodetype = VNODE_FILE;
        }

        memset(out->d_name, 0, NAME_SIZE);
        kstrncpy(out->d_name, entry.full_name, NAME_SIZE);

        return 0;
    }

    unreachable();
}

static int create_locked(struct vnode* restrict node, const char* restrict name, unsigned type, struct vnode** restrict out)
{
    struct fat_vnode_context* context = node->ctxdata;
    struct fat_vfs_context* fs = context->fs_ctx;

    if(fs->param_readonly) {
        return -EROFS;
    }

    ino_t existing_nodenumber;

    if(0 == lookup_locked(node, name, &existing_nodenumber)) {
        return -EEXIST;
    }

    size_t name_len = strnlen(name, NAME_SIZE);

    unsigned num_lfn = name_len + 12;
    num_lfn /= 13;

    unsigned slots = num_lfn + 1;

    size_t slot_offset;
    int retval = fat_dirent_alloc(fs, context->first_cluster, slots, &slot_offset);

    if(retval < 0) {
        return retval;
    }

    uintptr_t first_cluster = 0;

    if(type == VNODE_DIRECTORY) {
        first_cluster = fat_cluster_alloc_zeroed(fs);

        if(first_cluster == UINTPTR_MAX) {
            return -ENOSPC;
        }
    }

    struct fat_dirent dirent;
    memset(&dirent, 0, sizeof(struct fat_dirent));

    bitmap_type taken[32] = { 0 };
    struct fat_dirent taken_dirent;
    size_t taken_offset = 0;

    static_assert(sizeof(taken) * sizeof(bitmap_type) * CHAR_BIT > 999);

    while(0 == fat_dirent_read(fs, context->first_cluster, &taken_offset, &taken_dirent)) {
        int tilde = -1;

        for(int i = 0; i < 7; ++i) {
            if(taken_dirent.base.name[i] == '~') {
                tilde = i;
                break;
            }
        }

        if(tilde < 0) {
            continue;
        }

        unsigned n = 0;

        for(int i = tilde + 1; i < 8 && taken_dirent.base.name[i] != ' '; ++i) {
            uint8_t chr = taken_dirent.base.name[i];

            if(chr < '0' || chr > '9') {
                n = 0;
                break;
            }

            n *= 10;
            n += chr - '0';
        }

        if(n >= 1 && n <= 999) {
            bitmap_set(taken, n);
        }
    }

    bool found = false;

    for(unsigned n = 1; n < 999 && !found; ++n) {
        if(!bitmap_isset(taken, n)) {
            shortname_build(name, n, dirent.base.name);
            found = true;
        }
    }

    if(!found) {
        if(first_cluster)
            fat_cluster_free(fs, first_cluster);
        return -ENOSPC;
    }

    kstrncpy(dirent.full_name, name, NAME_SIZE);

    if(type == VNODE_DIRECTORY) {
        dirent.base.attribute = FAT_ATTR_DIRECTORY;
    }
    else {
        dirent.base.attribute = FAT_ATTR_ARCHIVE;
    }

    dirent.base.first_cluster_hi = htole16(first_cluster >> 16);
    dirent.base.first_cluster_lo = htole16(first_cluster & 0xFFFF);
    dirent.base.file_size = 0;

    retval = fat_dirent_write(fs, context->first_cluster, slot_offset + num_lfn, &dirent);

    if(retval < 0) {
        if(first_cluster != 0)
            fat_cluster_free(fs, first_cluster);
        return retval;
    }

    // Write . and .. entries into newly created directories
    if(type == VNODE_DIRECTORY) {
        union fat_entry dot_entry;
        memset(&dot_entry, 0, sizeof(union fat_entry));

        dot_entry.dir.attribute = FAT_ATTR_DIRECTORY;
        dot_entry.dir.first_cluster_hi = htole16(first_cluster >> 16);
        dot_entry.dir.first_cluster_lo = htole16(first_cluster & 0xFFFF);

        memset(dot_entry.dir.name, 0x20, 11);
        dot_entry.dir.name[0] = '.';

        retval = fat_dirent_update(fs, first_cluster, 0, &dot_entry.dir);

        if(retval < 0) {
            fat_cluster_free(fs, first_cluster);
            return retval;
        }

        memset(dot_entry.dir.name, 0x20, 11);
        dot_entry.dir.name[0] = '.';
        dot_entry.dir.name[1] = '.';

        // For FAT12/16 the parent's cluster field is 0 if parent is root
        uintptr_t parent_cluster = context->first_cluster;
        dot_entry.dir.first_cluster_hi = htole16(parent_cluster >> 16);
        dot_entry.dir.first_cluster_lo = htole16(parent_cluster & 0xFFFF);

        retval = fat_dirent_update(fs, first_cluster, 1, &dot_entry.dir);

        if(retval < 0) {
            fat_cluster_free(fs, first_cluster);
            return retval;
        }
    }

    struct fat_vnode_context* ctx = slab_alloc(sizeof(struct fat_vnode_context));

    if(ctx == nullptr) {
        fat_cluster_free(fs, first_cluster);
        return -ENOMEM;
    }

    ctx->fs_ctx = fs;
    ctx->first_cluster = first_cluster;
    ctx->size_bytes = 0;
    ctx->dir_cluster = context->first_cluster;
    ctx->dir_offset = slot_offset + num_lfn;
    ctx->cached_cluster = first_cluster;
    ctx->cached_cluster_idx = 0;

    struct vnode* vnode = vnode_alloc(type, &fat_vnode_ops, ctx);

    if(vnode == nullptr) {
        slab_free(ctx);
        fat_cluster_free(fs, first_cluster);
        return -ENOMEM;
    }

    vnode->fs = node->fs;
    vnode->nodenumber = fat_nodenumber(fs, context->first_cluster, slot_offset + num_lfn);

    *out = vnode;

    return 0;
}

static int check_empty(const struct fat_vfs_context* restrict fs, uintptr_t cluster)
{
    struct fat_dirent dirent;
    size_t offset = 0;

    for(;;) {
        int retval = fat_dirent_read(fs, cluster, &offset, &dirent);

        if(retval == -ENOENT) {
            return 0;
        }

        if(retval < 0) {
            return retval;
        }

        if(0 == strcmp(dirent.full_name, ".") || 0 == strcmp(dirent.full_name, "..")) {
            continue;
        }

        if(0 == (dirent.base.attribute & FAT_ATTR_VOLUME_ID)) {
            return -ENOTEMPTY;
        }
    }
}

static int unlink_locked(struct vnode* restrict node, const char* restrict name, struct vnode* restrict target)
{
    struct fat_vnode_context* context = node->ctxdata;
    struct fat_vnode_context* target_context = target->ctxdata;
    struct fat_vfs_context* fs = context->fs_ctx;

    if(fs->param_readonly) {
        return -EROFS;
    }

    if(target->nodetype == VNODE_DIRECTORY) {
        int retval = check_empty(fs, target_context->first_cluster);

        if(retval < 0) {
            return retval;
        }
    }

    struct fat_dirent dirent;
    size_t offset = 0;

    for(;;) {
        int retval = fat_dirent_read(fs, context->first_cluster, &offset, &dirent);

        if(retval < 0) {
            return retval;
        }

        if(0 == strncasecmp(dirent.full_name, name, NAME_MAX)) {
            break;
        }
    }

    size_t entry_offset = offset - 1; // fat_dirent_read increments the offset

    // The name was looked up without the lock
    // held, so it may have been replaced since
    if(entry_offset != target_context->dir_offset) {
        return -ENOENT;
    }

    int retval = fat_dirent_remove(fs, context->first_cluster, entry_offset, dirent.num_lfn);

    if(retval < 0) {
        return retval;
    }

    target_context->unlinked = true;
    vcache_remove(target);

    return 0;
}

static ssize_t read_locked(struct vnode* restrict node, void* restrict buffer, size_t sz, off_t offset)
{
    struct fat_vnode_context* context = node->ctxdata;
    struct fat_vfs_context* fs = context->fs_ctx;

    if(context->first_cluster != 0 && !fat_cluster_valid(fs, context->first_cluster)) {
        return -EIO;
    }

    if(offset >= context->size_bytes) {
        return 0;
    }

    size_t remaining = sz;

    if(offset + sz > context->size_bytes) {
        remaining = context->size_bytes - offset;
    }

    size_t cluster_index = offset / fs->cluster_size_b;
    uintptr_t cluster;

    if(cluster_index >= context->cached_cluster_idx) {
        cluster = context->cached_cluster;

        for(size_t i = context->cached_cluster_idx; i < cluster_index; ++i) {
            cluster = fat_cluster_next(fs, cluster);

            if(cluster == UINTPTR_MAX) {
                return -EIO;
            }
        }
    }
    else {
        cluster = context->first_cluster;

        for(size_t i = 0; i < cluster_index; ++i) {
            cluster = fat_cluster_next(fs, cluster);

            if(cluster == UINTPTR_MAX) {
                return -EIO;
            }
        }
    }

    context->cached_cluster_idx = cluster_index;
    context->cached_cluster = cluster;

    size_t offset_in_cluster = offset % fs->cluster_size_b;
    size_t done = 0;

    while(remaining > 0 && cluster != UINTPTR_MAX) {
        size_t sector_index = offset_in_cluster / fs->sector_size;
        size_t offset_in_sector = offset_in_cluster % fs->sector_size;

        while(remaining > 0 && sector_index < fs->cluster_size_s) {
            uintptr_t lba = fat_cluster_lba(fs, cluster) + sector_index;

            if(offset_in_sector == 0 && remaining >= fs->sector_size) {
                size_t full_sectors = remaining / fs->sector_size;
                size_t avail = fs->cluster_size_s - sector_index;

                if(full_sectors > avail) {
                    full_sectors = avail;
                }

                int retval = blk_read(fs->device, lba, full_sectors, (uint8_t*)buffer + done);

                if(retval < 0) {
                    if(done > 0)
                        return done;
                    return retval;
                }

                size_t bytes = full_sectors * fs->sector_size;
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

                size_t chunk = fs->sector_size - offset_in_sector;

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

        offset_in_cluster = 0;
        cluster_index += 1;
        cluster = fat_cluster_next(fs, cluster);
        context->cached_cluster_idx = cluster_index;
        context->cached_cluster = cluster;
    }

    return done;
}

static ssize_t write_locked(struct vnode* restrict node, const void* restrict buffer, size_t sz, off_t offset)
{
    struct fat_vnode_context* context = node->ctxdata;
    struct fat_vfs_context* fs = context->fs_ctx;

    if(fs->param_readonly) {
        return -EROFS;
    }

    if(context->first_cluster != 0 && !fat_cluster_valid(fs, context->first_cluster)) {
        return -EIO;
    }

    if(context->first_cluster == 0) {
        uintptr_t new_cluster = fat_cluster_alloc_zeroed(fs);

        if(new_cluster == UINTPTR_MAX) {
            return -ENOSPC;
        }

        context->first_cluster = new_cluster;
        context->cached_cluster = new_cluster;
        context->cached_cluster_idx = 0;

        // An unlinked file's entry slot
        // may belong to another file by now
        if(!context->unlinked) {
            struct fat_dirent dirent;
            size_t dir_off = context->dir_offset;
            int retval = fat_dirent_read(context->fs_ctx, context->dir_cluster, &dir_off, &dirent);

            if(retval < 0) {
                fat_cluster_free(fs, new_cluster);
                context->first_cluster = 0;
                return retval;
            }

            dirent.base.first_cluster_hi = htole16(new_cluster >> 16);
            dirent.base.first_cluster_lo = htole16(new_cluster & 0xFFFF);
            fat_dirent_update(context->fs_ctx, context->dir_cluster, context->dir_offset, &dirent.base);
        }
    }

    size_t cluster_index = offset / fs->cluster_size_b;
    uintptr_t cluster;

    if(cluster_index >= context->cached_cluster_idx) {
        cluster = context->cached_cluster;

        for(size_t i = context->cached_cluster_idx; i < cluster_index; ++i) {
            uintptr_t next = fat_cluster_next(fs, cluster);

            if(next == UINTPTR_MAX) {
                next = fat_cluster_alloc_zeroed(fs);

                if(next == UINTPTR_MAX) {
                    return -ENOSPC;
                }

                fat_cluster_link(fs, cluster, next);
            }

            cluster = next;
        }
    }
    else {
        cluster = context->first_cluster;

        for(size_t i = 0; i < cluster_index; ++i) {
            uintptr_t next = fat_cluster_next(fs, cluster);

            if(next == UINTPTR_MAX) {
                next = fat_cluster_alloc_zeroed(fs);

                if(next == UINTPTR_MAX) {
                    return -ENOSPC;
                }

                fat_cluster_link(fs, cluster, next);
            }

            cluster = next;
        }
    }

    context->cached_cluster_idx = cluster_index;
    context->cached_cluster = cluster;

    size_t offset_in_cluster = offset % fs->cluster_size_b;
    size_t remaining = sz;
    size_t done = 0;

    while(remaining > 0) {
        size_t sector_index = offset_in_cluster / fs->sector_size;
        size_t offset_in_sector = offset_in_cluster % fs->sector_size;

        while(remaining > 0 && sector_index < fs->cluster_size_s) {
            uintptr_t lba = fat_cluster_lba(fs, cluster) + sector_index;

            if(offset_in_sector == 0 && remaining >= fs->sector_size) {
                size_t full_sectors = remaining / fs->sector_size;
                size_t avail = fs->cluster_size_s - sector_index;

                if(full_sectors > avail) {
                    full_sectors = avail;
                }

                int retval = blk_write(fs->device, lba, full_sectors, (const uint8_t*)buffer + done);

                if(retval < 0) {
                    if(done > 0)
                        return done;
                    return retval;
                }

                size_t bytes = full_sectors * fs->sector_size;
                done += bytes;
                remaining -= bytes;
                sector_index += full_sectors;
            }
            else {
                size_t chunk = fs->sector_size - offset_in_sector;

                if(chunk > remaining) {
                    chunk = remaining;
                }

                int retval;

                if(offset_in_sector > 0 || chunk < fs->sector_size) {
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
            uintptr_t next = fat_cluster_next(fs, cluster);

            if(next == UINTPTR_MAX) {
                next = fat_cluster_alloc_zeroed(fs);

                if(next == UINTPTR_MAX) {
                    if(done > 0)
                        return done;
                    return -ENOSPC;
                }

                fat_cluster_link(fs, cluster, next);
            }

            cluster = next;
            cluster_index += 1;
            offset_in_cluster = 0;
            context->cached_cluster_idx = cluster_index;
            context->cached_cluster = cluster;
        }
    }

    if(offset + sz > context->size_bytes) {
        context->size_bytes = offset + sz;

        struct fat_dirent dirent;
        size_t dir_off = context->dir_offset;

        if(!context->unlinked && 0 == fat_dirent_read(context->fs_ctx, context->dir_cluster, &dir_off, &dirent)) {
            dirent.base.file_size = htole32(context->size_bytes);

            fat_dirent_update(context->fs_ctx, context->dir_cluster, context->dir_offset, &dirent.base);
        }
    }

    return done;
}

static int stat_locked(struct vnode* restrict node, struct k_stat* restrict out)
{
    struct fat_vnode_context* context = node->ctxdata;
    struct fat_vfs_context* fs = context->fs_ctx;

    memset(out, 0, sizeof(struct k_stat));

    out->st_ino = node->nodenumber;
    out->st_uid = fs->param_uid;
    out->st_gid = fs->param_gid;
    out->st_size = context->size_bytes;
    out->st_blksize = fs->cluster_size_b;

    out->st_blocks = context->size_bytes + fs->sector_size - 1;
    out->st_blocks /= fs->sector_size;

    mode_t rw_bits = S_IRUSR | S_IRGRP | S_IROTH;

    if(!fs->param_readonly) {
        rw_bits |= S_IWUSR | S_IWGRP;
    }

    if(node->nodetype == VNODE_DIRECTORY) {
        out->st_mode = S_IFDIR | rw_bits | S_IXUSR | S_IXGRP | S_IXOTH;
    }
    else {
        out->st_mode = S_IFREG | rw_bits;
    }

    return 0;
}

static struct mutex* fs_lock(struct vnode* restrict node)
{
    struct fat_vnode_context* context = node->ctxdata;
    return &context->fs_ctx->lock;
}

static int fat_vnode_lookup(struct vnode* restrict node, const char* restrict name, ino_t* restrict out)
{
    mutex_acquire(fs_lock(node));
    int retval = lookup_locked(node, name, out);
    mutex_release(fs_lock(node));
    return retval;
}

static int fat_vnode_iterate(struct vnode* restrict node, struct k_dirent* restrict out, size_t index)
{
    mutex_acquire(fs_lock(node));
    int retval = iterate_locked(node, out, index);
    mutex_release(fs_lock(node));
    return retval;
}

static int fat_vnode_unlink(struct vnode* restrict node, const char* restrict name, struct vnode* restrict target)
{
    mutex_acquire(fs_lock(node));
    int retval = unlink_locked(node, name, target);
    mutex_release(fs_lock(node));
    return retval;
}

static int fat_vnode_create(struct vnode* restrict node, const char* restrict name, unsigned type, struct vnode** restrict out)
{
    mutex_acquire(fs_lock(node));
    int retval = create_locked(node, name, type, out);
    mutex_release(fs_lock(node));
    return retval;
}

static ssize_t fat_vnode_read(struct vnode* restrict node, void* restrict buffer, size_t sz, off_t offset)
{
    mutex_acquire(fs_lock(node));
    ssize_t retval = read_locked(node, buffer, sz, offset);
    mutex_release(fs_lock(node));
    return retval;
}

static ssize_t fat_vnode_write(struct vnode* restrict node, const void* restrict buffer, size_t sz, off_t offset)
{
    mutex_acquire(fs_lock(node));
    ssize_t retval = write_locked(node, buffer, sz, offset);
    mutex_release(fs_lock(node));
    return retval;
}

static int fat_vnode_stat(struct vnode* restrict node, struct k_stat* restrict out)
{
    mutex_acquire(fs_lock(node));
    int retval = stat_locked(node, out);
    mutex_release(fs_lock(node));
    return retval;
}

const struct vnode_ops fat_vnode_ops = {
    .release = &fat_vnode_release,
    .lookup = &fat_vnode_lookup,
    .iterate = &fat_vnode_iterate,
    .create = &fat_vnode_create,
    .unlink = &fat_vnode_unlink,
    .read = &fat_vnode_read,
    .write = &fat_vnode_write,
    .stat = &fat_vnode_stat,
};
