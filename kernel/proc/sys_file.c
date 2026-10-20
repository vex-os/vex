#include <vex/atomic.h>
#include <vex/errno.h>
#include <vex/file.h>
#include <vex/path.h>
#include <vex/process.h>
#include <vex/stat.h>
#include <vex/syscall.h>
#include <vex/thread.h>
#include <vex/time.h>
#include <vex/uaccess.h>
#include <vex/vnode.h>

static intptr_t read_file(file_t* restrict file, const uintptr_t* restrict args)
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

        ssize_t num_read = file_read(file, buffer, chunk);

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

static intptr_t write_file(file_t* restrict file, const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();
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

        ssize_t num_written = file_write(file, thread->credentials, buffer, chunk);

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

static intptr_t seek_file(file_t* restrict file, const uintptr_t* restrict args)
{
    return file_seek(file, args[1], args[2]);
}

static intptr_t ftruncate_file(file_t* restrict file, const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();

    return file_truncate(file, thread->credentials, args[1]);
}

static intptr_t fstat_file(file_t* restrict file, const uintptr_t* restrict args)
{
    stat_t st;
    int retval = vnode_stat(file->node, &st);

    if(retval < 0) {
        return retval;
    }

    ssize_t copied = user_copyout(args[1], &st, sizeof(st));

    if(copied < 0) {
        return copied;
    }

    return 0;
}

static intptr_t ioctl_file(file_t* restrict file, const uintptr_t* restrict args)
{
    return file_ioctl(file, args[1], args[2]);
}

static intptr_t sys_openat(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ENOSYS;
    }

    char path[PATH_SIZE];
    ssize_t copied = user_copyin_str(path, args[1], sizeof(path));

    if(copied < 0) {
        return copied;
    }

    vnode_t* base_dir;
    int retval = syscall_dirfd(args[0], path, &base_dir);

    if(retval < 0) {
        return retval;
    }

    const credentials_t* credentials = thread->credentials;
    mode_t umask = atomic_load_n(&process->umask, ATOMIC_RELAXED);

    file_t* file;
    retval = file_open(base_dir, path, args[2], args[3] & ~umask, credentials, &file);

    if(base_dir) {
        vnode_decref(base_dir);
    }

    if(retval < 0) {
        return retval;
    }

    int fd = process_fd_alloc(process, file);

    if(fd < 0) {
        file_close(file);
        return fd;
    }

    return fd;
}

static intptr_t sys_close(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ENOSYS;
    }

    return process_fd_release(process, args[0]);
}

static intptr_t sys_read(const uintptr_t* restrict args)
{
    return syscall_with_fd(args, &read_file);
}

static intptr_t sys_write(const uintptr_t* restrict args)
{
    return syscall_with_fd(args, &write_file);
}

static intptr_t sys_seek(const uintptr_t* restrict args)
{
    return syscall_with_fd(args, &seek_file);
}

static intptr_t sys_ftruncate(const uintptr_t* restrict args)
{
    return syscall_with_fd(args, &ftruncate_file);
}

static intptr_t sys_fstat(const uintptr_t* restrict args)
{
    return syscall_with_fd(args, &fstat_file);
}

static intptr_t sys_fstatat(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ENOSYS;
    }

    unsigned flags = args[3];

    if(flags & ~(AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH)) {
        return -EINVAL;
    }

    char path[PATH_SIZE];
    ssize_t copied = user_copyin_str(path, args[1], sizeof(path));

    if(copied < 0) {
        return copied;
    }

    vnode_t* node;
    int retval = syscall_resolve_at(args[0], path, flags, &node);

    if(retval < 0) {
        return retval;
    }

    stat_t st;
    retval = vnode_stat(node, &st);

    vnode_decref(node);

    if(retval < 0) {
        return retval;
    }

    copied = user_copyout(args[2], &st, sizeof(st));

    if(copied < 0) {
        return copied;
    }

    return 0;
}

static intptr_t sys_fchmodat(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ENOSYS;
    }

    unsigned flags = args[3];

    if(flags & ~(AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH)) {
        return -EINVAL;
    }

    char path[PATH_SIZE];
    ssize_t copied = user_copyin_str(path, args[1], sizeof(path));

    if(copied < 0) {
        return copied;
    }

    vnode_t* node;
    int retval = syscall_resolve_at(args[0], path, flags, &node);

    if(retval < 0) {
        return retval;
    }

    retval = vnode_chmod(node, thread->credentials, args[2]);

    vnode_decref(node);

    return retval;
}

static intptr_t sys_fchownat(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ENOSYS;
    }

    unsigned flags = args[4];

    if(flags & ~(AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH)) {
        return -EINVAL;
    }

    char path[PATH_SIZE];
    ssize_t copied = user_copyin_str(path, args[1], sizeof(path));

    if(copied < 0) {
        return copied;
    }

    vnode_t* node;
    int retval = syscall_resolve_at(args[0], path, flags, &node);

    if(retval < 0) {
        return retval;
    }

    retval = vnode_chown(node, thread->credentials, args[2], args[3]);

    vnode_decref(node);

    return retval;
}

static intptr_t sys_dup(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ENOSYS;
    }

    return process_fd_dup(process, args[0], -1);
}

static intptr_t sys_dup2(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ENOSYS;
    }

    int old_fd = args[0];
    int new_fd = args[1];

    if(new_fd < 0) {
        return -EBADF;
    }

    return process_fd_dup(process, old_fd, new_fd);
}

static intptr_t sys_ioctl(const uintptr_t* restrict args)
{
    return syscall_with_fd(args, &ioctl_file);
}

static intptr_t sys_utimensat(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ENOSYS;
    }

    unsigned flags = args[3];

    if(flags & ~(AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH)) {
        return -EINVAL;
    }

    char path[PATH_SIZE];
    ssize_t copied = user_copyin_str(path, args[1], sizeof(path));

    if(copied < 0) {
        return copied;
    }

    timespec_t times[2];
    const timespec_t* times_ptr = nullptr;

    if(args[2]) {
        copied = user_copyin(times, args[2], sizeof(times));

        if(copied < 0) {
            return copied;
        }

        times_ptr = times;
    }

    vnode_t* node;
    int retval = syscall_resolve_at(args[0], path, flags, &node);

    if(retval < 0) {
        return retval;
    }

    retval = vnode_utimes(node, thread->credentials, times_ptr);

    vnode_decref(node);

    return retval;
}

define_syscall(sys_openat, SYS_OPENAT);
define_syscall(sys_close, SYS_CLOSE);
define_syscall(sys_read, SYS_READ);
define_syscall(sys_write, SYS_WRITE);
define_syscall(sys_ftruncate, SYS_FTRUNCATE);
define_syscall(sys_fchmodat, SYS_FCHMODAT);
define_syscall(sys_fchownat, SYS_FCHOWNAT);
define_syscall(sys_seek, SYS_SEEK);
define_syscall(sys_fstat, SYS_FSTAT);
define_syscall(sys_fstatat, SYS_FSTATAT);
define_syscall(sys_dup, SYS_DUP);
define_syscall(sys_dup2, SYS_DUP2);
define_syscall(sys_ioctl, SYS_IOCTL);
define_syscall(sys_utimensat, SYS_UTIMENSAT);
