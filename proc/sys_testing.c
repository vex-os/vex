#include <vex/kprintf.h>
#include <vex/syscall.h>
#include <vex/uaccess.h>

static intptr_t sys_test(const uintptr_t* restrict args)
{
    uintptr_t ptr = args[0];
    char buffer[256];

    ssize_t copied = user_copyin_str(buffer, ptr, sizeof(buffer));

    if(copied < 0) {
        return copied;
    }

    pr_inform("sys_test: ptr=%#lx sz=%lu data=\"%s\"", (unsigned long)(ptr), (unsigned long)(copied), buffer);

    return 0;
}

define_syscall(sys_test, SYS_TEST);
