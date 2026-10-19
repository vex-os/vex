#include <vex/assert.h>
#include <vex/atomic.h>
#include <vex/fcntl.h>
#include <vex/limits.h>
#include <vex/mount.h>
#include <vex/mutex.h>
#include <vex/slab.h>
#include <vex/spinlock.h>
#include <vex/string.h>
#include <vex/vcache.h>
#include <vex/vnode.h>
#include <vex/wallclock.h>
#include <vex/xattr.h>

struct privs_state {
    unsigned seq;
    unsigned caps_state;
    bool clear;
};

static const char* const xattr_prefixes[NUM_XATTR_SPACES] = {
    [XATTR_USER] = "user.",
    [XATTR_TRUSTED] = "trusted.",
    [XATTR_SECURITY] = "security.",
};

static_assert(sizeof(struct vnode) <= SLAB_MAX_SIZE);

static struct mutex rename_mutex = MUTEX_INIT;

static struct vnode* alloc_internal(struct vfs* restrict fs, ino_t nodenumber, const struct vnode_ops* restrict ops)
{
    kassert(fs);
    kassert(ops);

    struct vnode* result = slab_alloc(sizeof(struct vnode));

    if(result == nullptr) {
        return nullptr;
    }

    result->fs = fs;
    result->ops = ops;
    result->nodenumber = nodenumber;

    fcache_init(&result->pages);

    atomic_store(&result->refcount, 1, ATOMIC_RELAXED);

    return result;
}

static int lookup_crossed(struct vnode* restrict node, const char* restrict name, struct vnode** restrict out)
{
    if(node->ops->lookup == nullptr) {
        return -ENOSYS;
    }

    ino_t nodenumber;
    int retval = node->ops->lookup(node, name, &nodenumber);

    if(retval < 0) {
        return retval;
    }

    struct vnode* cached = vcache_get(node->fs, nodenumber);

    if(cached) {
        *out = cached;
        return 0;
    }

    struct vnode* result = alloc_internal(node->fs, nodenumber, node->ops);

    if(result == nullptr) {
        return -ENOMEM;
    }

    if(node->fs->ops->fillnode) {
        retval = node->fs->ops->fillnode(node->fs, result);

        if(retval < 0) {
            slab_free(result);
            return retval;
        }
    }

    if(result->nodetype == VNODE_DIRECTORY) {
        result->fs_mounted = vfs_get_fs(result);
        result->parent = node;
        vnode_incref(node);
    }

    struct vnode* cached_now = vcache_get_or_insert(result);

    if(cached_now != result) {
        vnode_destroy(result);

        if(cached_now == nullptr) {
            return -ENOMEM;
        }
    }

    *out = cached_now;

    return 0;
}

static int cache_created(struct vnode* restrict node, struct vnode* restrict created, struct vnode** restrict out)
{
    if(created->nodetype == VNODE_DIRECTORY) {
        created->parent = node;
        vnode_incref(node);
    }

    struct vnode* cached_now = vcache_get_or_insert(created);

    if(cached_now != created) {
        vnode_destroy(created);

        if(cached_now == nullptr) {
            return -ENOMEM;
        }
    }

    *out = cached_now;

    return 0;
}

static int check_new_name(struct vnode* restrict node, const char* restrict name)
{
    if(node->fs->readonly) {
        return -EROFS;
    }

    if(0 == strcmp(name, ".") || 0 == strcmp(name, "..")) {
        return -EEXIST;
    }

    return 0;
}

static int link_crossed(struct vnode* restrict node, const char* restrict name, struct vnode* restrict target)
{
    if(target->fs != node->fs) {
        return -EXDEV;
    }

    if(target->nodetype == VNODE_DIRECTORY || node->ops->link == nullptr) {
        return -EPERM;
    }

    int retval = check_new_name(node, name);

    if(retval < 0) {
        return retval;
    }

    return node->ops->link(node, name, target);
}

static int symlink_crossed(struct vnode* restrict node, const char* restrict name, const char* restrict target,
    const struct credentials* restrict cred, struct vnode** restrict out)
{
    if(node->ops->symlink == nullptr) {
        return -EPERM;
    }

    int retval = check_new_name(node, name);

    if(retval < 0) {
        return retval;
    }

    struct vnode* created;
    retval = node->ops->symlink(node, name, target, cred, &created);

    if(retval < 0) {
        return retval;
    }

    return cache_created(node, created, out);
}

static int create_crossed(struct vnode* restrict node, const char* restrict name, unsigned type, mode_t mode,
    const struct credentials* restrict cred, struct vnode** restrict out)
{
    if(node->ops->create == nullptr) {
        return -ENOSYS;
    }

    if(node->fs->readonly) {
        return -EROFS;
    }

    struct vnode* created;
    int retval = node->ops->create(node, name, type, mode, cred, &created);

    if(retval < 0) {
        return retval;
    }

    return cache_created(node, created, out);
}

static int unlink_crossed(struct vnode* restrict node, const char* restrict name, bool directory)
{
    if(node->ops->unlink == nullptr) {
        return -ENOSYS;
    }

    if(node->fs->readonly) {
        return -EROFS;
    }

    if(0 == strcmp(name, ".") || 0 == strcmp(name, "..")) {
        return -EINVAL;
    }

    struct vnode* victim;
    int retval = lookup_crossed(node, name, &victim);

    if(retval < 0) {
        return retval;
    }

    if(directory && victim->nodetype != VNODE_DIRECTORY) {
        vnode_decref(victim);
        return -ENOTDIR;
    }

    if(!directory && victim->nodetype == VNODE_DIRECTORY) {
        vnode_decref(victim);
        return -EISDIR;
    }

    if(victim->fs_mounted) {
        vnode_decref(victim);
        return -EBUSY;
    }

    // Once the victim's last name is gone, the filesystem has to vcache_remove it under its own lock,
    // before the entry can be reused, or a new file there gets this vnode; with names left it stays cached
    retval = node->ops->unlink(node, name, victim);

    // Whichever reference goes last, possibly
    // the one dropped right below, uncounts it
    if(retval == 0 && !vcache_cached(victim)) {
        victim->orphaned = true;
        atomic_fetch_add(&victim->fs->orphans, 1, ATOMIC_RELAXED);
    }

    vnode_decref(victim);

    return retval;
}

static bool is_ancestor(const struct vnode* restrict ancestor, const struct vnode* restrict node)
{
    for(const struct vnode* it = node; it; it = it->parent) {
        if(it == ancestor) {
            return true;
        }
    }

    return false;
}

static void set_parent(struct vnode* restrict node, struct vnode* restrict parent)
{
    vnode_incref(parent);

    spinlock_acquire(&node->lock);
    struct vnode* previous = node->parent;
    node->parent = parent;
    spinlock_release(&node->lock);

    if(previous) {
        vnode_decref(previous);
    }
}

static int check_rename(struct vnode* restrict source, struct vnode* restrict newdir, struct vnode* restrict victim)
{
    if(source->fs_mounted) {
        return -EBUSY;
    }

    if(source->nodetype == VNODE_DIRECTORY && is_ancestor(source, newdir)) {
        return -EINVAL;
    }

    if(victim == nullptr || victim == source) {
        return 0;
    }

    if(victim->fs_mounted) {
        return -EBUSY;
    }

    if(source->nodetype == VNODE_DIRECTORY && victim->nodetype != VNODE_DIRECTORY) {
        return -ENOTDIR;
    }

    if(source->nodetype != VNODE_DIRECTORY && victim->nodetype == VNODE_DIRECTORY) {
        return -EISDIR;
    }

    return 0;
}

static int rename_crossed(struct vnode* olddir, const char* oldname, struct vnode* newdir, const char* newname)
{
    if(olddir->fs != newdir->fs) {
        return -EXDEV;
    }

    if(olddir->ops->rename == nullptr) {
        return -ENOSYS;
    }

    if(olddir->fs->readonly) {
        return -EROFS;
    }

    if(0 == strcmp(oldname, ".") || 0 == strcmp(oldname, "..") || 0 == strcmp(newname, ".") || 0 == strcmp(newname, "..")) {
        return -EINVAL;
    }

    if(oldname[0] == 0 || newname[0] == 0) {
        return -ENOENT;
    }

    mutex_acquire(&rename_mutex);

    struct vnode* source;
    int retval = lookup_crossed(olddir, oldname, &source);

    if(retval < 0) {
        mutex_release(&rename_mutex);
        return retval;
    }

    struct vnode* victim = nullptr;
    retval = lookup_crossed(newdir, newname, &victim);

    if(retval == -ENOENT) {
        victim = nullptr;
        retval = 0;
    }

    if(retval == 0) {
        retval = check_rename(source, newdir, victim);
    }

    // Both names already lead to the same file
    bool same = victim == source;

    // The filesystem has to vcache_remove a victim left without names and vcache_rekey
    // the source under its own lock, before a lookup can see the new entry
    if(retval == 0 && !same) {
        retval = olddir->ops->rename(olddir, oldname, source, newdir, newname, victim);
    }

    if(retval == 0 && !same && victim && !vcache_cached(victim)) {
        victim->orphaned = true;
        atomic_fetch_add(&victim->fs->orphans, 1, ATOMIC_RELAXED);
    }

    if(retval == 0 && !same && source->nodetype == VNODE_DIRECTORY && olddir != newdir) {
        set_parent(source, newdir);
    }

    mutex_release(&rename_mutex);

    if(victim) {
        vnode_decref(victim);
    }

    vnode_decref(source);

    return retval;
}

static bool time_valid(const struct k_timespec* restrict time)
{
    if(time->tv_nsec == UTIME_NOW || time->tv_nsec == UTIME_OMIT) {
        return true;
    }

    return time->tv_nsec >= 0 && time->tv_nsec < 1000000000;
}

static bool time_set(const struct k_timespec* restrict time, time_t* restrict out)
{
    if(time->tv_nsec == UTIME_OMIT) {
        return false;
    }

    if(time->tv_nsec == UTIME_NOW) {
        *out = wallclock_now();
    }
    else {
        *out = time->tv_sec;
    }

    return true;
}

static int split_xattr_name(const char* restrict full, unsigned* restrict out_space, const char** restrict out_name)
{
    if(strlen(full) > XATTR_NAME_MAX) {
        return -ERANGE;
    }

    for(unsigned space = 0; space < NUM_XATTR_SPACES; space += 1) {
        size_t length = strlen(xattr_prefixes[space]);

        if(0 != strncmp(full, xattr_prefixes[space], length)) {
            continue;
        }

        if(full[length] == 0) {
            return -EINVAL;
        }

        *out_space = space;
        *out_name = full + length;

        return 0;
    }

    return -EOPNOTSUPP;
}

// Setgid without group execute marks mandatory locking rather than granting anything
static mode_t without_privs(mode_t mode)
{
    mode_t result = mode & ~(S_IFMT | S_ISUID);

    if(mode & S_IXGRP) {
        result &= ~S_ISGID;
    }

    return result;
}

static bool is_caps_name(unsigned space, const char* restrict name)
{
    return space == XATTR_SECURITY && 0 == strcmp(name, XATTR_CAPS_NAME);
}

static void privs_snapshot(struct vnode* restrict node, struct privs_state* restrict out)
{
    spinlock_acquire(&node->lock);
    out->seq = node->privs_seq;
    out->caps_state = node->caps_state;
    out->clear = node->privs_clear;
    spinlock_release(&node->lock);
}

// Comes after the change itself, so that whoever looked
// before it can't record what it saw over the change
static void privs_changed(struct vnode* restrict node)
{
    spinlock_acquire(&node->lock);
    node->privs_seq += 1;
    node->caps_state = VNODE_CAPS_UNKNOWN;
    node->privs_clear = false;
    spinlock_release(&node->lock);
}

static void privs_cleared(struct vnode* restrict node, unsigned seq)
{
    spinlock_acquire(&node->lock);

    if(node->privs_seq == seq) {
        node->caps_state = VNODE_CAPS_NONE;
        node->privs_clear = true;
    }

    spinlock_release(&node->lock);
}

static void caps_learned(struct vnode* restrict node, unsigned seq, unsigned caps_state)
{
    spinlock_acquire(&node->lock);

    if(node->privs_seq == seq) {
        node->caps_state = caps_state;
    }

    spinlock_release(&node->lock);
}

static int require_cap(const struct credentials* restrict cred, unsigned cap)
{
    if(cred_capable(cred, cap)) {
        return 0;
    }

    return -EPERM;
}

static int xattr_permission(struct vnode* restrict node, const struct credentials* restrict cred, unsigned space, const char* restrict name,
    bool write)
{
    if(space == XATTR_TRUSTED) {
        return require_cap(cred, CAP_SYS_ADMIN);
    }

    if(space == XATTR_SECURITY && !write) {
        return 0;
    }

    if(is_caps_name(space, name)) {
        return require_cap(cred, CAP_SETFCAP);
    }

    if(space == XATTR_SECURITY) {
        return require_cap(cred, CAP_SYS_ADMIN);
    }

    // Others could fill up a world-writable device node or symlink
    if(node->nodetype != VNODE_FILE && node->nodetype != VNODE_DIRECTORY) {
        return write ? -EPERM : -ENODATA;
    }

    return vnode_access(node, cred, write ? W_OK : R_OK);
}

static void strip_file_caps(struct vnode* restrict node)
{
    struct vnode* target;

    if(vfs_cross_mount(node, &target) < 0) {
        return;
    }

    struct privs_state state;
    privs_snapshot(target, &state);

    if(target->nodetype == VNODE_FILE && target->ops->xattr_remove && !target->fs->readonly && state.caps_state != VNODE_CAPS_NONE) {
        target->ops->xattr_remove(target, XATTR_SECURITY, XATTR_CAPS_NAME);
        privs_changed(target);
    }

    vnode_decref(target);
}

struct vnode* vnode_alloc(unsigned nodetype, const struct vnode_ops* restrict ops, void* ctxdata)
{
    kassert(ops);

    struct vnode* result = slab_alloc(sizeof(struct vnode));

    if(result == nullptr) {
        return nullptr;
    }

    result->ops = ops;
    result->nodetype = nodetype;
    result->ctxdata = ctxdata;

    fcache_init(&result->pages);

    atomic_store(&result->refcount, 1, ATOMIC_RELAXED);

    return result;
}

void vnode_incref(struct vnode* restrict node)
{
    kassert(node);

    atomic_fetch_add(&node->refcount, 1, ATOMIC_RELAXED);
}

bool vnode_decref(struct vnode* restrict node)
{
    kassert(node);

    unsigned prev = atomic_fetch_sub(&node->refcount, 1, ATOMIC_ACQ_REL);

    if(prev != 1 || vcache_keep(node)) {
        return false;
    }

    vnode_destroy(node);

    return true;
}

void vnode_destroy(struct vnode* restrict node)
{
    kassert(node->cache_entry == nullptr);

    struct vnode* parent = node->parent;

    if(node->orphaned) {
        atomic_fetch_sub(&node->fs->orphans, 1, ATOMIC_RELEASE);
    }

    fcache_deinit(&node->pages);

    if(node->ops->release)
        node->ops->release(node);
    slab_free(node);

    if(parent) {
        vnode_decref(parent);
    }
}

int vnode_lookup(struct vnode* restrict node, const char* restrict name, struct vnode** restrict out)
{
    kassert(node);

    struct vnode* target;
    int retval = vfs_cross_mount(node, &target);

    if(retval < 0) {
        return retval;
    }

    retval = lookup_crossed(target, name, out);

    vnode_decref(target);

    return retval;
}

int vnode_iterate(struct vnode* restrict node, struct k_dirent* restrict out, size_t index)
{
    kassert(node);

    struct vnode* target;
    int retval = vfs_cross_mount(node, &target);

    if(retval < 0) {
        return retval;
    }

    if(target->ops->iterate == nullptr) {
        retval = -ENOSYS;
    }
    else {
        retval = target->ops->iterate(target, out, index);
    }

    vnode_decref(target);

    return retval;
}

int vnode_create(struct vnode* restrict node, const char* restrict name, unsigned type, mode_t mode,
    const struct credentials* restrict cred, struct vnode** restrict out)
{
    kassert(node);
    kassert(cred);

    struct vnode* target;
    int retval = vfs_cross_mount(node, &target);

    if(retval < 0) {
        return retval;
    }

    retval = create_crossed(target, name, type, mode & ~S_IFMT, cred, out);

    vnode_decref(target);

    return retval;
}

int vnode_unlink(struct vnode* restrict node, const char* restrict name, bool directory)
{
    kassert(node);

    struct vnode* target;
    int retval = vfs_cross_mount(node, &target);

    if(retval < 0) {
        return retval;
    }

    retval = unlink_crossed(target, name, directory);

    vnode_decref(target);

    return retval;
}

int vnode_rename(struct vnode* olddir, const char* oldname, struct vnode* newdir, const char* newname)
{
    kassert(olddir);
    kassert(newdir);

    struct vnode* source_dir;
    int retval = vfs_cross_mount(olddir, &source_dir);

    if(retval < 0) {
        return retval;
    }

    struct vnode* target_dir;
    retval = vfs_cross_mount(newdir, &target_dir);

    if(retval < 0) {
        vnode_decref(source_dir);
        return retval;
    }

    retval = rename_crossed(source_dir, oldname, target_dir, newname);

    vnode_decref(target_dir);
    vnode_decref(source_dir);

    return retval;
}

int vnode_link(struct vnode* restrict node, const char* restrict name, struct vnode* restrict target)
{
    kassert(node);
    kassert(target);

    struct vnode* crossed;
    int retval = vfs_cross_mount(node, &crossed);

    if(retval < 0) {
        return retval;
    }

    retval = link_crossed(crossed, name, target);

    vnode_decref(crossed);

    return retval;
}

int vnode_symlink(struct vnode* restrict node, const char* restrict name, const char* restrict target,
    const struct credentials* restrict cred, struct vnode** restrict out)
{
    kassert(node);
    kassert(cred);

    if(target[0] == 0) {
        return -ENOENT;
    }

    struct vnode* crossed;
    int retval = vfs_cross_mount(node, &crossed);

    if(retval < 0) {
        return retval;
    }

    retval = symlink_crossed(crossed, name, target, cred, out);

    vnode_decref(crossed);

    return retval;
}

int vnode_open(struct vnode* restrict node, unsigned mode)
{
    kassert(node);

    struct vnode* target;
    int retval = vfs_cross_mount(node, &target);

    if(retval < 0) {
        return retval;
    }

    if(target->ops->open == nullptr) {
        retval = -ENOSYS;
    }
    else {
        retval = target->ops->open(target, mode);
    }

    vnode_decref(target);

    return retval;
}

int vnode_close(struct vnode* restrict node)
{
    kassert(node);

    struct vnode* target;
    int retval = vfs_cross_mount(node, &target);

    if(retval < 0) {
        return retval;
    }

    if(target->ops->close == nullptr) {
        retval = -ENOSYS;
    }
    else {
        retval = target->ops->close(target);
    }

    vnode_decref(target);

    return retval;
}

ssize_t vnode_read(struct vnode* restrict node, void* restrict buffer, size_t sz, off_t offset)
{
    kassert(node);

    struct vnode* target;
    ssize_t retval = vfs_cross_mount(node, &target);

    if(retval < 0) {
        return retval;
    }

    if(target->ops->read == nullptr) {
        retval = -ENOSYS;
    }
    else {
        retval = target->ops->read(target, buffer, sz, offset);
    }

    vnode_decref(target);

    return retval;
}

ssize_t vnode_write(struct vnode* restrict node, const void* restrict buffer, size_t sz, off_t offset)
{
    kassert(node);

    struct vnode* target;
    ssize_t retval = vfs_cross_mount(node, &target);

    if(retval < 0) {
        return retval;
    }

    if(target->ops->write == nullptr) {
        retval = -ENOSYS;
    }
    else {
        retval = target->ops->write(target, buffer, sz, offset);
    }

    vnode_decref(target);

    return retval;
}

int vnode_stat(struct vnode* restrict node, struct k_stat* restrict out)
{
    kassert(node);

    struct vnode* target;
    int retval = vfs_cross_mount(node, &target);

    if(retval < 0) {
        return retval;
    }

    if(target->ops->stat == nullptr) {
        retval = -ENOSYS;
    }
    else {
        retval = target->ops->stat(target, out);
    }

    vnode_decref(target);

    return retval;
}

int vnode_setattr(struct vnode* restrict node, const struct vattr* restrict attr)
{
    kassert(node);
    kassert(attr);

    if((attr->mask & VATTR_SIZE) && (node->nodetype != VNODE_FILE || attr->size < 0)) {
        return -EINVAL;
    }

    struct vnode* target;
    int retval = vfs_cross_mount(node, &target);

    if(retval < 0) {
        return retval;
    }

    if(target->fs->readonly) {
        retval = -EROFS;
    }
    else if(target->ops->setattr == nullptr) {
        retval = -EPERM;
    }
    else {
        retval = target->ops->setattr(target, attr);
    }

    if(retval == 0 && (attr->mask & (VATTR_MODE | VATTR_UID | VATTR_GID))) {
        privs_changed(target);
    }

    vnode_decref(target);

    return retval;
}

int vnode_chmod(struct vnode* restrict node, const struct credentials* restrict cred, mode_t mode)
{
    kassert(node);
    kassert(cred);

    struct k_stat st;
    int retval = vnode_stat(node, &st);

    if(retval < 0) {
        return retval;
    }

    if(cred->euid != st.st_uid && !cred_capable(cred, CAP_FOWNER)) {
        return -EPERM;
    }

    struct vattr attr = { .mask = VATTR_MODE, .mode = mode & (S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO) };

    // Otherwise anyone could hand out a group they are not in
    if(!cred_in_group(cred, st.st_gid) && !cred_capable(cred, CAP_FSETID)) {
        attr.mode &= ~S_ISGID;
    }

    return vnode_setattr(node, &attr);
}

int vnode_chown(struct vnode* restrict node, const struct credentials* restrict cred, uid_t uid, gid_t gid)
{
    kassert(node);
    kassert(cred);

    if(uid < -1 || gid < -1) {
        return -EINVAL;
    }

    struct k_stat st;
    int retval = vnode_stat(node, &st);

    if(retval < 0) {
        return retval;
    }

    struct vattr attr = { 0 };

    if(uid != -1 && uid != st.st_uid) {
        attr.mask |= VATTR_UID;
        attr.uid = uid;
    }

    if(gid != -1 && gid != st.st_gid) {
        attr.mask |= VATTR_GID;
        attr.gid = gid;
    }

    if(!cred_capable(cred, CAP_CHOWN)) {
        // The owner may only move the file
        // into another group of its own
        if(attr.mask & VATTR_UID) {
            return -EPERM;
        }

        if((attr.mask & VATTR_GID) && (cred->euid != st.st_uid || !cred_in_group(cred, gid))) {
            return -EPERM;
        }
    }

    if(attr.mask == 0) {
        return 0;
    }

    // A program must not keep running as an owner it no longer has
    if((st.st_mode & S_IFMT) != S_IFDIR && without_privs(st.st_mode) != (st.st_mode & ~S_IFMT)) {
        attr.mask |= VATTR_MODE;
        attr.mode = without_privs(st.st_mode);
    }

    retval = vnode_setattr(node, &attr);

    if(retval == 0) {
        strip_file_caps(node);
    }

    return retval;
}

int vnode_access(struct vnode* restrict node, const struct credentials* restrict cred, unsigned mask)
{
    kassert(node);
    kassert(cred);

    struct k_stat st;
    int retval = vnode_stat(node, &st);

    if(retval < 0) {
        return retval;
    }

    if(cred_capable(cred, CAP_DAC_OVERRIDE)) {
        if((mask & X_OK) && (st.st_mode & S_IFMT) != S_IFDIR && 0 == (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
            return -EACCES;
        }

        return 0;
    }

    mode_t granted;

    if(cred->euid == st.st_uid) {
        granted = (st.st_mode >> 6) & 7;
    }
    else if(cred_in_group(cred, st.st_gid)) {
        granted = (st.st_mode >> 3) & 7;
    }
    else {
        granted = st.st_mode & 7;
    }

    if((granted & mask) != mask) {
        return -EACCES;
    }

    return 0;
}

int vnode_remove_privs(struct vnode* restrict node, const struct credentials* restrict cred)
{
    kassert(node);
    kassert(cred);

    if(node->nodetype != VNODE_FILE) {
        return 0;
    }

    struct privs_state state;
    privs_snapshot(node, &state);

    if(state.clear) {
        return 0;
    }

    struct k_stat st;
    int retval = vnode_stat(node, &st);

    if(retval < 0) {
        return retval;
    }

    mode_t stripped = without_privs(st.st_mode);
    bool mode_clear = stripped == (st.st_mode & ~S_IFMT);

    if(!mode_clear && !cred_capable(cred, CAP_FSETID)) {
        struct vattr attr = { .mask = VATTR_MODE, .mode = stripped };
        retval = vnode_setattr(node, &attr);
    }

    bool caps_clear = state.caps_state == VNODE_CAPS_NONE || node->ops->xattr_remove == nullptr;

    if(retval == 0 && !caps_clear) {
        retval = node->ops->xattr_remove(node, XATTR_SECURITY, XATTR_CAPS_NAME);

        if(retval == 0) {
            privs_changed(node);
        }

        caps_clear = retval == -ENODATA;
    }

    if(retval == -ENODATA) {
        retval = 0;
    }

    // Whatever was taken away above moved the sequence on,
    // so only the next write marks the file as clean
    if(retval == 0 && mode_clear && caps_clear) {
        privs_cleared(node, state.seq);
    }

    return retval;
}

int vnode_utimes(struct vnode* restrict node, const struct credentials* restrict cred, const struct k_timespec* restrict times)
{
    kassert(node);
    kassert(cred);

    static const struct k_timespec both_now[2] = {
        { .tv_nsec = UTIME_NOW },
        { .tv_nsec = UTIME_NOW },
    };

    if(times == nullptr) {
        times = both_now;
    }

    if(!time_valid(&times[0]) || !time_valid(&times[1])) {
        return -EINVAL;
    }

    struct vattr attr = { 0 };

    if(time_set(&times[0], &attr.atime)) {
        attr.mask |= VATTR_ATIME;
    }

    if(time_set(&times[1], &attr.mtime)) {
        attr.mask |= VATTR_MTIME;
    }

    if(attr.mask == 0) {
        return 0;
    }

    struct k_stat st;
    int retval = vnode_stat(node, &st);

    if(retval < 0) {
        return retval;
    }

    // Anyone who may write the file may touch it, but
    // only its owner may set the times to something else
    if(cred->euid != st.st_uid && !cred_capable(cred, CAP_FOWNER)) {
        bool now_only = times[0].tv_nsec != UTIME_OMIT || times[1].tv_nsec != UTIME_OMIT;
        now_only = now_only && (times[0].tv_nsec == UTIME_NOW || times[0].tv_nsec == UTIME_OMIT);
        now_only = now_only && (times[1].tv_nsec == UTIME_NOW || times[1].tv_nsec == UTIME_OMIT);

        if(!now_only) {
            return -EPERM;
        }

        retval = vnode_access(node, cred, W_OK);

        if(retval < 0) {
            return retval;
        }
    }

    return vnode_setattr(node, &attr);
}

int vnode_may_delete(struct vnode* restrict dir, const char* restrict name, const struct credentials* restrict cred)
{
    kassert(dir);
    kassert(name);
    kassert(cred);

    int retval = vnode_access(dir, cred, W_OK | X_OK);

    if(retval < 0) {
        return retval;
    }

    struct k_stat dir_st;
    retval = vnode_stat(dir, &dir_st);

    if(retval < 0) {
        return retval;
    }

    // In a sticky directory such as /tmp, write access
    // alone lets nobody remove what others put there
    if(0 == (dir_st.st_mode & S_ISVTX) || cred->euid == dir_st.st_uid || cred_capable(cred, CAP_FOWNER)) {
        return 0;
    }

    struct vnode* victim;

    // Nothing there to protect; the operation itself reports a missing name
    if(vnode_lookup(dir, name, &victim) < 0) {
        return 0;
    }

    struct k_stat victim_st;
    retval = vnode_stat(victim, &victim_st);

    vnode_decref(victim);

    if(retval < 0) {
        return retval;
    }

    if(cred->euid != victim_st.st_uid) {
        return -EPERM;
    }

    return 0;
}

ssize_t vnode_readlink(struct vnode* restrict node, char* restrict buf, size_t sz)
{
    kassert(node);

    if(node->ops->readlink == nullptr)
        return -ENOSYS;
    return node->ops->readlink(node, buf, sz);
}

int vnode_ioctl(struct vnode* restrict node, uint32_t request, uintptr_t argument)
{
    kassert(node);

    struct vnode* target;
    int retval = vfs_cross_mount(node, &target);

    if(retval < 0) {
        return retval;
    }

    if(target->ops->ioctl == nullptr) {
        retval = -ENOSYS;
    }
    else {
        retval = target->ops->ioctl(target, request, argument);
    }

    vnode_decref(target);

    return retval;
}

int vnode_fsync(struct vnode* restrict node)
{
    kassert(node);

    if(node->ops->fsync == nullptr) {
        return 0;
    }

    return node->ops->fsync(node);
}

int vnode_map_page(struct vnode* restrict node, uint64_t index, struct pagemap* restrict pmap, uintptr_t virt, unsigned vprot)
{
    kassert(node);

    if(node->ops->map_page == nullptr) {
        return -ENODEV;
    }

    return node->ops->map_page(node, index, pmap, virt, vprot);
}

ssize_t vnode_xattr_get(struct vnode* restrict node, const struct credentials* restrict cred, const char* restrict name,
    void* restrict value, size_t size)
{
    kassert(node);
    kassert(cred);

    unsigned space;
    const char* suffix;
    ssize_t retval = split_xattr_name(name, &space, &suffix);

    if(retval < 0) {
        return retval;
    }

    struct vnode* target;
    retval = vfs_cross_mount(node, &target);

    if(retval < 0) {
        return retval;
    }

    retval = xattr_permission(target, cred, space, suffix, false);

    if(retval == 0 && target->ops->xattr_get == nullptr) {
        retval = -EOPNOTSUPP;
    }

    if(retval == 0) {
        retval = target->ops->xattr_get(target, space, suffix, value, size);
    }

    vnode_decref(target);

    return retval;
}

int vnode_xattr_set(struct vnode* restrict node, const struct credentials* restrict cred, const char* restrict name,
    const void* restrict value, size_t size, unsigned flags)
{
    kassert(node);
    kassert(cred);

    if((flags & ~(XATTR_CREATE | XATTR_REPLACE)) || flags == (XATTR_CREATE | XATTR_REPLACE)) {
        return -EINVAL;
    }

    if(size > XATTR_SIZE_MAX) {
        return -E2BIG;
    }

    unsigned space;
    const char* suffix;
    int retval = split_xattr_name(name, &space, &suffix);

    if(retval < 0) {
        return retval;
    }

    struct vnode* target;
    retval = vfs_cross_mount(node, &target);

    if(retval < 0) {
        return retval;
    }

    if(target->fs->readonly) {
        retval = -EROFS;
    }

    if(retval == 0) {
        retval = xattr_permission(target, cred, space, suffix, true);
    }

    if(retval == 0 && target->ops->xattr_set == nullptr) {
        retval = -EOPNOTSUPP;
    }

    if(retval == 0) {
        retval = target->ops->xattr_set(target, space, suffix, value, size, flags);

        if(is_caps_name(space, suffix)) {
            privs_changed(target);
        }
    }

    vnode_decref(target);

    return retval;
}

ssize_t vnode_xattr_list(struct vnode* restrict node, const struct credentials* restrict cred, char* restrict list, size_t size)
{
    kassert(node);
    kassert(cred);

    struct vnode* target;
    int retval = vfs_cross_mount(node, &target);

    if(retval < 0) {
        return retval;
    }

    bool trusted = cred_capable(cred, CAP_SYS_ADMIN);
    size_t length = 0;

    for(size_t index = 0; target->ops->xattr_iterate; index += 1) {
        struct xattr_entry entry;
        retval = target->ops->xattr_iterate(target, &entry, index);

        if(retval < 0) {
            break;
        }

        kassert(entry.space < NUM_XATTR_SPACES);

        if(entry.space == XATTR_TRUSTED && !trusted) {
            continue;
        }

        const char* prefix = xattr_prefixes[entry.space];
        size_t prefix_len = strlen(prefix);
        size_t name_len = strlen(entry.name) + 1;

        if(size && length + prefix_len + name_len > size) {
            retval = -ERANGE;
            break;
        }

        if(size) {
            memcpy(list + length, prefix, prefix_len);
            memcpy(list + length + prefix_len, entry.name, name_len);
        }

        length += prefix_len + name_len;
    }

    vnode_decref(target);

    if(retval < 0 && retval != -ENOENT) {
        return retval;
    }

    return length;
}

int vnode_xattr_remove(struct vnode* restrict node, const struct credentials* restrict cred, const char* restrict name)
{
    kassert(node);
    kassert(cred);

    unsigned space;
    const char* suffix;
    int retval = split_xattr_name(name, &space, &suffix);

    if(retval < 0) {
        return retval;
    }

    struct vnode* target;
    retval = vfs_cross_mount(node, &target);

    if(retval < 0) {
        return retval;
    }

    if(target->fs->readonly) {
        retval = -EROFS;
    }

    if(retval == 0) {
        retval = xattr_permission(target, cred, space, suffix, true);
    }

    if(retval == 0 && target->ops->xattr_remove == nullptr) {
        retval = -EOPNOTSUPP;
    }

    if(retval == 0) {
        retval = target->ops->xattr_remove(target, space, suffix);

        if(is_caps_name(space, suffix)) {
            privs_changed(target);
        }
    }

    vnode_decref(target);

    return retval;
}

// Skips both the permission check and the trip to
// the filesystem once a file is known to have no caps
ssize_t vnode_caps_get(struct vnode* restrict node, void* restrict value, size_t size)
{
    kassert(node);

    struct vnode* target;
    ssize_t retval = vfs_cross_mount(node, &target);

    if(retval < 0) {
        return retval;
    }

    struct privs_state state;
    privs_snapshot(target, &state);

    if(state.caps_state == VNODE_CAPS_NONE || target->ops->xattr_get == nullptr) {
        retval = -ENODATA;
    }
    else {
        retval = target->ops->xattr_get(target, XATTR_SECURITY, XATTR_CAPS_NAME, value, size);
    }

    if(retval == -ENODATA) {
        caps_learned(target, state.seq, VNODE_CAPS_NONE);
    }
    else if(retval >= 0 || retval == -ERANGE) {
        caps_learned(target, state.seq, VNODE_CAPS_PRESENT);
    }

    vnode_decref(target);

    return retval;
}
