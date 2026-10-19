#include <vex/blk_device.h>
#include <vex/errno.h>
#include <vex/file.h>
#include <vex/mount.h>
#include <vex/path.h>
#include <vex/process.h>
#include <vex/statvfs.h>
#include <vex/syscall.h>
#include <vex/thread.h>
#include <vex/uaccess.h>
#include <vex/vnode.h>

static intptr_t sys_mount(const uintptr_t* restrict args)
{
    struct thread* thrd = this_thread();
    struct process* proc = thrd->proc;

    if(proc == nullptr) {
        return -ENOSYS;
    }

    const struct credentials* cred = thrd->cred;

    if(!cred_capable(cred, CAP_MOUNT)) {
        return -EPERM;
    }

    char target[PATH_SIZE];
    ssize_t copied = user_copyin_str(target, args[1], sizeof(target));

    if(copied < 0) {
        return copied;
    }

    char fstype[64];
    copied = user_copyin_str(fstype, args[2], sizeof(fstype));

    if(copied < 0) {
        return copied;
    }

    char params[256];
    const char* params_ptr = nullptr;

    if(args[4]) {
        copied = user_copyin_str(params, args[4], sizeof(params));

        if(copied < 0) {
            return copied;
        }

        params_ptr = params;
    }

    const struct blk_device* device = nullptr;

    if(args[0]) {
        char source[PATH_SIZE];
        copied = user_copyin_str(source, args[0], sizeof(source));

        if(copied < 0) {
            return copied;
        }

        if(source[0] != 0) {
            struct vnode* stem;
            int retval = syscall_dirfd(AT_FDCWD, source, &stem);

            if(retval < 0) {
                return retval;
            }

            struct vnode* source_node;
            retval = path_resolve_at(stem, source, 0, cred, &source_node);

            if(stem) {
                vnode_decref(stem);
            }

            if(retval < 0) {
                return retval;
            }

            if(source_node->nodetype != VNODE_BLOCK_DEV || source_node->blkdev == nullptr) {
                vnode_decref(source_node);
                return -EINVAL;
            }

            device = source_node->blkdev;
            blk_device_incref(device);
            vnode_decref(source_node);
        }
    }

    int retval = vfs_mount(fstype, target, device, params_ptr);

    if(device) {
        blk_device_decref(device);
    }

    return retval;
}

static intptr_t sys_umount(const uintptr_t* restrict args)
{
    struct thread* thrd = this_thread();
    struct process* proc = thrd->proc;

    if(proc == nullptr) {
        return -ENOSYS;
    }

    const struct credentials* cred = thrd->cred;

    if(!cred_capable(cred, CAP_MOUNT)) {
        return -EPERM;
    }

    char path[PATH_SIZE];
    ssize_t copied = user_copyin_str(path, args[0], sizeof(path));

    if(copied < 0) {
        return copied;
    }

    return vfs_unmount(path);
}

static intptr_t sys_sync(const uintptr_t* restrict args)
{
    return vfs_sync_all();
}

static intptr_t syncfs_file(struct file* restrict fp, const uintptr_t* restrict args)
{
    struct vnode* node;
    int retval = vfs_cross_mount(fp->node, &node);

    if(retval < 0) {
        return retval;
    }

    struct vfs* fs = node->fs;

    if(fs == nullptr) {
        retval = -EINVAL;
    }
    else {
        retval = vfs_sync_fs(fs);
    }

    vnode_decref(node);

    return retval;
}

static intptr_t sys_syncfs(const uintptr_t* restrict args)
{
    return syscall_with_fd(args, &syncfs_file);
}

static intptr_t fsync_file(struct file* restrict fp, const uintptr_t* restrict args)
{
    struct vnode* node;
    int retval = vfs_cross_mount(fp->node, &node);

    if(retval < 0) {
        return retval;
    }

    struct vfs* fs = node->fs;

    retval = vnode_fsync(node);

    if(retval == 0 && fs && fs->ops->sync) {
        retval = fs->ops->sync(fs);
    }

    vnode_decref(node);

    return retval;
}

static intptr_t sys_fsync(const uintptr_t* restrict args)
{
    return syscall_with_fd(args, &fsync_file);
}

static intptr_t statvfs_file(struct file* restrict fp, const uintptr_t* restrict args)
{
    struct vnode* node;
    int retval = vfs_cross_mount(fp->node, &node);

    if(retval < 0) {
        return retval;
    }

    struct vfs* fs = node->fs;
    struct k_statvfs st;

    if(fs == nullptr) {
        retval = -EINVAL;
    }
    else {
        retval = vfs_statvfs(fs, &st);
    }

    vnode_decref(node);

    if(retval < 0) {
        return retval;
    }

    ssize_t copied = user_copyout(args[1], &st, sizeof(st));

    if(copied < 0) {
        return copied;
    }

    return 0;
}

static intptr_t sys_statvfs(const uintptr_t* restrict args)
{
    return syscall_with_fd(args, &statvfs_file);
}

define_syscall(sys_mount, SYS_MOUNT);
define_syscall(sys_umount, SYS_UMOUNT);
define_syscall(sys_sync, SYS_SYNC);
define_syscall(sys_syncfs, SYS_SYNCFS);
define_syscall(sys_fsync, SYS_FSYNC);
define_syscall(sys_statvfs, SYS_STATVFS);
