#include <vex/errno.h>
#include <vex/syscall.h>

static intptr_t sys_mount(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

static intptr_t sys_umount(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

static intptr_t sys_sync(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

static intptr_t sys_statvfs(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

define_syscall(sys_mount, SYS_MOUNT);
define_syscall(sys_umount, SYS_UMOUNT);
define_syscall(sys_sync, SYS_SYNC);
define_syscall(sys_statvfs, SYS_STATVFS);
