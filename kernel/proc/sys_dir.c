#include <vex/dirent.h>
#include <vex/errno.h>
#include <vex/file.h>
#include <vex/path.h>
#include <vex/process.h>
#include <vex/string.h>
#include <vex/syscall.h>
#include <vex/thread.h>
#include <vex/uaccess.h>
#include <vex/vnode.h>

static intptr_t sys_unlink(const uintptr_t* restrict args)
{
    char path[PATH_SIZE];
    ssize_t copied = user_copyin_str(path, args[0], sizeof(path));

    if(copied < 0) {
        return copied;
    }

    const char* name;
    struct vnode* parent;
    int retval = path_resolve_parent(path, &parent, &name);

    if(retval < 0) {
        return retval;
    }

    retval = vnode_unlink(parent, name);

    vnode_decref(parent);

    return retval;
}

static intptr_t sys_mkdir(const uintptr_t* restrict args)
{
    char path[PATH_SIZE];
    ssize_t copied = user_copyin_str(path, args[0], sizeof(path));

    if(copied < 0) {
        return copied;
    }

    const char* name;
    struct vnode* parent;
    int retval = path_resolve_parent(path, &parent, &name);

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

static intptr_t sys_readdir(const uintptr_t* restrict args)
{
    struct thread* thread = this_thread();
    struct process* proc = thread->proc;

    if(proc == nullptr) {
        return -ENOSYS;
    }

    struct file* fp = dereference_fd(proc, args[0]);

    if(fp == nullptr) {
        return -EBADF;
    }

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

    struct vnode* node;
    int retval = path_resolve(path, &node);

    if(retval < 0) {
        return retval;
    }

    if(node->nodetype != VNODE_DIRECTORY) {
        vnode_decref(node);
        return -ENOTDIR;
    }

    if(proc->cwd) {
        vnode_decref(proc->cwd);
    }

    proc->cwd = node;

    return 0;
}

define_syscall(sys_unlink, SYS_UNLINK);
define_syscall(sys_mkdir, SYS_MKDIR);
define_syscall(sys_readdir, SYS_READDIR);
define_syscall(sys_getcwd, SYS_GETCWD);
define_syscall(sys_chdir, SYS_CHDIR);
