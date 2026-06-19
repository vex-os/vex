#include <usr/vex/syscall.h>
#include <usr/vex/utsname.h>

static long syscall1(long num, long a1)
{
    long ret;
    asm volatile("syscall" : "=a"(ret) : "a"(num), "D"(a1) : "rcx", "r11", "memory");
    return ret;
}

static long syscall2(long num, long a1, long a2)
{
    long ret;
    asm volatile("syscall" : "=a"(ret) : "a"(num), "D"(a1), "S"(a2) : "rcx", "r11", "memory");
    return ret;
}

static unsigned long ustrlen(const char* s)
{
    unsigned long n = 0;
    while(*s++)
        n++;
    return n;
}

int main(void)
{
    struct __k_utsname name;
    syscall1(__k_SYS_UNAME, (long)&name);

    syscall2(__k_SYS_TEST, (long)name.sysname, ustrlen(name.sysname));
    syscall2(__k_SYS_TEST, (long)name.nodename, ustrlen(name.nodename));
    syscall2(__k_SYS_TEST, (long)name.release, ustrlen(name.release));
    syscall2(__k_SYS_TEST, (long)name.version, ustrlen(name.version));
    syscall2(__k_SYS_TEST, (long)name.machine, ustrlen(name.machine));
    syscall2(__k_SYS_TEST, (long)"Hello, world!", 13);

    return 1337;
}
