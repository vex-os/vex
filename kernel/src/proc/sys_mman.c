#include <vex/errno.h>
#include <vex/syscall.h>

static intptr_t sys_mmap(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

static intptr_t sys_munmap(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

static intptr_t sys_mprotect(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

define_syscall(sys_mmap, SYS_MMAP);
define_syscall(sys_munmap, SYS_MUNMAP);
define_syscall(sys_mprotect, SYS_MPROTECT);
