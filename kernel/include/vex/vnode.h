#pragma once

#include <vex/compiler.h>
#include <vex/cred.h>
#include <vex/dirent.h>
#include <vex/errno.h>
#include <vex/spinlock.h>
#include <vex/stat.h>
#include <vex/types.h>

enum {
    VNODE_FILE,
    VNODE_DIRECTORY,
    VNODE_BLOCK_DEV,
    VNODE_CHAR_DEV,
    VNODE_SYM_LINK,
};

#define VATTR_MODE 0x0001U
#define VATTR_UID  0x0002U
#define VATTR_GID  0x0004U
#define VATTR_SIZE 0x0008U

struct vattr {
    unsigned mask; // VATTR_* fields to change, the others are ignored
    mode_t mode;
    uid_t uid;
    gid_t gid;
    off_t size;
};

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
    int (*open)(struct vnode* restrict node, unsigned mode);
    int (*close)(struct vnode* restrict node);
    ssize_t (*read)(struct vnode* restrict node, void* restrict buffer, size_t sz, off_t offset);
    ssize_t (*write)(struct vnode* restrict node, const void* restrict buffer, size_t sz, off_t offset);
    ssize_t (*readlink)(struct vnode* restrict node, char* restrict buf, size_t sz);
    int (*ioctl)(struct vnode* restrict node, uint32_t request, uintptr_t argument);
    int (*stat)(struct vnode* restrict node, struct k_stat* restrict out);
    int (*setattr)(struct vnode* restrict node, const struct vattr* restrict attr);
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
int vnode_open(struct vnode* restrict node, unsigned mode);
int vnode_close(struct vnode* restrict node);
ssize_t vnode_read(struct vnode* restrict node, void* restrict buffer, size_t sz, off_t offset);
ssize_t vnode_write(struct vnode* restrict node, const void* restrict buffer, size_t sz, off_t offset);
ssize_t vnode_readlink(struct vnode* restrict node, char* restrict buf, size_t sz);
int vnode_stat(struct vnode* restrict node, struct k_stat* restrict out);
int vnode_setattr(struct vnode* restrict node, const struct vattr* restrict attr);
int vnode_chmod(struct vnode* restrict node, const struct credentials* restrict cred, mode_t mode);
int vnode_chown(struct vnode* restrict node, const struct credentials* restrict cred, uid_t uid, gid_t gid);
int vnode_access(struct vnode* restrict node, const struct credentials* restrict cred, unsigned mask) __nodiscard;
int vnode_ioctl(struct vnode* restrict node, uint32_t request, uintptr_t argument);
