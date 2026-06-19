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

static long syscall3(long num, long a1, long a2, long a3)
{
    long ret;
    asm volatile("syscall" : "=a"(ret) : "a"(num), "D"(a1), "S"(a2), "d"(a3) : "rcx", "r11", "memory");
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

    // create /test directory
    syscall1(__k_SYS_MKDIR, (long)"/test");

    // open /test/filename.txt for writing, create if not exists
    long fd = syscall2(__k_SYS_OPEN, (long)"/test/filename.txt", 0x0002 | 0x0004); // O_WRONLY | O_CREAT

    if(fd >= 0) {
        const char* msg = "amogus";
        syscall3(__k_SYS_WRITE, fd, (long)msg, ustrlen(msg));
        syscall1(__k_SYS_CLOSE, fd);
    }

    // read it back
    fd = syscall2(__k_SYS_OPEN, (long)"/test/filename.txt", 0x0001); // O_RDONLY

    if(fd >= 0) {
        char buf[16];
        long n = syscall3(__k_SYS_READ, fd, (long)buf, sizeof(buf) - 1);
        if(n > 0) {
            syscall2(__k_SYS_TEST, (long)buf, n);
        }
        syscall1(__k_SYS_CLOSE, fd);
    }

    return 1337;
}
