#include <vex/errno.h>
#include <vex/syscall.h>

static intptr_t sys_gettime(const uintptr_t* restrict args)
{
    return -ENOSYS;
}

define_syscall(sys_gettime, SYS_GETTIME);
