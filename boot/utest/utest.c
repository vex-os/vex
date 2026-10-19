#include <vex/fcntl.h>
#include <vex/syscall.h>
#include <vex/time.h>
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

static long syscall4(long num, long a1, long a2, long a3, long a4)
{
    long ret;
    register long r10 asm("r10") = a4;
    asm volatile("syscall" : "=a"(ret) : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(r10) : "rcx", "r11", "memory");
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

static void readback(const char* path)
{
    long fd = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)path, __k_O_RDONLY, 0);

    if(fd < 0) {
        printnumber(fd);
        return;
    }

    char buf[16];
    long n = syscall3(__k_SYS_READ, fd, (long)buf, sizeof(buf) - 1);

    if(n > 0) {
        syscall2(__k_SYS_TEST, (long)buf, n);
    }

    syscall1(__k_SYS_CLOSE, fd);
}

int main(void)
{
    struct __k_utsname name;
    syscall1(__k_SYS_UNAME, (long)&name);

    syscall2(__k_SYS_TEST, (long)"Hello, world!", 13);

    long number = 1234567890;
    printnumber(number);

    number = syscall3(__k_SYS_MKDIRAT, __k_AT_FDCWD, (long)"/test/subdir", 0755); // should fail because /test doesn't exist yet

    printnumber(number); // should print a negative error code (likely -ENOENT which is -243)

    // create /test directory
    syscall3(__k_SYS_MKDIRAT, __k_AT_FDCWD, (long)"/test", 0755);

    // open /test/filename.txt for writing, create if not exists
    long fd = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"/test/filename.txt", __k_O_WRONLY | __k_O_CREAT, 0644);

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

    // read it back through a path relative to the working directory
    syscall1(__k_SYS_CHDIR, (long)"/test");
    fd = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"filename.txt", __k_O_RDONLY, 0);

    if(fd >= 0) {
        char buf[16];
        long n = syscall3(__k_SYS_READ, fd, (long)buf, sizeof(buf) - 1);

        if(n > 0) {
            syscall2(__k_SYS_TEST, (long)buf, n);
        }

        syscall1(__k_SYS_CLOSE, fd);
    }

    // unlinkat refuses a directory without AT_REMOVEDIR
    syscall3(__k_SYS_MKDIRAT, __k_AT_FDCWD, (long)"subdir", 0755);
    printnumber(syscall3(__k_SYS_UNLINKAT, __k_AT_FDCWD, (long)"subdir", 0));                // should print -229 (EISDIR)
    printnumber(syscall3(__k_SYS_UNLINKAT, __k_AT_FDCWD, (long)"subdir", __k_AT_REMOVEDIR)); // should print 0

    // renameat moves a file into another directory
    syscall3(__k_SYS_MKDIRAT, __k_AT_FDCWD, (long)"moved", 0755);
    printnumber(syscall4(__k_SYS_RENAMEAT, __k_AT_FDCWD, (long)"filename.txt", __k_AT_FDCWD, (long)"moved/renamed.txt")); // should print 0
    printnumber(syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"filename.txt", __k_O_RDONLY, 0)); // should print -243 (ENOENT)
    readback("moved/renamed.txt");                                                              // should print the uname dump

    // renameat replaces an existing file, and refuses to move a directory into itself
    fd = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"other.txt", __k_O_WRONLY | __k_O_CREAT, 0644);
    syscall3(__k_SYS_WRITE, fd, (long)"replaced\n", 9);
    syscall1(__k_SYS_CLOSE, fd);
    printnumber(syscall4(__k_SYS_RENAMEAT, __k_AT_FDCWD, (long)"other.txt", __k_AT_FDCWD, (long)"moved/renamed.txt")); // should print 0
    readback("moved/renamed.txt");                                                                           // should print "replaced"
    printnumber(syscall4(__k_SYS_RENAMEAT, __k_AT_FDCWD, (long)"moved", __k_AT_FDCWD, (long)"moved/inner")); // should print -226 (EINVAL)

    // A moved directory's ".." follows it to its new parent
    printnumber(syscall4(__k_SYS_RENAMEAT, __k_AT_FDCWD, (long)"moved", __k_AT_FDCWD, (long)"/moved"));            // should print 0
    printnumber(syscall4(__k_SYS_RENAMEAT, __k_AT_FDCWD, (long)"/moved/../moved", __k_AT_FDCWD, (long)"/moved2")); // should print 0
    readback("/moved2/renamed.txt"); // should print "replaced"

    // Long names spill the directory past its first cluster
    static const char* long_names[] = {
        "/moved2/a-rather-long-file-name-1.txt",
        "/moved2/a-rather-long-file-name-2.txt",
        "/moved2/a-rather-long-file-name-3.txt",
        "/moved2/a-rather-long-file-name-4.txt",
        "/moved2/a-rather-long-file-name-5.txt",
        "/moved2/a-rather-long-file-name-6.txt",
    };

    for(int i = 0; i < 6; ++i) {
        fd = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)long_names[i], __k_O_WRONLY | __k_O_CREAT, 0644);
        syscall3(__k_SYS_WRITE, fd, (long)long_names[i], ustrlen(long_names[i]));
        syscall1(__k_SYS_CLOSE, fd);
    }

    readback(long_names[5]); // should print "/moved2/a-rath"
    printnumber(syscall4(__k_SYS_RENAMEAT, __k_AT_FDCWD, (long)long_names[5], __k_AT_FDCWD, (long)"/moved2/short.txt")); // should print 0
    readback("/moved2/short.txt");                                                       // should print "/moved2/a-rath"
    printnumber(syscall3(__k_SYS_UNLINKAT, __k_AT_FDCWD, (long)"/moved2/short.txt", 0)); // should print 0
    printnumber(syscall3(__k_SYS_UNLINKAT, __k_AT_FDCWD, (long)long_names[4], 0));       // should print 0

    struct __k_timespec now;
    syscall2(__k_SYS_GETTIME, __k_CLOCK_REALTIME, (long)&now);
    printnumber(now.tv_sec); // should print the date at boot, seconds since the epoch
    syscall2(__k_SYS_GETTIME, __k_CLOCK_MONOTONIC, (long)&now);
    printnumber(now.tv_sec); // should print a few seconds of uptime

    struct __k_timespec date = { .tv_sec = 2000000000, .tv_nsec = 0 };
    printnumber(syscall2(__k_SYS_SETTIME, __k_CLOCK_MONOTONIC, (long)&date)); // should print -226 (EINVAL)
    printnumber(syscall2(__k_SYS_SETTIME, __k_CLOCK_REALTIME, (long)&date));  // should print 0
    syscall2(__k_SYS_GETTIME, __k_CLOCK_REALTIME, (long)&now);
    printnumber(now.tv_sec); // should print 2000000000

    return 1337;
}
