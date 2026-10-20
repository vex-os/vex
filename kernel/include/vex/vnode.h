#pragma once

#include <vex/compiler.h>
#include <vex/cred.h>
#include <vex/dirent.h>
#include <vex/errno.h>
#include <vex/fcache.h>
#include <vex/spinlock.h>
#include <vex/stat.h>
#include <vex/time.h>
#include <vex/types.h>
#include <vex/xattr.h>

typedef struct pmap_s pmap_t;
typedef struct vcache_entry_s vcache_entry_t;
typedef struct blk_device_s blk_device_t;
typedef struct vfs_s vfs_t;

enum {
    VNODE_FILE,
    VNODE_DIRECTORY,
    VNODE_BLOCK_DEV,
    VNODE_CHAR_DEV,
    VNODE_SYM_LINK,
};

enum {
    VNODE_CAPS_UNKNOWN,
    VNODE_CAPS_NONE,
    VNODE_CAPS_PRESENT,
};

#define VATTR_MODE  0x0001U
#define VATTR_UID   0x0002U
#define VATTR_GID   0x0004U
#define VATTR_SIZE  0x0008U
#define VATTR_ATIME 0x0010U
#define VATTR_MTIME 0x0020U
#define VATTR_CTIME 0x0040U

typedef struct vnode_attr_s vnode_attr_t;
typedef struct vnode_s vnode_t;
typedef struct vnode_ops_s vnode_ops_t;

struct vnode_attr_s {
    unsigned mask; // VATTR; fields to change, the others are ignored
    mode_t mode;
    uid_t uid;
    gid_t gid;
    off_t size;
    timespec_t atime;
    timespec_t mtime;
    timespec_t ctime;
};

/// Implemented by each filesystem. An operation left nullptr makes the VFS fail the call.
/// By the time an operation runs, the VFS has crossed mount points and turned away "."
/// and ".." as new names, links or renames across filesystems, and changes to a read-only
/// filesystem, writes included as file_open refuses those; the filesystem serializes its
/// operations with its own lock
struct vnode_ops_s {
    /// Called once the last reference is gone: frees ctxdata, and the
    /// storage of a file whose last name was removed while it was open
    void (*release)(vnode_t* restrict node);

    /// Finds the inode number only; the VFS takes the vnode from the vcache or builds it through vfs_ops_t.fillnode
    int (*lookup)(vnode_t* restrict node, const char* restrict name, ino_t* restrict out);

    /// Fills the entry at index, counting entries a reader sees; -ENOENT past the last one
    int (*iterate)(vnode_t* restrict node, dirent_t* restrict out, size_t index);

    /// Hands out a new vnode with one reference and fs and ino set, not yet in the vcache, which
    /// the VFS inserts it into; on failure nothing is handed out. symlink follows the same rules
    int (*create)(vnode_t* restrict node, const char* restrict name, unsigned type, mode_t mode, const credentials_t* restrict credentials,
        vnode_t** restrict out);

    /// The target is the vnode the name led to. Once its last name is gone, the filesystem calls vcache_remove
    /// on it under its own lock, before the entry can be reused, or a new file there would get this vnode
    int (*unlink)(vnode_t* restrict node, const char* restrict name, vnode_t* restrict target);

    /// Same rules as unlink for a victim left without names, and the victim may be nullptr. Where the inode number
    /// follows from the entry's location, the source gets vcache_rekey under the same lock; the VFS has already
    /// filtered out renaming a file onto itself and a directory into its own subtree
    int (*rename)(vnode_t* olddir, const char* oldname, vnode_t* source, vnode_t* newdir, const char* newname, vnode_t* victim);

    /// The target is never a directory
    int (*link)(vnode_t* restrict node, const char* restrict name, vnode_t* restrict target);
    int (*symlink)(vnode_t* restrict node, const char* restrict name, const char* restrict target,
        const credentials_t* restrict credentials, vnode_t** restrict out);
    int (*open)(vnode_t* restrict node, unsigned mode);
    int (*close)(vnode_t* restrict node);
    ssize_t (*read)(vnode_t* restrict node, void* restrict buffer, size_t size, off_t offset);
    ssize_t (*write)(vnode_t* restrict node, const void* restrict buffer, size_t size, off_t offset);
    ssize_t (*readlink)(vnode_t* restrict node, char* restrict buf, size_t size);
    int (*ioctl)(vnode_t* restrict node, uint32_t request, uintptr_t argument);
    int (*stat)(vnode_t* restrict node, stat_t* restrict out);

    /// Applies the fields in attr->mask; permission checks are done, and the VFS has
    /// already added the ctime, and the mtime for a size change, unless the caller set them
    int (*setattr)(vnode_t* restrict node, const vnode_attr_t* restrict attr);
    int (*fsync)(vnode_t* restrict node);

    /// Maps page index of the file at virt, normally through fcache_map_page
    int (*map_page)(vnode_t* restrict node, uint64_t index, pmap_t* restrict pmap, uintptr_t virt, unsigned vprot);

    /// The xattr operations get the namespace as an XATTR space and the name without its
    /// prefix; permission checks are done. A missing attribute is -ENODATA, and iterate
    /// returns -ENOENT past the last one
    ssize_t (*xattr_get)(vnode_t* restrict node, unsigned space, const char* restrict name, void* restrict value, size_t size);
    int (*xattr_set)(vnode_t* restrict node, unsigned space, const char* restrict name, const void* restrict value, size_t size,
        unsigned flags);
    int (*xattr_iterate)(vnode_t* restrict node, xattr_entry_t* restrict out, size_t index);
    int (*xattr_remove)(vnode_t* restrict node, unsigned space, const char* restrict name);
};

struct vnode_s {
    vfs_t* fs;
    vfs_t* mounted_fs;
    vnode_t* parent;
    const vnode_ops_t* ops;
    spinlock_t lock;
    unsigned refcount;
    unsigned type;
    bool orphaned;
    unsigned privs_seq;
    unsigned caps_state;
    bool privs_clear;
    ino_t ino;
    vcache_entry_t* cache_entry;
    const blk_device_t* device;
    fcache_t pages;
    void* ctxdata;
};

vnode_t* vnode_alloc(unsigned type, const vnode_ops_t* restrict ops, void* restrict ctxdata) __nodiscard;
void vnode_incref(vnode_t* restrict node);
bool vnode_decref(vnode_t* restrict node);
void vnode_destroy(vnode_t* restrict node);

int vnode_lookup(vnode_t* restrict node, const char* restrict name, vnode_t** restrict out);
int vnode_iterate(vnode_t* restrict node, dirent_t* restrict out, size_t index);
int vnode_create(vnode_t* restrict node, const char* restrict name, unsigned type, mode_t mode, const credentials_t* restrict credentials,
    vnode_t** restrict out);
int vnode_unlink(vnode_t* restrict node, const char* restrict name, bool directory);
int vnode_rename(vnode_t* olddir, const char* oldname, vnode_t* newdir, const char* newname);
int vnode_link(vnode_t* restrict node, const char* restrict name, vnode_t* restrict target);
int vnode_symlink(vnode_t* restrict node, const char* restrict name, const char* restrict target, const credentials_t* restrict credentials,
    vnode_t** restrict out);
int vnode_open(vnode_t* restrict node, unsigned mode);
int vnode_close(vnode_t* restrict node);
ssize_t vnode_read(vnode_t* restrict node, void* restrict buffer, size_t size, off_t offset);
ssize_t vnode_write(vnode_t* restrict node, const void* restrict buffer, size_t size, off_t offset);
ssize_t vnode_readlink(vnode_t* restrict node, char* restrict buf, size_t size);
int vnode_stat(vnode_t* restrict node, stat_t* restrict out);
/// Every change made through here is a status change, so the ctime
/// moves with it unless the caller set one; a new size also modifies
int vnode_setattr(vnode_t* restrict node, const vnode_attr_t* restrict request);
int vnode_chmod(vnode_t* restrict node, const credentials_t* restrict credentials, mode_t mode);
int vnode_chown(vnode_t* restrict node, const credentials_t* restrict credentials, uid_t uid, gid_t gid);
int vnode_utimes(vnode_t* restrict node, const credentials_t* restrict credentials, const timespec_t* restrict times);
int vnode_may_delete(vnode_t* restrict dir, const char* restrict name, const credentials_t* restrict credentials) __nodiscard;
int vnode_access(vnode_t* restrict node, const credentials_t* restrict credentials, unsigned mask) __nodiscard;
int vnode_remove_privs(vnode_t* restrict node, const credentials_t* restrict credentials) __nodiscard;
int vnode_ioctl(vnode_t* restrict node, uint32_t request, uintptr_t argument);
int vnode_fsync(vnode_t* restrict node);
int vnode_map_page(vnode_t* restrict node, uint64_t index, pmap_t* restrict pmap, uintptr_t virt, unsigned vprot);
ssize_t vnode_xattr_get(vnode_t* restrict node, const credentials_t* restrict credentials, const char* restrict name, void* restrict value,
    size_t size);
int vnode_xattr_set(vnode_t* restrict node, const credentials_t* restrict credentials, const char* restrict name,
    const void* restrict value, size_t size, unsigned flags);
ssize_t vnode_xattr_list(vnode_t* restrict node, const credentials_t* restrict credentials, char* restrict list, size_t size);
int vnode_xattr_remove(vnode_t* restrict node, const credentials_t* restrict credentials, const char* restrict name);
/// Skips both the permission check and the trip to
/// the filesystem once a file is known to have no caps
ssize_t vnode_caps_get(vnode_t* restrict node, void* restrict value, size_t size);
