#include <vex/errno.h>
#include <vex/process.h>
#include <vex/stat.h>
#include <vex/string.h>
#include <vex/syscall.h>
#include <vex/thread.h>
#include <vex/uaccess.h>
#include <vex/vfs_dirent.h>
#include <vex/vfs_file.h>
#include <vex/vfs_path.h>
#include <vex/vfs_vnode.h>

static int resolve_parent(const char* path, struct vnode** out_parent, const char** out_name)
{
    const char* slash = nullptr;

    for(const char* it = path; *it; ++it) {
        if(*it == PATH_SEPARATOR) {
            slash = it;
        }
    }

    *out_name = slash ? &slash[1] : path;

    if(slash == nullptr || slash == path) {
        return path_resolve("/", out_parent);
    }

    char dirpath[NAME_SIZE];
    size_t dir_length = slash - path;

    if(dir_length >= NAME_SIZE) {
        return -ENAMETOOLONG;
    }

    memcpy(dirpath, path, dir_length);
    dirpath[dir_length] = 0x00;

    return path_resolve(dirpath, out_parent);
}

static intptr_t sys_open(const uintptr_t* restrict args)
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

    struct file* fp = file_open(path, args[1]);

    if(fp == nullptr) {
        return -ENOENT;
    }

    int fd = allocate_fd(proc, fp);

    if(fd < 0) {
        file_close(fp);
        return fd;
    }

    return fd;
}

static intptr_t sys_close(const uintptr_t* restrict args)
{
    struct thread* thread = this_thread();
    struct process* proc = thread->proc;

    if(proc == nullptr) {
        return -ENOSYS;
    }

    release_fd(proc, args[0]);
    return 0;
}

static intptr_t sys_read(const uintptr_t* restrict args)
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

    uintptr_t user_buf = args[1];
    size_t remaining = args[2];
    size_t total = 0;

    char buffer[512];

    while(remaining > 0) {
        size_t chunk;

        if(remaining > sizeof(buffer)) {
            chunk = sizeof(buffer);
        }
        else {
            chunk = remaining;
        }

        ssize_t num_read = file_read(fp, buffer, chunk);

        if(num_read < 0) {
            if(total > 0)
                return total;
            return num_read;
        }

        if(num_read == 0) {
            break;
        }

        ssize_t copied = user_copyout(user_buf, buffer, num_read);

        if(copied < 0) {
            return copied;
        }

        user_buf += num_read;
        total += num_read;
        remaining -= num_read;
    }

    return total;
}

static intptr_t sys_write(const uintptr_t* restrict args)
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

    uintptr_t user_buf = args[1];
    size_t remaining = args[2];
    size_t total = 0;

    char buffer[512];

    while(remaining > 0) {
        size_t chunk;

        if(remaining > sizeof(buffer)) {
            chunk = sizeof(buffer);
        }
        else {
            chunk = remaining;
        }

        ssize_t copied = user_copyin(buffer, user_buf, chunk);

        if(copied < 0) {
            return copied;
        }

        ssize_t num_written = file_write(fp, buffer, chunk);

        if(num_written < 0) {
            if(total > 0)
                return total;
            return num_written;
        }

        user_buf += num_written;
        total += num_written;
        remaining -= num_written;
    }

    return total;
}

static intptr_t sys_seek(const uintptr_t* restrict args)
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

    return file_seek(fp, args[1], args[2]);
}

static intptr_t sys_stat(const uintptr_t* restrict args)
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

    struct k_stat st;
    int retval = vnode_stat(fp->node, &st);

    if(retval < 0) {
        return retval;
    }

    return user_copyout(args[1], &st, sizeof(st));
}

static intptr_t sys_unlink(const uintptr_t* restrict args)
{
    char path[PATH_SIZE];
    ssize_t copied = user_copyin_str(path, args[0], sizeof(path));

    if(copied < 0) {
        return copied;
    }

    struct vnode* parent;
    const char* name;
    int retval = resolve_parent(path, &parent, &name);

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

    struct vnode* parent;
    const char* name;
    int retval = resolve_parent(path, &parent, &name);

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

    struct dirent entry;
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

static intptr_t sys_dup(const uintptr_t* restrict args)
{
    struct thread* thread = this_thread();
    struct process* proc = thread->proc;

    if(proc == nullptr) {
        return -ENOSYS;
    }

    return duplicate_fd(proc, args[0], -1);
}

static intptr_t sys_dup2(const uintptr_t* restrict args)
{
    struct thread* thread = this_thread();
    struct process* proc = thread->proc;

    if(proc == nullptr) {
        return -ENOSYS;
    }

    int old_fd = args[0];
    int new_fd = args[1];

    if(new_fd < 0) {
        return -EBADF;
    }

    return duplicate_fd(proc, old_fd, new_fd);
}

static intptr_t sys_ioctl(const uintptr_t* restrict args)
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

    return file_ioctl(fp, args[1], args[2]);
}

define_syscall(sys_open, SYS_OPEN);
define_syscall(sys_close, SYS_CLOSE);
define_syscall(sys_read, SYS_READ);
define_syscall(sys_write, SYS_WRITE);
define_syscall(sys_seek, SYS_SEEK);
define_syscall(sys_stat, SYS_STAT);
define_syscall(sys_unlink, SYS_UNLINK);
define_syscall(sys_mkdir, SYS_MKDIR);
define_syscall(sys_readdir, SYS_READDIR);
define_syscall(sys_getcwd, SYS_GETCWD);
define_syscall(sys_chdir, SYS_CHDIR);
define_syscall(sys_dup, SYS_DUP);
define_syscall(sys_dup2, SYS_DUP2);
define_syscall(sys_ioctl, SYS_IOCTL);
