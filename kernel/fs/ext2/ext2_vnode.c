#include <vex/blk_device.h>
#include <vex/errno.h>
#include <vex/fcache.h>
#include <vex/kprintf.h>
#include <vex/mount.h>
#include <vex/mutex.h>
#include <vex/pslab.h>
#include <vex/slab.h>
#include <vex/string.h>
#include <vex/strings.h>
#include <vex/vcache.h>
#include <vex/vnode.h>
#include <vex/wallclock.h>

#include "ext2.h"

static struct mutex* fs_lock(struct vnode* restrict node)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    return &ctx->fs_ctx->lock;
}

static bool is_fast_symlink(const struct ext2_vfs_context* restrict fs_ctx, const struct ext2_inode* restrict inode)
{
    uint32_t xattr_sectors = 0;

    if(inode->i_file_acl) {
        xattr_sectors = fs_ctx->block_size / 512;
    }

    return xattr_sectors == le32toh(inode->i_blocks);
}

static int entry_nodetype(struct ext2_vfs_context* restrict fs_ctx, const struct ext2_dirent* restrict entry, unsigned* restrict out)
{
    switch(entry->file_type) {
        case EXT2_FT_REG_FILE:
            *out = VNODE_FILE;
            return 0;

        case EXT2_FT_DIR:
            *out = VNODE_DIRECTORY;
            return 0;

        case EXT2_FT_SYMLINK:
            *out = VNODE_SYM_LINK;
            return 0;

        case EXT2_FT_UNKNOWN:
            break;

        default:
            *out = VNODE_FILE; // No vnode types for devices, FIFOs and sockets yet
            return 0;
    }

    struct ext2_inode inode;
    int retval = ext2_inode_read(fs_ctx, entry->ino, &inode);

    if(retval < 0) {
        return retval;
    }

    if(ext2_inode_nodetype(le16toh(inode.i_mode), out) < 0) {
        *out = VNODE_FILE;
    }

    return 0;
}

static int lookup_locked(struct vnode* restrict node, const char* restrict name, ino_t* restrict out)
{
    if(node->nodetype != VNODE_DIRECTORY) {
        return -ENOTDIR;
    }

    if(strlen(name) > EXT2_NAME_LEN) {
        return -ENAMETOOLONG;
    }

    struct ext2_dirent entry;
    int retval = ext2_dirent_find(node, name, &entry);

    if(retval < 0) {
        return retval;
    }

    *out = entry.ino;

    return 0;
}

static int iterate_locked(struct vnode* restrict node, struct k_dirent* restrict out, size_t index)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    struct ext2_dirent entry;
    uint64_t offset = 0;
    size_t visible = 0;

    if(node->nodetype != VNODE_DIRECTORY) {
        return -ENOTDIR;
    }

    for(;;) {
        int retval = ext2_dirent_read(node, &offset, &entry);

        if(retval < 0) {
            return retval;
        }

        if(visible < index) {
            visible += 1;
            continue;
        }

        unsigned nodetype;
        retval = entry_nodetype(ctx->fs_ctx, &entry, &nodetype);

        if(retval < 0) {
            return retval;
        }

        out->d_ino = entry.ino;
        out->d_nodetype = nodetype;

        memset(out->d_name, 0, NAME_SIZE);
        kstrncpy(out->d_name, entry.name, NAME_SIZE);

        return 0;
    }
}

static ssize_t readlink_locked(struct vnode* restrict node, char* restrict buf, size_t sz)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    const struct ext2_inode* inode = &ctx->inode;
    uint64_t size = ext2_inode_size(inode);

    if(node->nodetype != VNODE_SYM_LINK) {
        return -EINVAL;
    }

    if(sz > size) {
        sz = size;
    }

    if(!is_fast_symlink(ctx->fs_ctx, inode)) {
        return fcache_read(node, &ext2_bmap, buf, sz, 0, size);
    }

    if(size > sizeof(inode->i_block)) {
        return -EIO;
    }

    memcpy(buf, inode->i_block, sz);

    return sz;
}

static int write_inode_locked(struct vnode* restrict node)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    return ext2_inode_write(ctx->fs_ctx, (uint32_t)(node->nodenumber), &ctx->inode);
}

// The new times reach the disk with the next inode write
static void touch_locked(struct vnode* restrict node, bool modified)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    uint32_t now = (uint32_t)(wallclock_now());

    ctx->inode.i_ctime = htole32(now);

    if(modified) {
        ctx->inode.i_mtime = htole32(now);
    }
}

static void touch_dir_locked(struct vnode* restrict dir)
{
    touch_locked(dir, true);
    write_inode_locked(dir);
}

static bool block_mapped_locked(struct vnode* restrict node, uint64_t pos)
{
    struct fcache_extent extent;
    size_t sector_size = node->fs->device->sector_size;
    return ext2_bmap(node, pos / sector_size, false, &extent) == 0 && extent.mapped;
}

static ssize_t write_span_locked(struct vnode* restrict node, const void* restrict buffer, size_t sz, uint64_t offset)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    uint64_t size = ext2_inode_size(&ctx->inode);
    ssize_t written = fcache_write(node, &ext2_bmap, buffer, sz, offset, size);

    if(written <= 0 || offset + written <= size) {
        return written;
    }

    int retval = ext2_inode_set_size(ctx->fs_ctx, &ctx->inode, offset + written);

    if(retval == 0) {
        retval = write_inode_locked(node);
    }

    if(retval < 0) {
        return retval;
    }

    return written;
}

static int zero_fill_locked(struct vnode* restrict node, uint64_t from, uint64_t to)
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

static int fill_hole_locked(struct vnode* restrict node, uint64_t pos, uint64_t limit)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    size_t block_size = ctx->fs_ctx->block_size;

    if(block_mapped_locked(node, pos)) {
        return 0;
    }

    uint64_t start = pos - pos % block_size;
    uint64_t stop = start + block_size;

    if(stop > limit) {
        stop = limit;
    }

    return zero_fill_locked(node, start, stop);
}

static int clear_tail_locked(struct vnode* restrict node, uint64_t size, uint64_t new_size)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    size_t block_size = ctx->fs_ctx->block_size;

    if(0 == size % block_size || !block_mapped_locked(node, size)) {
        return 0;
    }

    uint64_t stop = size - size % block_size + block_size;

    if(stop > new_size) {
        stop = new_size;
    }

    return zero_fill_locked(node, size, stop);
}

static ssize_t write_locked(struct vnode* restrict node, const void* restrict buffer, size_t sz, off_t offset)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    size_t block_size = ctx->fs_ctx->block_size;

    if(offset < 0) {
        return -EINVAL;
    }

    if(sz == 0) {
        return 0;
    }

    uint64_t size = ext2_inode_size(&ctx->inode);
    uint64_t end = offset + sz;
    uint64_t limit = size;

    if(end > limit) {
        limit = end;
    }

    int retval = 0;

    if((uint64_t)offset > size) {
        retval = clear_tail_locked(node, size, offset);
    }

    if(retval == 0) {
        retval = fill_hole_locked(node, offset, limit);
    }

    if(retval == 0 && (end - 1) / block_size != (uint64_t)offset / block_size) {
        retval = fill_hole_locked(node, end - 1, limit);
    }

    if(retval < 0) {
        return retval;
    }

    ssize_t written = write_span_locked(node, buffer, sz, offset);

    if(written > 0) {
        touch_locked(node, true);
        retval = write_inode_locked(node);
    }

    if(retval < 0) {
        return retval;
    }

    return written;
}

static int resize_locked(struct vnode* restrict node, uint64_t new_size)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    uint64_t size = ext2_inode_size(&ctx->inode);
    int retval = 0;

    if(new_size > size) {
        retval = clear_tail_locked(node, size, new_size);
    }
    else if(new_size < size) {
        fcache_truncate(node, new_size);

        retval = ext2_truncate_blocks(node, new_size);
    }

    if(retval == 0) {
        retval = ext2_inode_set_size(ctx->fs_ctx, &ctx->inode, new_size);
    }

    if(retval == 0) {
        retval = write_inode_locked(node);
    }

    return retval;
}

static int setattr_locked(struct vnode* restrict node, const struct vattr* restrict attr)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    struct ext2_vfs_context* fs_ctx = ctx->fs_ctx;
    struct ext2_inode* inode = &ctx->inode;

    if(attr->mask & (VATTR_UID | VATTR_GID)) {
        uid_t uid = ext2_inode_uid(fs_ctx, inode);
        gid_t gid = ext2_inode_gid(fs_ctx, inode);

        if(attr->mask & VATTR_UID) {
            uid = attr->uid;
        }

        if(attr->mask & VATTR_GID) {
            gid = attr->gid;
        }

        if(!ext2_inode_set_owner(fs_ctx, inode, uid, gid)) {
            return -EINVAL;
        }
    }

    if(attr->mask & VATTR_MODE) {
        uint16_t mode = le16toh(inode->i_mode);
        inode->i_mode = htole16((mode & S_IFMT) | (attr->mode & ~S_IFMT));
    }

    if(attr->mask) {
        touch_locked(node, attr->mask & VATTR_SIZE);
    }

    if(attr->mask & VATTR_ATIME) {
        inode->i_atime = htole32((uint32_t)(attr->atime));
    }

    if(attr->mask & VATTR_MTIME) {
        inode->i_mtime = htole32((uint32_t)(attr->mtime));
    }

    if(attr->mask & VATTR_SIZE) {
        return resize_locked(node, attr->size);
    }

    if(attr->mask) {
        return write_inode_locked(node);
    }

    return 0;
}

static uint8_t file_type_of(uint16_t mode)
{
    switch(mode & S_IFMT) {
        case S_IFREG:
            return EXT2_FT_REG_FILE;

        case S_IFDIR:
            return EXT2_FT_DIR;

        case S_IFLNK:
            return EXT2_FT_SYMLINK;

        default:
            return EXT2_FT_UNKNOWN;
    }
}

static int add_links_locked(struct vnode* restrict node, int delta)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    int links = le16toh(ctx->inode.i_links_count) + delta;

    if(links < 0) {
        links = 0;
    }

    ctx->inode.i_links_count = htole16((uint16_t)(links));

    return write_inode_locked(node);
}

static void destroy_inode_locked(struct vnode* restrict node)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    struct ext2_vfs_context* fs_ctx = ctx->fs_ctx;
    struct ext2_inode* inode = &ctx->inode;
    uint32_t ino = (uint32_t)(node->nodenumber);

    if(node->nodetype != VNODE_SYM_LINK || !is_fast_symlink(fs_ctx, inode)) {
        if(ext2_truncate_blocks(node, 0) < 0) {
            pr_warning("ext2: %s: failed to free the blocks of inode %u", fs_ctx->device->identifier, ino);
        }
    }

    uint32_t xattr_block = le32toh(inode->i_file_acl);

    if(xattr_block) {
        ext2_xattr_release(fs_ctx, xattr_block);
        inode->i_file_acl = 0;
    }

    ext2_inode_set_size(fs_ctx, inode, 0);
    inode->i_dtime = htole32((uint32_t)(wallclock_now()));
    write_inode_locked(node);
    ext2_inode_free(fs_ctx, ino, node->nodetype == VNODE_DIRECTORY);
}

static int write_link_target_locked(struct vnode* restrict node, const char* restrict target)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    struct ext2_vfs_context* fs_ctx = ctx->fs_ctx;
    size_t length = strlen(target);
    int retval = 0;

    if(length < sizeof(ctx->inode.i_block)) {
        memcpy(ctx->inode.i_block, target, length);
    }
    else {
        ssize_t written = fcache_write(node, &ext2_bmap, target, length, 0, 0);

        if(written < 0) {
            retval = written;
        }
        else if((size_t)(written) != length) {
            retval = -EIO;
        }
    }

    if(retval == 0) {
        retval = ext2_inode_set_size(fs_ctx, &ctx->inode, length);
    }

    if(retval == 0) {
        retval = write_inode_locked(node);
    }

    return retval;
}

static bool link_target_fits(const struct vnode* restrict node, const char* restrict target)
{
    const struct ext2_vnode_context* ctx = node->ctxdata;

    return strlen(target) < ctx->fs_ctx->block_size;
}

static int link_locked(struct vnode* restrict node, const char* restrict name, struct vnode* restrict target)
{
    struct ext2_vnode_context* target_ctx = target->ctxdata;

    uint16_t links = le16toh(target_ctx->inode.i_links_count);

    if(strlen(name) > EXT2_NAME_LEN) {
        return -ENAMETOOLONG;
    }

    // Unlinked while still open
    if(links == 0) {
        return -ENOENT;
    }

    if(links >= EXT2_LINK_MAX) {
        return -EMLINK;
    }

    struct ext2_dirent existing;
    int retval = ext2_dirent_find(node, name, &existing);

    if(retval == 0) {
        return -EEXIST;
    }

    if(retval != -ENOENT) {
        return retval;
    }

    retval = ext2_dirent_add(node, name, (uint32_t)(target->nodenumber), file_type_of(le16toh(target_ctx->inode.i_mode)));

    if(retval < 0) {
        return retval;
    }

    touch_dir_locked(node);
    touch_locked(target, false);

    return add_links_locked(target, 1);
}

static int create_locked(struct vnode* restrict node, const char* restrict name, unsigned type, mode_t mode,
    const char* restrict link_target, const struct credentials* restrict cred, struct vnode** restrict out)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    struct ext2_vfs_context* fs_ctx = ctx->fs_ctx;
    bool directory = type == VNODE_DIRECTORY;

    *out = nullptr;

    if(node->nodetype != VNODE_DIRECTORY) {
        return -ENOTDIR;
    }

    if(strlen(name) > EXT2_NAME_LEN) {
        return -ENAMETOOLONG;
    }

    if(type != VNODE_FILE && type != VNODE_DIRECTORY && type != VNODE_SYM_LINK) {
        return -EINVAL;
    }

    if(directory && le16toh(ctx->inode.i_links_count) >= EXT2_LINK_MAX) {
        return -EMLINK;
    }

    struct ext2_dirent existing;
    int retval = ext2_dirent_find(node, name, &existing);

    if(retval == 0) {
        return -EEXIST;
    }

    if(retval != -ENOENT) {
        return retval;
    }

    uint16_t inode_mode = mode & ~S_IFMT;
    gid_t gid = cred->egid;

    if(directory) {
        inode_mode |= S_IFDIR;
    }
    else if(type == VNODE_SYM_LINK) {
        inode_mode |= S_IFLNK;
    }
    else {
        inode_mode |= S_IFREG;
    }

    // A set-group-ID directory hands down its
    // group, and the bit itself to subdirectories
    if(le16toh(ctx->inode.i_mode) & S_ISGID) {
        gid = ext2_inode_gid(fs_ctx, &ctx->inode);

        if(directory) {
            inode_mode |= S_ISGID;
        }
    }

    struct ext2_vnode_context* child_ctx = slab_alloc(sizeof(struct ext2_vnode_context));

    if(child_ctx == nullptr) {
        return -ENOMEM;
    }

    memset(child_ctx, 0, sizeof(struct ext2_vnode_context));

    child_ctx->fs_ctx = fs_ctx;
    child_ctx->inode.i_mode = htole16(inode_mode);
    child_ctx->inode.i_atime = htole32((uint32_t)(wallclock_now()));
    child_ctx->inode.i_ctime = child_ctx->inode.i_atime;
    child_ctx->inode.i_mtime = child_ctx->inode.i_atime;
    child_ctx->inode.i_links_count = htole16(1);

    if(directory) {
        child_ctx->inode.i_links_count = htole16(2);
    }

    if(!ext2_inode_set_owner(fs_ctx, &child_ctx->inode, cred->euid, gid)) {
        slab_free(child_ctx);
        return -EINVAL;
    }

    uint32_t ino = ext2_inode_alloc(fs_ctx, (uint32_t)(node->nodenumber), directory);

    if(ino == 0) {
        slab_free(child_ctx);
        return -ENOSPC;
    }

    struct vnode* child = vnode_alloc(type, &ext2_vnode_ops, child_ctx);

    if(child == nullptr) {
        ext2_inode_free(fs_ctx, ino, directory);
        slab_free(child_ctx);
        return -ENOMEM;
    }

    child->fs = node->fs;
    child->nodenumber = ino;

    *out = child;

    retval = write_inode_locked(child);

    if(retval == 0 && directory) {
        retval = ext2_dir_init(child, (uint32_t)(node->nodenumber));
    }

    if(retval == 0 && type == VNODE_SYM_LINK) {
        retval = write_link_target_locked(child, link_target);
    }

    // The new directory's ".." links to the parent
    if(retval == 0 && directory) {
        retval = add_links_locked(node, 1);
    }

    if(retval == 0) {
        retval = ext2_dirent_add(node, name, ino, file_type_of(inode_mode));

        if(retval < 0 && directory) {
            add_links_locked(node, -1);
        }
    }

    if(retval == 0) {
        touch_dir_locked(node);
    }

    if(retval < 0) {
        child_ctx->inode.i_links_count = 0;
    }

    return retval;
}

static int find_entry_locked(struct vnode* restrict dir, const char* restrict name, const struct vnode* restrict expected,
    struct ext2_dirent* restrict out)
{
    int retval = ext2_dirent_find(dir, name, out);

    if(retval < 0) {
        return retval;
    }

    if(out->ino != expected->nodenumber) {
        return -ENOENT;
    }

    return 0;
}

static int drop_entry_target_locked(struct vnode* restrict target)
{
    struct ext2_vnode_context* ctx = target->ctxdata;
    int retval;

    if(target->nodetype == VNODE_DIRECTORY) {
        ctx->inode.i_links_count = 0;
        retval = write_inode_locked(target);
    }
    else {
        retval = add_links_locked(target, -1);
    }

    // A file with names left stays cached
    // so they all keep leading to this vnode
    if(0 == ctx->inode.i_links_count) {
        vcache_remove(target);
    }

    return retval;
}

static int unlink_locked(struct vnode* restrict node, const char* restrict name, struct vnode* restrict target)
{
    struct ext2_dirent entry;
    int retval = find_entry_locked(node, name, target, &entry);

    if(retval == 0 && target->nodetype == VNODE_DIRECTORY) {
        retval = ext2_dir_check_empty(target);
    }

    if(retval == 0) {
        retval = ext2_dirent_remove(node, entry.offset);
    }

    if(retval < 0) {
        return retval;
    }

    touch_dir_locked(node);

    // The removed directory's ".." linked to the parent
    if(target->nodetype == VNODE_DIRECTORY) {
        add_links_locked(node, -1);
    }

    touch_locked(target, false);

    return drop_entry_target_locked(target);
}

static int rename_locked(struct vnode* olddir, const char* oldname, struct vnode* source, struct vnode* newdir, const char* newname,
    struct vnode* victim)
{
    if(strlen(newname) > EXT2_NAME_LEN) {
        return -ENAMETOOLONG;
    }

    struct ext2_dirent old_entry;
    int retval = find_entry_locked(olddir, oldname, source, &old_entry);

    if(retval < 0) {
        return retval;
    }

    struct ext2_dirent new_entry;
    retval = ext2_dirent_find(newdir, newname, &new_entry);

    if(retval < 0 && retval != -ENOENT) {
        return retval;
    }

    if(victim == nullptr && retval == 0) {
        return -EEXIST;
    }

    if(victim && (retval == -ENOENT || new_entry.ino != victim->nodenumber)) {
        return -ENOENT;
    }

    struct ext2_vnode_context* source_ctx = source->ctxdata;
    struct ext2_vnode_context* newdir_ctx = newdir->ctxdata;
    bool victim_directory = victim && victim->nodetype == VNODE_DIRECTORY;
    bool moves_directory = source->nodetype == VNODE_DIRECTORY && olddir != newdir;

    if(victim_directory) {
        retval = ext2_dir_check_empty(victim);

        if(retval < 0) {
            return retval;
        }
    }

    if(moves_directory && !victim_directory && le16toh(newdir_ctx->inode.i_links_count) >= EXT2_LINK_MAX) {
        return -EMLINK;
    }

    uint32_t ino = (uint32_t)(source->nodenumber);
    uint8_t file_type = file_type_of(le16toh(source_ctx->inode.i_mode));

    if(victim) {
        retval = ext2_dirent_retarget(newdir, new_entry.offset, ino, file_type);
    }
    else {
        retval = ext2_dirent_add(newdir, newname, ino, file_type);
    }

    // Adding a record only ever splits others
    // so the old one is still where it was found
    if(retval == 0) {
        retval = ext2_dirent_remove(olddir, old_entry.offset);
    }

    if(retval < 0) {
        return retval;
    }

    touch_dir_locked(olddir);

    if(newdir != olddir) {
        touch_dir_locked(newdir);
    }

    touch_locked(source, false);
    write_inode_locked(source);

    // Links to a directory come from the ".." of its subdirectories
    if(moves_directory) {
        struct ext2_dirent dotdot;
        retval = ext2_dirent_find(source, "..", &dotdot);

        if(retval == 0) {
            retval = ext2_dirent_retarget(source, dotdot.offset, (uint32_t)(newdir->nodenumber), EXT2_FT_DIR);
        }

        add_links_locked(olddir, -1);
        add_links_locked(newdir, 1);
    }

    if(victim_directory) {
        add_links_locked(newdir, -1);
    }

    if(victim) {
        touch_locked(victim, false);

        int victim_retval = drop_entry_target_locked(victim);

        if(retval == 0) {
            retval = victim_retval;
        }
    }

    return retval;
}

static void ext2_vnode_release(struct vnode* restrict node)
{
    struct ext2_vnode_context* ctx = node->ctxdata;

    if(0 == ctx->inode.i_links_count && !node->fs->readonly) {
        mutex_acquire(&ctx->fs_ctx->lock);
        destroy_inode_locked(node);
        mutex_release(&ctx->fs_ctx->lock);
    }

    slab_free(ctx);
}

static int ext2_vnode_lookup(struct vnode* restrict node, const char* restrict name, ino_t* restrict out)
{
    mutex_acquire(fs_lock(node));
    int retval = lookup_locked(node, name, out);
    mutex_release(fs_lock(node));
    return retval;
}

static int ext2_vnode_iterate(struct vnode* restrict node, struct k_dirent* restrict out, size_t index)
{
    mutex_acquire(fs_lock(node));
    int retval = iterate_locked(node, out, index);
    mutex_release(fs_lock(node));
    return retval;
}

static int ext2_vnode_create(struct vnode* restrict node, const char* restrict name, unsigned type, mode_t mode,
    const struct credentials* restrict cred, struct vnode** restrict out)
{
    struct vnode* created;

    mutex_acquire(fs_lock(node));
    int retval = create_locked(node, name, type, mode, nullptr, cred, &created);
    mutex_release(fs_lock(node));

    if(retval < 0 && created) {
        vnode_decref(created);
    }

    if(retval == 0) {
        *out = created;
    }

    return retval;
}

static int ext2_vnode_link(struct vnode* restrict node, const char* restrict name, struct vnode* restrict target)
{
    mutex_acquire(fs_lock(node));
    int retval = link_locked(node, name, target);
    mutex_release(fs_lock(node));
    return retval;
}

static int ext2_vnode_symlink(struct vnode* restrict node, const char* restrict name, const char* restrict target,
    const struct credentials* restrict cred, struct vnode** restrict out)
{
    struct vnode* created;

    if(!link_target_fits(node, target)) {
        return -ENAMETOOLONG;
    }

    mutex_acquire(fs_lock(node));
    int retval = create_locked(node, name, VNODE_SYM_LINK, S_IRWXU | S_IRWXG | S_IRWXO, target, cred, &created);
    mutex_release(fs_lock(node));

    if(retval < 0 && created) {
        vnode_decref(created);
    }

    if(retval == 0) {
        *out = created;
    }

    return retval;
}

static int ext2_vnode_unlink(struct vnode* restrict node, const char* restrict name, struct vnode* restrict target)
{
    mutex_acquire(fs_lock(node));
    int retval = unlink_locked(node, name, target);
    mutex_release(fs_lock(node));
    return retval;
}

static int ext2_vnode_rename(struct vnode* olddir, const char* oldname, struct vnode* source, struct vnode* newdir, const char* newname,
    struct vnode* victim)
{
    mutex_acquire(fs_lock(olddir));
    int retval = rename_locked(olddir, oldname, source, newdir, newname, victim);
    mutex_release(fs_lock(olddir));
    return retval;
}

static ssize_t ext2_vnode_read(struct vnode* restrict node, void* restrict buffer, size_t sz, off_t offset)
{
    struct ext2_vnode_context* ctx = node->ctxdata;

    if(node->nodetype == VNODE_DIRECTORY) {
        return -EISDIR;
    }

    if(node->nodetype != VNODE_FILE) {
        return -EINVAL;
    }

    mutex_acquire(fs_lock(node));
    ssize_t retval = fcache_read(node, &ext2_bmap, buffer, sz, offset, ext2_inode_size(&ctx->inode));
    mutex_release(fs_lock(node));

    return retval;
}

static ssize_t ext2_vnode_write(struct vnode* restrict node, const void* restrict buffer, size_t sz, off_t offset)
{
    if(node->nodetype == VNODE_DIRECTORY) {
        return -EISDIR;
    }

    if(node->nodetype != VNODE_FILE) {
        return -EINVAL;
    }

    mutex_acquire(fs_lock(node));
    ssize_t retval = write_locked(node, buffer, sz, offset);
    mutex_release(fs_lock(node));

    return retval;
}

static ssize_t ext2_vnode_readlink(struct vnode* restrict node, char* restrict buf, size_t sz)
{
    mutex_acquire(fs_lock(node));
    ssize_t retval = readlink_locked(node, buf, sz);
    mutex_release(fs_lock(node));
    return retval;
}

static int ext2_vnode_stat(struct vnode* restrict node, struct k_stat* restrict out)
{
    struct ext2_vnode_context* ctx = node->ctxdata;
    struct ext2_vfs_context* fs_ctx = ctx->fs_ctx;
    const struct ext2_inode* inode = &ctx->inode;

    memset(out, 0, sizeof(struct k_stat));

    mutex_acquire(&fs_ctx->lock);

    out->st_ino = node->nodenumber;
    out->st_mode = le16toh(inode->i_mode);
    out->st_nlink = le16toh(inode->i_links_count);
    out->st_uid = ext2_inode_uid(fs_ctx, inode);
    out->st_gid = ext2_inode_gid(fs_ctx, inode);
    out->st_size = (off_t)ext2_inode_size(inode);
    out->st_atime = le32toh(inode->i_atime);
    out->st_mtime = le32toh(inode->i_mtime);
    out->st_ctime = le32toh(inode->i_ctime);
    out->st_blksize = fs_ctx->block_size;
    out->st_blocks = le32toh(inode->i_blocks);

    mutex_release(&fs_ctx->lock);

    return 0;
}

static int ext2_vnode_setattr(struct vnode* restrict node, const struct vattr* restrict attr)
{
    mutex_acquire(fs_lock(node));
    int retval = setattr_locked(node, attr);
    mutex_release(fs_lock(node));
    return retval;
}

static int ext2_vnode_fsync(struct vnode* restrict node)
{
    struct ext2_vnode_context* ctx = node->ctxdata;

    mutex_acquire(fs_lock(node));
    int retval = fcache_writeback(node, &ext2_bmap, ext2_inode_size(&ctx->inode));
    mutex_release(fs_lock(node));

    return retval;
}

static int ext2_vnode_map_page(struct vnode* restrict node, uint64_t index, struct pagemap* restrict pmap, uintptr_t virt, unsigned vprot)
{
    struct ext2_vnode_context* ctx = node->ctxdata;

    if(node->nodetype != VNODE_FILE) {
        return -ENODEV;
    }

    mutex_acquire(fs_lock(node));
    int retval = fcache_map_page(node, &ext2_bmap, index, ext2_inode_size(&ctx->inode), pmap, virt, vprot);
    mutex_release(fs_lock(node));

    return retval;
}

static ssize_t ext2_vnode_xattr_get(struct vnode* restrict node, unsigned space, const char* restrict name, void* restrict value,
    size_t size)
{
    mutex_acquire(fs_lock(node));
    ssize_t retval = ext2_xattr_get(node, space, name, value, size);
    mutex_release(fs_lock(node));
    return retval;
}

static int ext2_vnode_xattr_set(struct vnode* restrict node, unsigned space, const char* restrict name, const void* restrict value,
    size_t size, unsigned flags)
{
    mutex_acquire(fs_lock(node));

    int retval = ext2_xattr_set(node, space, name, value, size, flags);

    if(retval == 0) {
        touch_locked(node, false);
        retval = write_inode_locked(node);
    }

    mutex_release(fs_lock(node));

    return retval;
}

static int ext2_vnode_xattr_iterate(struct vnode* restrict node, struct xattr_entry* restrict out, size_t index)
{
    mutex_acquire(fs_lock(node));
    int retval = ext2_xattr_iterate(node, out, index);
    mutex_release(fs_lock(node));
    return retval;
}

static int ext2_vnode_xattr_remove(struct vnode* restrict node, unsigned space, const char* restrict name)
{
    mutex_acquire(fs_lock(node));

    int retval = ext2_xattr_remove(node, space, name);

    if(retval == 0) {
        touch_locked(node, false);
        retval = write_inode_locked(node);
    }

    mutex_release(fs_lock(node));

    return retval;
}

const struct vnode_ops ext2_vnode_ops = {
    .release = &ext2_vnode_release,
    .lookup = &ext2_vnode_lookup,
    .iterate = &ext2_vnode_iterate,
    .create = &ext2_vnode_create,
    .unlink = &ext2_vnode_unlink,
    .rename = &ext2_vnode_rename,
    .link = &ext2_vnode_link,
    .symlink = &ext2_vnode_symlink,
    .read = &ext2_vnode_read,
    .write = &ext2_vnode_write,
    .readlink = &ext2_vnode_readlink,
    .stat = &ext2_vnode_stat,
    .setattr = &ext2_vnode_setattr,
    .fsync = &ext2_vnode_fsync,
    .map_page = &ext2_vnode_map_page,
    .xattr_get = &ext2_vnode_xattr_get,
    .xattr_set = &ext2_vnode_xattr_set,
    .xattr_iterate = &ext2_vnode_xattr_iterate,
    .xattr_remove = &ext2_vnode_xattr_remove,
};
