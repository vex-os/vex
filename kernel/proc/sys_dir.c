#include <vex/dirent.h>
#include <vex/errno.h>
#include <vex/fcntl.h>
#include <vex/file.h>
#include <vex/path.h>
#include <vex/process.h>
#include <vex/string.h>
#include <vex/syscall.h>
#include <vex/thread.h>
#include <vex/uaccess.h>
#include <vex/vnode.h>

static intptr_t sys_unlinkat(const uintptr_t* restrict args)
{
    unsigned flags = args[2];

    if(flags & ~AT_REMOVEDIR) {
        return -EINVAL;
    }

    char path[PATH_SIZE];
    ssize_t copied = user_copyin_str(path, args[1], sizeof(path));

    if(copied < 0) {
        return copied;
    }

    struct vnode* stem;
    int retval = syscall_dirfd(args[0], path, &stem);

    if(retval < 0) {
        return retval;
    }

    const char* name;
    struct vnode* parent;
    retval = path_resolve_parent_at(stem, path, &parent, &name);

    if(stem) {
        vnode_decref(stem);
    }

    if(retval < 0) {
        return retval;
    }

    retval = vnode_unlink(parent, name, flags & AT_REMOVEDIR);

    vnode_decref(parent);

    return retval;
}

static intptr_t sys_mkdirat(const uintptr_t* restrict args)
{
    char path[PATH_SIZE];
    ssize_t copied = user_copyin_str(path, args[1], sizeof(path));

    if(copied < 0) {
        return copied;
    }

    struct vnode* stem;
    int retval = syscall_dirfd(args[0], path, &stem);

    if(retval < 0) {
        return retval;
    }

    const char* name;
    struct vnode* parent;
    retval = path_resolve_parent_at(stem, path, &parent, &name);

    if(stem) {
        vnode_decref(stem);
    }

    if(retval < 0) {
        return retval;
    }

    struct vnode* created;
    retval = vnode_create(parent, name, VNODE_DIRECTORY, &created);

    vnode_decref(parent);

    if(retval < 0) {
        return retval;
    }

    vnode_decref(created);

    return 0;
}

static int resolve_rename_parent(int dirfd, const char* restrict path, struct vnode** restrict out_parent, const char** restrict out_name)
{
    struct vnode* stem;
    int retval = syscall_dirfd(dirfd, path, &stem);

    if(retval < 0) {
        return retval;
    }

    retval = path_resolve_parent_at(stem, path, out_parent, out_name);

    if(stem) {
        vnode_decref(stem);
    }

    return retval;
}

static intptr_t sys_renameat(const uintptr_t* restrict args)
{
    char oldpath[PATH_SIZE];
    ssize_t copied = user_copyin_str(oldpath, args[1], sizeof(oldpath));

    if(copied < 0) {
        return copied;
    }

    char newpath[PATH_SIZE];
    copied = user_copyin_str(newpath, args[3], sizeof(newpath));

    if(copied < 0) {
        return copied;
    }

    const char* oldname;
    struct vnode* olddir;
    int retval = resolve_rename_parent(args[0], oldpath, &olddir, &oldname);

    if(retval < 0) {
        return retval;
    }

    const char* newname;
    struct vnode* newdir;
    retval = resolve_rename_parent(args[2], newpath, &newdir, &newname);

    if(retval < 0) {
        vnode_decref(olddir);
        return retval;
    }

    retval = vnode_rename(olddir, oldname, newdir, newname);

    vnode_decref(newdir);
    vnode_decref(olddir);

    return retval;
}

static intptr_t readdir_file(struct file* restrict fp, const uintptr_t* restrict args)
{
    struct k_dirent entry;
    int retval = vnode_iterate(fp->node, &entry, fp->offset);

    if(retval == -ENOENT) {
        return 0;
    }

    if(retval < 0) {
        return retval;
    }

    ssize_t copied = user_copyout(args[1], &entry, sizeof(entry));

    if(copied < 0) {
        return copied;
    }

    fp->offset += 1;

    return 1;
}

static intptr_t sys_readdir(const uintptr_t* restrict args)
{
    return syscall_with_fd(args, &readdir_file);
}

static intptr_t sys_getcwd(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

static intptr_t sys_chdir(const uintptr_t* restrict args)
{
    struct thread* thread = this_thread();
    struct process* proc = thread->proc;

    if(proc == nullptr) {
        return -ENOSYS;
    }

    char path[PATH_SIZE];
    ssize_t copied = user_copyin_str(path, args[0], sizeof(path));

    if(copied < 0) {
        return copied;
    }

    struct vnode* stem;
    int retval = syscall_dirfd(AT_FDCWD, path, &stem);

    if(retval < 0) {
        return retval;
    }

    struct vnode* node;
    retval = path_resolve_at(stem, path, 0, &node);

    if(stem) {
        vnode_decref(stem);
    }

    if(retval < 0) {
        return retval;
    }

    if(node->nodetype != VNODE_DIRECTORY) {
        vnode_decref(node);
        return -ENOTDIR;
    }

    process_set_cwd(proc, node);

    return 0;
}

static intptr_t sys_fchdir(const uintptr_t* restrict args)
{
    struct thread* thread = this_thread();
    struct process* proc = thread->proc;

    if(proc == nullptr) {
        return -ENOSYS;
    }

    int fd = args[0];

    if(fd == AT_FDCWD) {
        return -EBADF;
    }

    // An empty path is never absolute, so this
    // always hands back the directory behind fd
    struct vnode* node;
    int retval = syscall_dirfd(fd, "", &node);

    if(retval < 0) {
        return retval;
    }

    process_set_cwd(proc, node);

    return 0;
}

define_syscall(sys_unlinkat, SYS_UNLINKAT);
define_syscall(sys_mkdirat, SYS_MKDIRAT);
define_syscall(sys_renameat, SYS_RENAMEAT);
define_syscall(sys_readdir, SYS_READDIR);
define_syscall(sys_getcwd, SYS_GETCWD);
define_syscall(sys_chdir, SYS_CHDIR);
define_syscall(sys_fchdir, SYS_FCHDIR);
