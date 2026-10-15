#include <vex/errno.h>
#include <vex/file.h>
#include <vex/path.h>
#include <vex/process.h>
#include <vex/stat.h>
#include <vex/syscall.h>
#include <vex/thread.h>
#include <vex/uaccess.h>
#include <vex/vnode.h>

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

    return release_fd(proc, args[0]);
}

static intptr_t read_file(struct file* restrict fp, const uintptr_t* restrict args)
{
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

static intptr_t sys_read(const uintptr_t* restrict args)
{
    return syscall_with_fd(args, &read_file);
}

static intptr_t write_file(struct file* restrict fp, const uintptr_t* restrict args)
{
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

        if(num_written == 0) {
            break;
        }

        user_buf += num_written;
        total += num_written;
        remaining -= num_written;
    }

    return total;
}

static intptr_t sys_write(const uintptr_t* restrict args)
{
    return syscall_with_fd(args, &write_file);
}

static intptr_t seek_file(struct file* restrict fp, const uintptr_t* restrict args)
{
    return file_seek(fp, args[1], args[2]);
}

static intptr_t sys_seek(const uintptr_t* restrict args)
{
    return syscall_with_fd(args, &seek_file);
}

static intptr_t stat_file(struct file* restrict fp, const uintptr_t* restrict args)
{
    struct k_stat st;
    int retval = vnode_stat(fp->node, &st);

    if(retval < 0) {
        return retval;
    }

    return user_copyout(args[1], &st, sizeof(st));
}

static intptr_t sys_stat(const uintptr_t* restrict args)
{
    return syscall_with_fd(args, &stat_file);
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

static intptr_t ioctl_file(struct file* restrict fp, const uintptr_t* restrict args)
{
    return file_ioctl(fp, args[1], args[2]);
}

static intptr_t sys_ioctl(const uintptr_t* restrict args)
{
    return syscall_with_fd(args, &ioctl_file);
}

define_syscall(sys_open, SYS_OPEN);
define_syscall(sys_close, SYS_CLOSE);
define_syscall(sys_read, SYS_READ);
define_syscall(sys_write, SYS_WRITE);
define_syscall(sys_seek, SYS_SEEK);
define_syscall(sys_stat, SYS_STAT);
define_syscall(sys_dup, SYS_DUP);
define_syscall(sys_dup2, SYS_DUP2);
define_syscall(sys_ioctl, SYS_IOCTL);
