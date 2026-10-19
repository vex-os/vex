#pragma once

#include <vex/compiler.h>
#include <vex/cred.h>
#include <vex/dirent.h>
#include <vex/errno.h>
#include <vex/fcache.h>
#include <vex/spinlock.h>
#include <vex/stat.h>
#include <vex/types.h>
#include <vex/xattr.h>

enum {
    VNODE_FILE,
    VNODE_DIRECTORY,
    VNODE_BLOCK_DEV,
    VNODE_CHAR_DEV,
    VNODE_SYM_LINK,
};

#define VATTR_MODE  0x0001U
#define VATTR_UID   0x0002U
#define VATTR_GID   0x0004U
#define VATTR_SIZE  0x0008U
#define VATTR_ATIME 0x0010U
#define VATTR_MTIME 0x0020U

struct vattr {
    unsigned mask; // VATTR; fields to change, the others are ignored
    mode_t mode;
    uid_t uid;
    gid_t gid;
    off_t size;
    time_t atime;
    time_t mtime;
};

struct pagemap;
struct vnode;
struct vnode_ops {
    void (*release)(struct vnode* restrict node);
    int (*lookup)(struct vnode* restrict node, const char* restrict name, ino_t* restrict out);
    int (*iterate)(struct vnode* restrict node, struct k_dirent* restrict out, size_t index);
    int (*create)(struct vnode* restrict node, const char* restrict name, unsigned type, mode_t mode,
        const struct credentials* restrict cred, struct vnode** restrict out);
    int (*unlink)(struct vnode* restrict node, const char* restrict name, struct vnode* restrict target);
    int (*rename)(struct vnode* olddir, const char* oldname, struct vnode* source, struct vnode* newdir, const char* newname,
        struct vnode* victim);
    int (*link)(struct vnode* restrict node, const char* restrict name, struct vnode* restrict target);
    int (*symlink)(struct vnode* restrict node, const char* restrict name, const char* restrict target,
        const struct credentials* restrict cred, struct vnode** restrict out);
    int (*open)(struct vnode* restrict node, unsigned mode);
    int (*close)(struct vnode* restrict node);
    ssize_t (*read)(struct vnode* restrict node, void* restrict buffer, size_t sz, off_t offset);
    ssize_t (*write)(struct vnode* restrict node, const void* restrict buffer, size_t sz, off_t offset);
    ssize_t (*readlink)(struct vnode* restrict node, char* restrict buf, size_t sz);
    int (*ioctl)(struct vnode* restrict node, uint32_t request, uintptr_t argument);
    int (*stat)(struct vnode* restrict node, struct k_stat* restrict out);
    int (*setattr)(struct vnode* restrict node, const struct vattr* restrict attr);
    int (*fsync)(struct vnode* restrict node);
    int (*map_page)(struct vnode* restrict node, uint64_t index, struct pagemap* restrict pmap, uintptr_t virt, unsigned vprot);
    ssize_t (*xattr_get)(struct vnode* restrict node, unsigned space, const char* restrict name, void* restrict value, size_t size);
    int (*xattr_set)(struct vnode* restrict node, unsigned space, const char* restrict name, const void* restrict value, size_t size,
        unsigned flags);
    int (*xattr_iterate)(struct vnode* restrict node, struct xattr_entry* restrict out, size_t index);
    int (*xattr_remove)(struct vnode* restrict node, unsigned space, const char* restrict name);
};

struct blk_device;
struct vfs;
struct vnode {
    struct vfs* fs;
    struct vfs* fs_mounted;
    struct vnode* parent;
    const struct vnode_ops* ops;
    spinlock_type lock;
    unsigned refcount;
    unsigned nodetype;
    bool orphaned;
    ino_t nodenumber;
    struct vcache* cache_entry;
    const struct blk_device* blkdev;
    struct fcache pages;
    void* ctxdata;
};

struct vnode* vnode_alloc(unsigned nodetype, const struct vnode_ops* restrict ops, void* restrict ctxdata) __nodiscard;
void vnode_incref(struct vnode* restrict node);
bool vnode_decref(struct vnode* restrict node);
void vnode_destroy(struct vnode* restrict node);

int vnode_lookup(struct vnode* restrict node, const char* restrict name, struct vnode** restrict out);
int vnode_iterate(struct vnode* restrict node, struct k_dirent* restrict out, size_t index);
int vnode_create(struct vnode* restrict node, const char* restrict name, unsigned type, mode_t mode,
    const struct credentials* restrict cred, struct vnode** restrict out);
int vnode_unlink(struct vnode* restrict node, const char* restrict name, bool directory);
int vnode_rename(struct vnode* olddir, const char* oldname, struct vnode* newdir, const char* newname);
int vnode_link(struct vnode* restrict node, const char* restrict name, struct vnode* restrict target);
int vnode_symlink(struct vnode* restrict node, const char* restrict name, const char* restrict target,
    const struct credentials* restrict cred, struct vnode** restrict out);
int vnode_open(struct vnode* restrict node, unsigned mode);
int vnode_close(struct vnode* restrict node);
ssize_t vnode_read(struct vnode* restrict node, void* restrict buffer, size_t sz, off_t offset);
ssize_t vnode_write(struct vnode* restrict node, const void* restrict buffer, size_t sz, off_t offset);
ssize_t vnode_readlink(struct vnode* restrict node, char* restrict buf, size_t sz);
int vnode_stat(struct vnode* restrict node, struct k_stat* restrict out);
int vnode_setattr(struct vnode* restrict node, const struct vattr* restrict attr);
int vnode_chmod(struct vnode* restrict node, const struct credentials* restrict cred, mode_t mode);
int vnode_chown(struct vnode* restrict node, const struct credentials* restrict cred, uid_t uid, gid_t gid);
int vnode_utimes(struct vnode* restrict node, const struct credentials* restrict cred, const struct k_timespec* restrict times);
int vnode_may_delete(struct vnode* restrict dir, const char* restrict name, const struct credentials* restrict cred) __nodiscard;
int vnode_access(struct vnode* restrict node, const struct credentials* restrict cred, unsigned mask) __nodiscard;
int vnode_ioctl(struct vnode* restrict node, uint32_t request, uintptr_t argument);
int vnode_fsync(struct vnode* restrict node);
int vnode_map_page(struct vnode* restrict node, uint64_t index, struct pagemap* restrict pmap, uintptr_t virt, unsigned vprot);
ssize_t vnode_xattr_get(struct vnode* restrict node, const struct credentials* restrict cred, const char* restrict name,
    void* restrict value, size_t size);
int vnode_xattr_set(struct vnode* restrict node, const struct credentials* restrict cred, const char* restrict name,
    const void* restrict value, size_t size, unsigned flags);
ssize_t vnode_xattr_list(struct vnode* restrict node, const struct credentials* restrict cred, char* restrict list, size_t size);
int vnode_xattr_remove(struct vnode* restrict node, const struct credentials* restrict cred, const char* restrict name);
