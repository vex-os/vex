#include <vex/assert.h>
#include <vex/bitmap.h>
#include <vex/blk_device.h>
#include <vex/ctype.h>
#include <vex/dirent.h>
#include <vex/endian.h>
#include <vex/errno.h>
#include <vex/fcache.h>
#include <vex/kprintf.h>
#include <vex/mount.h>
#include <vex/pslab.h>
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
    uintptr_t entry_offset = index % num_entries * sizeof(union fat_entry);
    uintptr_t lba;

    if(dir_cluster == 0) {
        lba = fs->data_start - fs->rootdir_sectors + index / num_entries;
    }
    else {
        size_t cluster_entries = fs->cluster_size_b / sizeof(union fat_entry);
        size_t cluster_idx = index / cluster_entries;
        uintptr_t cluster = dir_cluster;

        for(size_t i = 0; i < cluster_idx; ++i) {
            cluster = fat_cluster_next(fs, cluster);

            kassert(cluster < UINTPTR_MAX);
        }

        lba = fat_cluster_lba(fs, cluster) + index % cluster_entries / num_entries;
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

static bool shortname_pick(const struct fat_vfs_context* restrict fs, uintptr_t dir_cluster, const char* restrict name, uint8_t out[11])
{
    bitmap_type taken[32] = { 0 };
    struct fat_dirent taken_dirent;
    size_t taken_offset = 0;

    static_assert(sizeof(taken) * sizeof(bitmap_type) * CHAR_BIT > 999);

    while(0 == fat_dirent_read(fs, dir_cluster, &taken_offset, &taken_dirent)) {
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

    for(unsigned n = 1; n < 999; ++n) {
        if(!bitmap_isset(taken, n)) {
            shortname_build(name, n, out);
            return true;
        }
    }

    return false;
}

static int find_entry(const struct fat_vfs_context* restrict fs, uintptr_t dir_cluster, const char* restrict name,
    struct fat_dirent* restrict out, size_t* restrict out_offset)
{
    size_t offset = 0;

    for(;;) {
        int retval = fat_dirent_read(fs, dir_cluster, &offset, out);

        if(retval < 0) {
            return retval;
        }

        if(0 == strncasecmp(out->full_name, name, NAME_MAX)) {
            *out_offset = offset - 1;
            return 0;
        }
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
    size_t offset;

    int retval = find_entry(context->fs_ctx, context->first_cluster, name, &dirent, &offset);

    if(retval < 0) {
        return retval;
    }

    *out = fat_nodenumber(context->fs_ctx, context->first_cluster, offset);

    return 0;
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

        out->d_ino = fat_nodenumber(context->fs_ctx, context->first_cluster, offset - 1);

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
        first_cluster = fat_cluster_alloc_dir(fs);

        if(first_cluster == UINTPTR_MAX) {
            return -ENOSPC;
        }
    }

    struct fat_dirent dirent;
    memset(&dirent, 0, sizeof(struct fat_dirent));

    if(!shortname_pick(fs, context->first_cluster, name, dirent.base.name)) {
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

        // A ".." pointing at the root holds 0 on every
        // FAT type, even though the FAT32 root has a real cluster
        uintptr_t parent_cluster = context->first_cluster;

        if(parent_cluster == fs->root_cluster) {
            parent_cluster = 0;
        }
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
    size_t entry_offset;
    int retval = find_entry(fs, context->first_cluster, name, &dirent, &entry_offset);

    if(retval < 0) {
        return retval;
    }

    // The name was looked up without the lock
    // held, so it may have been replaced since
    if(fat_nodenumber(fs, context->first_cluster, entry_offset) != target->nodenumber) {
        return -ENOENT;
    }

    retval = fat_dirent_remove(fs, context->first_cluster, entry_offset, dirent.num_lfn);

    if(retval < 0) {
        return retval;
    }

    target_context->unlinked = true;
    vcache_remove(target);

    return 0;
}

static int set_dotdot(struct fat_vfs_context* restrict fs, uintptr_t dir_cluster, uintptr_t parent_cluster)
{
    struct fat_dirent dotdot;
    size_t offset = 1;
    int retval = fat_dirent_read(fs, dir_cluster, &offset, &dotdot);

    if(retval < 0) {
        return retval;
    }

    if(offset != 2 || 0 != strcmp(dotdot.full_name, "..")) {
        return -EIO;
    }

    // A ".." pointing at the root holds 0 on every
    // FAT type, even though the FAT32 root has a real cluster
    if(parent_cluster == fs->root_cluster) {
        parent_cluster = 0;
    }

    dotdot.base.first_cluster_hi = htole16(parent_cluster >> 16);
    dotdot.base.first_cluster_lo = htole16(parent_cluster & 0xFFFF);

    return fat_dirent_update(fs, dir_cluster, 1, &dotdot.base);
}

static int rename_locked(struct vnode* olddir, const char* oldname, struct vnode* source, struct vnode* newdir, const char* newname,
    struct vnode* victim)
{
    struct fat_vnode_context* old_context = olddir->ctxdata;
    struct fat_vnode_context* new_context = newdir->ctxdata;
    struct fat_vnode_context* source_context = source->ctxdata;
    struct fat_vfs_context* fs = old_context->fs_ctx;

    if(fs->param_readonly) {
        return -EROFS;
    }

    struct fat_dirent old_entry;
    size_t old_offset;
    int retval = find_entry(fs, old_context->first_cluster, oldname, &old_entry, &old_offset);

    if(retval < 0) {
        return retval;
    }

    // Both names were looked up without the lock
    // held, so either may have been replaced since
    if(fat_nodenumber(fs, old_context->first_cluster, old_offset) != source->nodenumber) {
        return -ENOENT;
    }

    struct fat_dirent victim_entry;
    size_t victim_offset;
    retval = find_entry(fs, new_context->first_cluster, newname, &victim_entry, &victim_offset);

    if(retval < 0 && retval != -ENOENT) {
        return retval;
    }

    if(victim == nullptr && retval == 0) {
        return -EEXIST;
    }

    if(victim) {
        if(retval == -ENOENT || fat_nodenumber(fs, new_context->first_cluster, victim_offset) != victim->nodenumber) {
            return -ENOENT;
        }

        if(victim->nodetype == VNODE_DIRECTORY) {
            struct fat_vnode_context* victim_context = victim->ctxdata;
            retval = check_empty(fs, victim_context->first_cluster);

            if(retval < 0) {
                return retval;
            }
        }
    }

    struct fat_dirent entry;
    memset(&entry, 0, sizeof(struct fat_dirent));
    memcpy(&entry.base, &old_entry.base, sizeof(struct fat_dir_entry));
    kstrncpy(entry.full_name, newname, NAME_SIZE);

    if(!shortname_pick(fs, new_context->first_cluster, newname, entry.base.name)) {
        return -ENOSPC;
    }

    unsigned num_lfn = strnlen(newname, NAME_SIZE) + 12;
    num_lfn /= 13;

    size_t slot_offset;
    retval = fat_dirent_alloc(fs, new_context->first_cluster, num_lfn + 1, &slot_offset);

    if(retval < 0) {
        return retval;
    }

    size_t new_offset = slot_offset + num_lfn;
    retval = fat_dirent_write(fs, new_context->first_cluster, new_offset, &entry);

    if(retval < 0) {
        return retval;
    }

    if(victim) {
        retval = fat_dirent_remove(fs, new_context->first_cluster, victim_offset, victim_entry.num_lfn);

        if(retval < 0) {
            pr_error("fat: rename: removing the replaced entry failed (%d)", retval);
        }
    }

    retval = fat_dirent_remove(fs, old_context->first_cluster, old_offset, old_entry.num_lfn);

    if(retval < 0) {
        pr_error("fat: rename: removing the old entry failed (%d)", retval);
    }

    if(source->nodetype == VNODE_DIRECTORY && olddir != newdir) {
        retval = set_dotdot(fs, source_context->first_cluster, new_context->first_cluster);

        if(retval < 0) {
            pr_error("fat: rename: updating \"..\" failed (%d)", retval);
        }
    }

    if(victim) {
        struct fat_vnode_context* victim_context = victim->ctxdata;
        victim_context->unlinked = true;
        vcache_remove(victim);
    }

    source_context->dir_cluster = new_context->first_cluster;
    source_context->dir_offset = new_offset;

    vcache_rekey(source, fat_nodenumber(fs, new_context->first_cluster, new_offset));

    return 0;
}

static int chain_cluster(struct vnode* restrict node, size_t cluster_index, bool create, uintptr_t* restrict out)
{
    struct fat_vnode_context* context = node->ctxdata;
    struct fat_vfs_context* fs = context->fs_ctx;

    if(context->first_cluster == 0) {
        if(!create) {
            return -EIO;
        }

        uintptr_t new_cluster = fat_cluster_alloc_zeroed(fs);

        if(new_cluster == UINTPTR_MAX) {
            return -ENOSPC;
        }

        // An unlinked file's entry slot
        // may belong to another file by now
        if(!context->unlinked) {
            struct fat_dirent dirent;
            size_t dir_off = context->dir_offset;
            int retval = fat_dirent_read(fs, context->dir_cluster, &dir_off, &dirent);

            if(retval < 0) {
                fat_cluster_free(fs, new_cluster);
                return retval;
            }

            dirent.base.first_cluster_hi = htole16(new_cluster >> 16);
            dirent.base.first_cluster_lo = htole16(new_cluster & 0xFFFF);
            fat_dirent_update(fs, context->dir_cluster, context->dir_offset, &dirent.base);
        }

        context->first_cluster = new_cluster;
        context->cached_cluster = new_cluster;
        context->cached_cluster_idx = 0;
    }

    if(!fat_cluster_valid(fs, context->first_cluster)) {
        return -EIO;
    }

    uintptr_t cluster = context->first_cluster;
    size_t index = 0;

    if(cluster_index >= context->cached_cluster_idx && fat_cluster_valid(fs, context->cached_cluster)) {
        cluster = context->cached_cluster;
        index = context->cached_cluster_idx;
    }

    while(index < cluster_index) {
        uintptr_t next = fat_cluster_next(fs, cluster);

        if(next == UINTPTR_MAX) {
            if(!create) {
                return -EIO;
            }

            next = fat_cluster_alloc_zeroed(fs);

            if(next == UINTPTR_MAX) {
                return -ENOSPC;
            }

            fat_cluster_link(fs, cluster, next);
        }

        cluster = next;
        index += 1;
    }

    context->cached_cluster = cluster;
    context->cached_cluster_idx = cluster_index;

    *out = cluster;

    return 0;
}

static int fat_bmap(struct vnode* restrict node, uint64_t sector, bool create, struct fcache_extent* restrict out)
{
    struct fat_vnode_context* context = node->ctxdata;
    struct fat_vfs_context* fs = context->fs_ctx;

    if(fs->sector_size != fs->device->sector_size) {
        return -EIO;
    }

    size_t in_cluster = sector % fs->cluster_size_s;
    uintptr_t cluster;
    int retval = chain_cluster(node, sector / fs->cluster_size_s, create, &cluster);

    if(retval < 0) {
        return retval;
    }

    out->lba = fat_cluster_lba(fs, cluster) + in_cluster;
    out->count = fs->cluster_size_s - in_cluster;
    out->mapped = true;

    return 0;
}

static ssize_t read_locked(struct vnode* restrict node, void* restrict buffer, size_t sz, off_t offset)
{
    struct fat_vnode_context* context = node->ctxdata;

    return fcache_read(node, &fat_bmap, buffer, sz, offset, context->size_bytes);
}

static ssize_t write_span_locked(struct vnode* restrict node, const void* restrict buffer, size_t sz, off_t offset)
{
    struct fat_vnode_context* context = node->ctxdata;
    struct fat_vfs_context* fs = context->fs_ctx;

    if(fs->param_readonly) {
        return -EROFS;
    }

    // A FAT file size is a 32-bit field
    if(offset < 0 || (uint64_t)(offset) + sz > UINT32_MAX) {
        return -EFBIG;
    }

    ssize_t written = fcache_write(node, &fat_bmap, buffer, sz, offset, context->size_bytes);

    if(written <= 0 || offset + written <= (off_t)(context->size_bytes)) {
        return written;
    }

    context->size_bytes = offset + written;

    struct fat_dirent dirent;
    size_t dir_off = context->dir_offset;

    if(!context->unlinked && 0 == fat_dirent_read(fs, context->dir_cluster, &dir_off, &dirent)) {
        dirent.base.file_size = htole32(context->size_bytes);

        fat_dirent_update(fs, context->dir_cluster, context->dir_offset, &dirent.base);
    }

    return written;
}

static int zero_fill_locked(struct vnode* restrict node, off_t from, off_t to)
{
    void* zeroes = pslab_alloc_virt();

    if(zeroes == nullptr) {
        return -ENOMEM;
    }

    memset(zeroes, 0, PAGE_SIZE);

    int retval = 0;

    while(from < to) {
        size_t chunk = to - from;

        if(chunk > PAGE_SIZE) {
            chunk = PAGE_SIZE;
        }

        ssize_t written = write_span_locked(node, zeroes, chunk, from);

        if(written < 0) {
            retval = written;
            break;
        }

        from += written;
    }

    pslab_free_virt(zeroes);

    return retval;
}

// Bytes past the end of the file may hold stale data
// in the last cluster, so a gap is zeroed out on disk
static ssize_t write_locked(struct vnode* restrict node, const void* restrict buffer, size_t sz, off_t offset)
{
    struct fat_vnode_context* context = node->ctxdata;

    if(offset > context->size_bytes) {
        int retval = zero_fill_locked(node, context->size_bytes, offset);

        if(retval < 0) {
            return retval;
        }
    }

    return write_span_locked(node, buffer, sz, offset);
}

static int shrink_locked(struct vnode* restrict node, size_t size)
{
    struct fat_vnode_context* context = node->ctxdata;
    struct fat_vfs_context* fs = context->fs_ctx;

    // Dirty pages past the new end must not
    // reach clusters that are freed below
    fcache_truncate(node, size);

    size_t keep = (size + fs->cluster_size_b - 1) / fs->cluster_size_b;

    if(keep == 0) {
        free_chain(fs, context->first_cluster);
        context->first_cluster = 0;
    }
    else {
        uintptr_t last = context->first_cluster;

        for(size_t i = 1; i < keep; ++i) {
            last = fat_cluster_next(fs, last);

            if(last == UINTPTR_MAX) {
                return -EIO;
            }
        }

        uintptr_t rest = fat_cluster_next(fs, last);

        if(rest != UINTPTR_MAX) {
            fat_cluster_end(fs, last);
            free_chain(fs, rest);
        }
    }

    context->size_bytes = size;
    context->cached_cluster = context->first_cluster;
    context->cached_cluster_idx = 0;

    if(context->unlinked) {
        return 0;
    }

    struct fat_dirent dirent;
    size_t dir_off = context->dir_offset;
    int retval = fat_dirent_read(fs, context->dir_cluster, &dir_off, &dirent);

    if(retval < 0) {
        return retval;
    }

    dirent.base.file_size = htole32(size);
    dirent.base.first_cluster_hi = htole16(context->first_cluster >> 16);
    dirent.base.first_cluster_lo = htole16(context->first_cluster & 0xFFFF);

    return fat_dirent_update(fs, context->dir_cluster, context->dir_offset, &dirent.base);
}

static int setattr_locked(struct vnode* restrict node, const struct vattr* restrict attr)
{
    struct fat_vnode_context* context = node->ctxdata;
    struct fat_vfs_context* fs = context->fs_ctx;

    // FAT has nowhere to keep a mode or an owner; with quiet,
    // the change is dropped instead of failing, as with vfat on Linux
    if((attr->mask & (VATTR_MODE | VATTR_UID | VATTR_GID)) && !fs->param_quiet) {
        return -EPERM;
    }

    if(0 == (attr->mask & VATTR_SIZE) || (size_t)attr->size == context->size_bytes) {
        return 0;
    }

    if(fs->param_readonly) {
        return -EROFS;
    }

    // A FAT file size is a 32-bit field
    if(attr->size > UINT32_MAX) {
        return -EFBIG;
    }

    if((size_t)attr->size > context->size_bytes) {
        return zero_fill_locked(node, context->size_bytes, attr->size);
    }

    return shrink_locked(node, attr->size);
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

    mode_t perm_bits = rw_bits | S_IXUSR | S_IXGRP | S_IXOTH;

    if(node->nodetype == VNODE_DIRECTORY) {
        out->st_mode = S_IFDIR | perm_bits;
    }
    else {
        out->st_mode = S_IFREG | perm_bits;
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

static int fat_vnode_rename(struct vnode* olddir, const char* oldname, struct vnode* source, struct vnode* newdir, const char* newname,
    struct vnode* victim)
{
    mutex_acquire(fs_lock(olddir));
    int retval = rename_locked(olddir, oldname, source, newdir, newname, victim);
    mutex_release(fs_lock(olddir));
    return retval;
}

static int fat_vnode_create(struct vnode* restrict node, const char* restrict name, unsigned type, mode_t mode,
    const struct credentials* restrict cred, struct vnode** restrict out)
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

static int fat_vnode_fsync(struct vnode* restrict node)
{
    struct fat_vnode_context* context = node->ctxdata;

    mutex_acquire(fs_lock(node));
    int retval = fcache_writeback(node, &fat_bmap, context->size_bytes);
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

static int fat_vnode_setattr(struct vnode* restrict node, const struct vattr* restrict attr)
{
    mutex_acquire(fs_lock(node));
    int retval = setattr_locked(node, attr);
    mutex_release(fs_lock(node));
    return retval;
}

const struct vnode_ops fat_vnode_ops = {
    .release = &fat_vnode_release,
    .lookup = &fat_vnode_lookup,
    .iterate = &fat_vnode_iterate,
    .create = &fat_vnode_create,
    .unlink = &fat_vnode_unlink,
    .rename = &fat_vnode_rename,
    .read = &fat_vnode_read,
    .write = &fat_vnode_write,
    .stat = &fat_vnode_stat,
    .setattr = &fat_vnode_setattr,
    .fsync = &fat_vnode_fsync,
};
