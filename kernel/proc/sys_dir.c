#include <vex/atomic.h>
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

static int resolve_rename_parent(int dirfd, const char* restrict path, const credentials_t* restrict credentials,
    vnode_t** restrict out_parent, const char** restrict out_name)
{
    vnode_t* base_dir;
    int retval = syscall_dirfd(dirfd, path, &base_dir);

    if(retval < 0) {
        return retval;
    }

    retval = path_resolve_parent_at(base_dir, path, credentials, out_parent, out_name);

    if(base_dir) {
        vnode_decref(base_dir);
    }

    if(retval < 0) {
        return retval;
    }

    retval = vnode_may_delete(*out_parent, *out_name, credentials);

    if(retval < 0) {
        vnode_decref(*out_parent);
        return retval;
    }

    return 0;
}

static int resolve_new_parent(int dirfd, const char* restrict path, const credentials_t* restrict credentials,
    vnode_t** restrict out_parent, const char** restrict out_name)
{
    vnode_t* base_dir;
    int retval = syscall_dirfd(dirfd, path, &base_dir);

    if(retval < 0) {
        return retval;
    }

    retval = path_resolve_parent_at(base_dir, path, credentials, out_parent, out_name);

    if(base_dir) {
        vnode_decref(base_dir);
    }

    if(retval < 0) {
        return retval;
    }

    retval = vnode_access(*out_parent, credentials, W_OK | X_OK);

    if(retval < 0) {
        vnode_decref(*out_parent);
        return retval;
    }

    return 0;
}

static intptr_t readdir_file(file_t* restrict file, const uintptr_t* restrict args)
{
    dirent_t entry;
    int retval = vnode_iterate(file->node, &entry, file->offset);

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

    file->offset += 1;

    return 1;
}

static intptr_t sys_unlinkat(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ENOSYS;
    }

    unsigned flags = args[2];

    if(flags & ~AT_REMOVEDIR) {
        return -EINVAL;
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

    const char* name;
    vnode_t* parent;
    retval = path_resolve_parent_at(base_dir, path, credentials, &parent, &name);

    if(base_dir) {
        vnode_decref(base_dir);
    }

    if(retval < 0) {
        return retval;
    }

    retval = vnode_may_delete(parent, name, credentials);

    if(retval == 0) {
        retval = vnode_unlink(parent, name, flags & AT_REMOVEDIR);
    }

    vnode_decref(parent);

    return retval;
}

static intptr_t sys_mkdirat(const uintptr_t* restrict args)
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

    const char* name;
    vnode_t* parent;
    retval = path_resolve_parent_at(base_dir, path, credentials, &parent, &name);

    if(base_dir) {
        vnode_decref(base_dir);
    }

    if(retval < 0) {
        return retval;
    }

    vnode_t* created;
    retval = vnode_access(parent, credentials, W_OK | X_OK);

    if(retval == 0) {
        retval = vnode_create(parent, name, VNODE_DIRECTORY, args[2] & ~umask, credentials, &created);
    }

    vnode_decref(parent);

    if(retval < 0) {
        return retval;
    }

    vnode_decref(created);

    return 0;
}

static intptr_t sys_renameat(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ENOSYS;
    }

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

    const credentials_t* credentials = thread->credentials;

    const char* oldname;
    vnode_t* olddir;
    int retval = resolve_rename_parent(args[0], oldpath, credentials, &olddir, &oldname);

    if(retval < 0) {
        return retval;
    }

    const char* newname;
    vnode_t* newdir;
    retval = resolve_rename_parent(args[2], newpath, credentials, &newdir, &newname);

    if(retval < 0) {
        vnode_decref(olddir);
        return retval;
    }

    retval = vnode_rename(olddir, oldname, newdir, newname);

    vnode_decref(newdir);
    vnode_decref(olddir);

    return retval;
}

static intptr_t sys_linkat(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ENOSYS;
    }

    unsigned flags = args[4];

    if(flags & ~(AT_SYMLINK_FOLLOW | AT_EMPTY_PATH)) {
        return -EINVAL;
    }

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

    // Unlike most calls, a symlink is
    // linked to itself unless asked otherwise
    unsigned resolve_flags = flags & AT_EMPTY_PATH;

    if(0 == (flags & AT_SYMLINK_FOLLOW)) {
        resolve_flags |= AT_SYMLINK_NOFOLLOW;
    }

    vnode_t* target;
    int retval = syscall_resolve_at(args[0], oldpath, resolve_flags, &target);

    if(retval < 0) {
        return retval;
    }

    const char* name;
    vnode_t* parent;
    retval = resolve_new_parent(args[2], newpath, thread->credentials, &parent, &name);

    if(retval == 0) {
        retval = vnode_link(parent, name, target);
        vnode_decref(parent);
    }

    vnode_decref(target);

    return retval;
}

static intptr_t sys_symlinkat(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ENOSYS;
    }

    char target[PATH_SIZE];
    ssize_t copied = user_copyin_str(target, args[0], sizeof(target));

    if(copied < 0) {
        return copied;
    }

    char path[PATH_SIZE];
    copied = user_copyin_str(path, args[2], sizeof(path));

    if(copied < 0) {
        return copied;
    }

    const char* name;
    vnode_t* parent;
    int retval = resolve_new_parent(args[1], path, thread->credentials, &parent, &name);

    if(retval < 0) {
        return retval;
    }

    vnode_t* created;
    retval = vnode_symlink(parent, name, target, thread->credentials, &created);

    vnode_decref(parent);

    if(retval < 0) {
        return retval;
    }

    vnode_decref(created);

    return 0;
}

static intptr_t sys_readlinkat(const uintptr_t* restrict args)
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

    vnode_t* node;
    int retval = syscall_resolve_at(args[0], path, AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH, &node);

    if(retval < 0) {
        return retval;
    }

    char target[PATH_SIZE];
    size_t size = args[3];
    ssize_t length = -EINVAL;

    if(size > sizeof(target)) {
        size = sizeof(target);
    }

    if(node->type == VNODE_SYM_LINK) {
        length = vnode_readlink(node, target, size);
    }

    vnode_decref(node);

    if(length < 0) {
        return length;
    }

    copied = user_copyout(args[2], target, length);

    if(copied < 0) {
        return copied;
    }

    return length;
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
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ENOSYS;
    }

    char path[PATH_SIZE];
    ssize_t copied = user_copyin_str(path, args[0], sizeof(path));

    if(copied < 0) {
        return copied;
    }

    vnode_t* base_dir;
    int retval = syscall_dirfd(AT_FDCWD, path, &base_dir);

    if(retval < 0) {
        return retval;
    }

    const credentials_t* credentials = thread->credentials;

    vnode_t* node;
    retval = path_resolve_at(base_dir, path, 0, credentials, &node);

    if(base_dir) {
        vnode_decref(base_dir);
    }

    if(retval < 0) {
        return retval;
    }

    if(node->type != VNODE_DIRECTORY) {
        vnode_decref(node);
        return -ENOTDIR;
    }

    retval = vnode_access(node, credentials, X_OK);

    if(retval < 0) {
        vnode_decref(node);
        return retval;
    }

    process_set_cwd(process, node);

    return 0;
}

static intptr_t sys_fchdir(const uintptr_t* restrict args)
{
    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -ENOSYS;
    }

    int fd = args[0];

    if(fd == AT_FDCWD) {
        return -EBADF;
    }

    // An empty path is never absolute, so this
    // always hands back the directory behind fd
    vnode_t* node;
    int retval = syscall_dirfd(fd, "", &node);

    if(retval < 0) {
        return retval;
    }

    const credentials_t* credentials = thread->credentials;
    retval = vnode_access(node, credentials, X_OK);

    if(retval < 0) {
        vnode_decref(node);
        return retval;
    }

    process_set_cwd(process, node);

    return 0;
}

define_syscall(sys_unlinkat, SYS_UNLINKAT);
define_syscall(sys_mkdirat, SYS_MKDIRAT);
define_syscall(sys_renameat, SYS_RENAMEAT);
define_syscall(sys_readdir, SYS_READDIR);
define_syscall(sys_getcwd, SYS_GETCWD);
define_syscall(sys_chdir, SYS_CHDIR);
define_syscall(sys_fchdir, SYS_FCHDIR);
define_syscall(sys_linkat, SYS_LINKAT);
define_syscall(sys_symlinkat, SYS_SYMLINKAT);
define_syscall(sys_readlinkat, SYS_READLINKAT);
