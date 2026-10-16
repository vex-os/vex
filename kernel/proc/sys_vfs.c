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
            retval = path_resolve_at(stem, source, 0, &source_node);

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
            vnode_decref(source_node);
        }
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

    return vfs_mount(fstype, target, device, params_ptr);
}

static intptr_t sys_umount(const uintptr_t* restrict args)
{
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
    struct vfs* fs = fp->node->fs;

    if(fs == nullptr) {
        return -EINVAL;
    }

    return vfs_sync_fs(fs);
}

static intptr_t sys_syncfs(const uintptr_t* restrict args)
{
    return syscall_with_fd(args, &syncfs_file);
}

static intptr_t statvfs_file(struct file* restrict fp, const uintptr_t* restrict args)
{
    struct vfs* fs = fp->node->fs;

    if(fs == nullptr) {
        return -EINVAL;
    }

    struct k_statvfs st;
    int retval = vfs_statvfs(fs, &st);

    if(retval < 0) {
        return retval;
    }

    return user_copyout(args[1], &st, sizeof(st));
}

static intptr_t sys_statvfs(const uintptr_t* restrict args)
{
    return syscall_with_fd(args, &statvfs_file);
}

define_syscall(sys_mount, SYS_MOUNT);
define_syscall(sys_umount, SYS_UMOUNT);
define_syscall(sys_sync, SYS_SYNC);
define_syscall(sys_syncfs, SYS_SYNCFS);
define_syscall(sys_statvfs, SYS_STATVFS);
