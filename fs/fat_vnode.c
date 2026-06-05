#include <core/errno.h>
#include <dev/blk_device.h>
#include <fs/fat.h>
#include <fs/stat.h>
#include <fs/vfs_dirent.h>
#include <fs/vfs_mount.h>
#include <fs/vfs_vnode.h>
#include <lib/ctype.h>
#include <lib/string.h>
#include <lib/strings.h>
#include <mm/slab.h>

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

static bool check_shortpath_char(char c)
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

    for(const char* it = ok; *it; ++it) {
        if(c == *it) {
            return true;
        }
    }

    return false;
}

static void shortpath_parts(const char* restrict filename, char stem[6], unsigned* restrict stem_len, char ext[3],
    unsigned* restrict ext_len)
{
    *stem_len = 0;
    *ext_len = 0;

    const char* last_dot = nullptr;
    for(const char* it = filename; *it; ++it) {
        if(*it == '.') {
            last_dot = it;
        }
    }

    const char* stem_end;

    if(last_dot == nullptr) {
        stem_end = filename + strnlen(filename, PATH_NAME_SIZE);
    }
    else {
        stem_end = last_dot;
    }

    for(const char* it = filename; it != stem_end && *stem_len < 6; ++it) {
        char chr = toupper(*it);

        if(chr == ' ' || chr == '.') {
            continue;
        }

        if(check_shortpath_char(chr)) {
            stem[*stem_len] = chr;
            *stem_len += 1;
        }
    }

    if(last_dot) {
        for(const char* it = last_dot + 1; *it && *ext_len < 3; ++it) {
            char chr = toupper(*it);

            if(chr == ' ') {
                continue;
            }

            if(check_shortpath_char(chr)) {
                ext[*ext_len] = chr;
                *ext_len += 1;
            }
        }
    }
}

static void shortpath_filename(const char* restrict name, unsigned tail_n, uint8_t out[11])
{
    memset(out, 0x20, 11);

    char stem[6], ext[3];
    unsigned stem_len, ext_len;
    shortpath_parts(name, stem, &stem_len, ext, &ext_len);

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

static bool has_shortpath(const struct fat_vfs_context* restrict fs, uintptr_t cluster, const uint8_t shortname[11])
{
    struct fat_dirent entry;
    size_t offset = 0;

    for(;;) {
        int retval = fat_dirent_read(fs, cluster, &offset, &entry);

        if(retval < 0) {
            return false;
        }

        if(0 == memcmp(entry.base.name, shortname, 11)) {
            return true;
        }
    }
}

static void fat_vnode_release(struct vnode* restrict node);
static int fat_vnode_lookup(struct vnode* restrict node, const char* restrict name, ino_t* restrict out_ino);
static int fat_vnode_iterate(struct vnode* restrict node, struct dirent* restrict out, size_t index);
static int fat_vnode_create(struct vnode* restrict node, const char* restrict name, unsigned type, struct vnode** restrict out);
static ssize_t fat_vnode_read(struct vnode* restrict node, void* restrict buffer, size_t sz, uintptr_t offset);
static ssize_t fat_vnode_write(struct vnode* restrict node, const void* restrict buffer, size_t sz, uintptr_t offset);
static int fat_vnode_stat(struct vnode* restrict node, struct __k_stat* restrict out);

const struct vnode_ops fat_vnode_ops = {
    .release = &fat_vnode_release,
    .lookup = &fat_vnode_lookup,
    .iterate = &fat_vnode_iterate,
    .create = &fat_vnode_create,
    .read = &fat_vnode_read,
    .write = &fat_vnode_write,
    .stat = &fat_vnode_stat,
};

static void fat_vnode_release(struct vnode* restrict node)
{
    slab_free(node->ctxdata);
}

static int fat_vnode_lookup(struct vnode* restrict node, const char* restrict name, ino_t* restrict out)
{
    struct fat_vnode_context* context = node->ctxdata;
    struct fat_dirent dirent;
    size_t offset = 0;

    for(;;) {
        int retval = fat_dirent_read(context->fs_ctx, context->first_cluster, &offset, &dirent);

        if(retval < 0) {
            return retval;
        }

        if(strncasecmp(dirent.full_name, name, PATH_NAME_MAX)) {
            continue;
        }

        *out = fat_nodenumber(context->fs_ctx, context->first_cluster, offset - 1); // fat_dirent_read increments the offset

        return 0;
    }

    unreachable();
}

static int fat_vnode_iterate(struct vnode* restrict node, struct dirent* restrict out, size_t index)
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

        if(entry.base.attribute & FAT_ATTR_DIRECTORY) {
            out->nodetype = VNODE_DIRECTORY;
        }
        else {
            out->nodetype = VNODE_FILE;
        }

        memset(out->name, 0, PATH_NAME_SIZE);
        kstrncpy(out->name, entry.full_name, PATH_NAME_SIZE);

        return 0;
    }

    unreachable();
}

static int fat_vnode_create(struct vnode* restrict node, const char* restrict name, unsigned type, struct vnode** restrict out)
{
    struct fat_vnode_context* context = node->ctxdata;
    struct fat_vfs_context* fs = context->fs_ctx;

    if(fs->param_readonly) {
        return -EROFS;
    }

    ino_t existing_nodenumber;

    if(0 == fat_vnode_lookup(node, name, &existing_nodenumber)) {
        return vnode_lookup(node, name, out);
    }

    size_t name_len = strnlen(name, PATH_NAME_SIZE);

    unsigned num_lfn = name_len + 12;
    num_lfn /= 13;

    unsigned slots = num_lfn + 1;

    size_t slot_offset;
    int retval = fat_dirent_find_free(fs, context->first_cluster, slots, &slot_offset);

    if(retval < 0) {
        return retval;
    }

    // Directories need a cluster immediately (for . and ..).
    // Files start with first_cluster=0 and get a cluster on first write —
    // pre-allocating an unused cluster causes fsck to report "chain > file_size".
    uintptr_t first_cluster = 0;

    if(type == VNODE_DIRECTORY) {
        first_cluster = fat_cluster_alloc(fs);

        if(first_cluster == UINTPTR_MAX) {
            return -ENOSPC;
        }
    }

    struct fat_dirent dirent;
    memset(&dirent, 0, sizeof(struct fat_dirent));

    bool found_shortpath = false;
    for(unsigned n = 1; n <= 999; ++n) {
        shortpath_filename(name, n, dirent.base.name);

        if(!has_shortpath(fs, context->first_cluster, dirent.base.name)) {
            found_shortpath = true;
            break;
        }
    }

    if(!found_shortpath) {
        if(first_cluster)
            fat_cluster_free(fs, first_cluster);
        return -ENOSPC;
    }

    kstrncpy(dirent.full_name, name, PATH_NAME_SIZE);

    if(type == VNODE_DIRECTORY) {
        dirent.base.attribute = FAT_ATTR_DIRECTORY;
    }
    else {
        dirent.base.attribute = FAT_ATTR_ARCHIVE;
    }

    dirent.base.first_cluster_hi = first_cluster >> 16;
    dirent.base.first_cluster_lo = first_cluster & 0xFFFF;
    dirent.base.file_size = 0;

    retval = fat_dirent_write(fs, context->first_cluster, slot_offset + num_lfn, &dirent);

    if(retval < 0) {
        if(first_cluster != 0)
            fat_cluster_free(fs, first_cluster);
        return retval;
    }

    // Write . and .. entries into newly created directories
    if(type == VNODE_DIRECTORY) {
        uintptr_t dir_lba = fat_cluster_lba(fs, first_cluster);

        memset(fs->bounce, 0, fs->sector_size);

        for(size_t s = 0; s < fs->cluster_size_s; s++) {
            retval = blk_device_write(fs->device, dir_lba + s, 1, fs->bounce);

            if(retval < 0) {
                fat_cluster_free(fs, first_cluster);
                return retval;
            }
        }

        union fat_entry dot_entry;
        memset(&dot_entry, 0, sizeof(union fat_entry));

        dot_entry.dir.attribute = FAT_ATTR_DIRECTORY;
        dot_entry.dir.first_cluster_hi = first_cluster >> 16;
        dot_entry.dir.first_cluster_lo = first_cluster & 0xFFFF;

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
        dot_entry.dir.first_cluster_hi = parent_cluster >> 16;
        dot_entry.dir.first_cluster_lo = parent_cluster & 0xFFFF;

        retval = fat_dirent_update(fs, first_cluster, 1, &dot_entry.dir);

        if(retval < 0) {
            fat_cluster_free(fs, first_cluster);
            return retval;
        }
    }

    struct fat_vnode_context* out_ctx = slab_alloc(sizeof(struct fat_vnode_context));

    if(out_ctx == nullptr) {
        fat_cluster_free(fs, first_cluster);
        return -ENOMEM;
    }

    out_ctx->fs_ctx = fs;
    out_ctx->first_cluster = first_cluster;
    out_ctx->size_bytes = 0;
    out_ctx->dir_cluster = context->first_cluster;
    out_ctx->dir_offset = slot_offset + num_lfn;

    struct vnode* out_node = vnode_alloc(type, &fat_vnode_ops, out_ctx);

    if(out_node == nullptr) {
        slab_free(out_ctx);
        fat_cluster_free(fs, first_cluster);
        return -ENOMEM;
    }

    out_node->fs = node->fs;
    out_node->nodenumber = fat_nodenumber(fs, context->first_cluster, slot_offset + num_lfn);

    *out = out_node;

    return 0;
}

static ssize_t fat_vnode_read(struct vnode* restrict node, void* restrict buffer, size_t sz, uintptr_t offset)
{
    struct fat_vnode_context* context = node->ctxdata;
    struct fat_vfs_context* fs = context->fs_ctx;

    if(offset >= context->size_bytes) {
        return 0;
    }

    size_t remaining = sz;

    if(offset + sz > context->size_bytes) {
        remaining = context->size_bytes - offset;
    }

    uintptr_t cluster = context->first_cluster;
    size_t cluster_index = offset / fs->cluster_size_b;

    for(size_t i = 0; i < cluster_index; i++) {
        cluster = fat_cluster_next(fs, cluster);

        if(cluster == UINTPTR_MAX) {
            return -EIO;
        }
    }

    size_t offset_in_cluster = offset % fs->cluster_size_b;
    size_t done = 0;

    while(remaining > 0 && cluster != UINTPTR_MAX) {
        size_t sector_index = offset_in_cluster / fs->sector_size;
        size_t offset_in_sector = offset_in_cluster % fs->sector_size;

        while(remaining > 0 && sector_index < fs->cluster_size_s) {
            uintptr_t lba = sector_index + fat_cluster_lba(fs, cluster);
            int retval = blk_device_read(fs->device, lba, 1, fs->bounce);

            if(retval < 0) {
                if(done > 0)
                    return done;
                return retval;
            }

            size_t chunk = fs->sector_size - offset_in_sector;

            if(chunk > remaining) {
                chunk = remaining;
            }

            memcpy((uint8_t*)(buffer) + done, fs->bounce + offset_in_sector, chunk);

            done += chunk;
            remaining -= chunk;
            offset_in_sector = 0;
            sector_index += 1;
        }

        offset_in_cluster = 0;
        cluster = fat_cluster_next(fs, cluster);
    }

    return done;
}

static ssize_t fat_vnode_write(struct vnode* restrict node, const void* restrict buffer, size_t sz, uintptr_t offset)
{
    struct fat_vnode_context* context = node->ctxdata;
    struct fat_vfs_context* fs = context->fs_ctx;

    if(fs->param_readonly) {
        return -EROFS;
    }

    if(context->first_cluster == 0) {
        uintptr_t new_cluster = fat_cluster_alloc(fs);

        if(new_cluster == UINTPTR_MAX) {
            return -ENOSPC;
        }

        context->first_cluster = new_cluster;

        struct fat_dirent dirent;
        size_t dir_off = context->dir_offset;
        int retval = fat_dirent_read(context->fs_ctx, context->dir_cluster, &dir_off, &dirent);

        if(retval < 0) {
            fat_cluster_free(fs, new_cluster);
            context->first_cluster = 0;
            return retval;
        }

        dirent.base.first_cluster_hi = new_cluster >> 16;
        dirent.base.first_cluster_lo = new_cluster & 0xFFFF;
        fat_dirent_update(context->fs_ctx, context->dir_cluster, context->dir_offset, &dirent.base);
    }

    uintptr_t cluster = context->first_cluster;
    size_t cluster_index = offset / fs->cluster_size_b;

    for(size_t i = 0; i < cluster_index; i++) {
        uintptr_t next = fat_cluster_next(fs, cluster);

        if(next == UINTPTR_MAX) {
            next = fat_cluster_alloc(fs);

            if(next == UINTPTR_MAX) {
                return -ENOSPC;
            }

            fat_cluster_link(fs, cluster, next);
        }

        cluster = next;
    }

    size_t offset_in_cluster = offset % fs->cluster_size_b;
    size_t remaining = sz;
    size_t done = 0;

    while(remaining > 0) {
        size_t sector_index = offset_in_cluster / fs->sector_size;
        size_t offset_in_sector = offset_in_cluster % fs->sector_size;

        while(remaining > 0 && sector_index < fs->cluster_size_s) {
            uintptr_t lba = fat_cluster_lba(fs, cluster) + sector_index;
            size_t chunk = fs->sector_size - offset_in_sector;

            if(chunk > remaining) {
                chunk = remaining;
            }

            int retval;

            if(offset_in_sector > 0 || chunk < fs->sector_size) {
                retval = blk_device_read(fs->device, lba, 1, fs->bounce);

                if(retval < 0) {
                    if(done > 0)
                        return done;
                    return retval;
                }
            }

            memcpy(fs->bounce + offset_in_sector, (const uint8_t*)buffer + done, chunk);

            retval = blk_device_write(fs->device, lba, 1, fs->bounce);

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

        if(remaining > 0) {
            uintptr_t next = fat_cluster_next(fs, cluster);

            if(next == UINTPTR_MAX) {
                next = fat_cluster_alloc(fs);

                if(next == UINTPTR_MAX) {
                    if(done > 0)
                        return done;
                    return -ENOSPC;
                }

                fat_cluster_link(fs, cluster, next);
            }

            cluster = next;
            offset_in_cluster = 0;
        }
    }

    if(offset + sz > context->size_bytes) {
        context->size_bytes = offset + sz;

        struct fat_dirent dirent;
        size_t dir_off = context->dir_offset;

        if(0 == fat_dirent_read(context->fs_ctx, context->dir_cluster, &dir_off, &dirent)) {
            dirent.base.file_size = context->size_bytes;

            fat_dirent_update(context->fs_ctx, context->dir_cluster, context->dir_offset, &dirent.base);
        }
    }

    return done;
}

static int fat_vnode_stat(struct vnode* restrict node, struct __k_stat* restrict out)
{
    struct fat_vnode_context* context = node->ctxdata;
    struct fat_vfs_context* fs = context->fs_ctx;

    memset(out, 0, sizeof(struct __k_stat));

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
