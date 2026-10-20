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

struct vnode_ops_s {
    void (*release)(vnode_t* restrict node);
    int (*lookup)(vnode_t* restrict node, const char* restrict name, ino_t* restrict out);
    int (*iterate)(vnode_t* restrict node, dirent_t* restrict out, size_t index);
    int (*create)(vnode_t* restrict node, const char* restrict name, unsigned type, mode_t mode, const credentials_t* restrict credentials,
        vnode_t** restrict out);
    int (*unlink)(vnode_t* restrict node, const char* restrict name, vnode_t* restrict target);
    int (*rename)(vnode_t* olddir, const char* oldname, vnode_t* source, vnode_t* newdir, const char* newname, vnode_t* victim);
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
    int (*setattr)(vnode_t* restrict node, const vnode_attr_t* restrict attr);
    int (*fsync)(vnode_t* restrict node);
    int (*map_page)(vnode_t* restrict node, uint64_t index, pmap_t* restrict pmap, uintptr_t virt, unsigned vprot);
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
ssize_t vnode_caps_get(vnode_t* restrict node, void* restrict value, size_t size);
