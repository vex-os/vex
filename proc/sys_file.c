#include <vex/errno.h>
#include <vex/syscall.h>

static intptr_t sys_open(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

static intptr_t sys_close(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

static intptr_t sys_read(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

static intptr_t sys_write(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

static intptr_t sys_seek(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

static intptr_t sys_stat(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

static intptr_t sys_unlink(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

static intptr_t sys_mkdir(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

static intptr_t sys_rmdir(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

static intptr_t sys_readdir(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

static intptr_t sys_getcwd(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

static intptr_t sys_chdir(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

static intptr_t sys_dup(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

static intptr_t sys_dup2(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

static intptr_t sys_ioctl(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

define_syscall(sys_open, SYS_OPEN);
define_syscall(sys_close, SYS_CLOSE);
define_syscall(sys_read, SYS_READ);
define_syscall(sys_write, SYS_WRITE);
define_syscall(sys_seek, SYS_SEEK);
define_syscall(sys_stat, SYS_STAT);
define_syscall(sys_unlink, SYS_UNLINK);
define_syscall(sys_mkdir, SYS_MKDIR);
define_syscall(sys_rmdir, SYS_RMDIR);
define_syscall(sys_readdir, SYS_READDIR);
define_syscall(sys_getcwd, SYS_GETCWD);
define_syscall(sys_chdir, SYS_CHDIR);
define_syscall(sys_dup, SYS_DUP);
define_syscall(sys_dup2, SYS_DUP2);
define_syscall(sys_ioctl, SYS_IOCTL);
