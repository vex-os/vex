#include <vex/fcntl.h>
#include <vex/syscall.h>
#include <vex/utsname.h>

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

static void printnumber(long number)
{
    int i, sign;
    char buffer[128];

    for(i = 0; i < sizeof(buffer); buffer[i++] = 0) {
        /* empty */
    }

    if(number == 0) {
        syscall2(__k_SYS_TEST, (long)"0", 1);
        return;
    }

    if(number < 0) {
        number = -number;
        sign = 1;
    }
    else {
        sign = 0;
    }

    i = 0;

    while(number > 0) {
        buffer[i++] = '0' + (number % 10);
        number /= 10;
    }

    if(sign) {
        buffer[i++] = '-';
    }

    // reverse the string
    for(int j = 0; j < i / 2; j++) {
        char tmp = buffer[j];
        buffer[j] = buffer[i - j - 1];
        buffer[i - j - 1] = tmp;
    }

    syscall2(__k_SYS_TEST, (long)buffer, i);
}

int main(void)
{
    struct __k_utsname name;
    syscall1(__k_SYS_UNAME, (long)&name);

    syscall2(__k_SYS_TEST, (long)"Hello, world!", 13);

    long number = 1234567890;
    printnumber(number);

    number = syscall1(__k_SYS_MKDIR, (long)"/test/subdir"); // should fail because /test doesn't exist yet

    printnumber(number); // should print a negative error code (likely -ENOENT which is -243)

    // create /test directory
    syscall1(__k_SYS_MKDIR, (long)"/test");

    // open /test/filename.txt for writing, create if not exists
    long fd = syscall2(__k_SYS_OPEN, (long)"/test/filename.txt", __k_O_WRONLY | __k_O_CREAT);

    if(fd >= 0) {
        static const char* newline = "\n\n";
        static const char* msg = "sus amogus\n";

        syscall3(__k_SYS_WRITE, fd, (long)name.sysname, ustrlen(name.sysname));
        syscall3(__k_SYS_WRITE, fd, (long)newline, 2);

        syscall3(__k_SYS_WRITE, fd, (long)name.nodename, ustrlen(name.nodename));
        syscall3(__k_SYS_WRITE, fd, (long)newline, 2);

        syscall3(__k_SYS_WRITE, fd, (long)name.release, ustrlen(name.release));
        syscall3(__k_SYS_WRITE, fd, (long)newline, 2);

        syscall3(__k_SYS_WRITE, fd, (long)name.version, ustrlen(name.version));
        syscall3(__k_SYS_WRITE, fd, (long)newline, 2);

        syscall3(__k_SYS_WRITE, fd, (long)name.machine, ustrlen(name.machine));
        syscall3(__k_SYS_WRITE, fd, (long)newline, 2);

        syscall3(__k_SYS_WRITE, fd, (long)msg, ustrlen(msg));

        syscall1(__k_SYS_CLOSE, fd);
    }

    // read it back
    fd = syscall2(__k_SYS_OPEN, (long)"/test/filename.txt", __k_O_RDONLY);

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
