#include <vex/errno.h>
#include <vex/fcntl.h>
#include <vex/syscall.h>
#include <vex/utsname.h>

#include "ktest.h"

#define LONG_NAME_COUNT 6

static const char* const long_names[LONG_NAME_COUNT] = {
    "moved2/a-rather-long-file-name-1.txt",
    "moved2/a-rather-long-file-name-2.txt",
    "moved2/a-rather-long-file-name-3.txt",
    "moved2/a-rather-long-file-name-4.txt",
    "moved2/a-rather-long-file-name-5.txt",
    "moved2/a-rather-long-file-name-6.txt",
};

static long make_directory(const char* path)
{
    return ktest_syscall3(__k_SYS_MKDIRAT, __k_AT_FDCWD, (long)(path), 0755);
}

static long rename_path(const char* from, const char* to)
{
    return ktest_syscall4(__k_SYS_RENAMEAT, __k_AT_FDCWD, (long)(from), __k_AT_FDCWD, (long)(to));
}

static long unlink_path(const char* path, long flags)
{
    return ktest_syscall3(__k_SYS_UNLINKAT, __k_AT_FDCWD, (long)(path), flags);
}

static void write_file(const char* path, const char* text)
{
    long fd = ktest_open(path, __k_O_WRONLY | __k_O_CREAT, 0644);

    ktest_assert(fd >= 0);
    ktest_write(fd, text, ktest_strlen(text));
    ktest_close(fd);
}

int main(void)
{
    ktest_enter_work_directory("fs");

    // The directory of the file doesn't exist
    ktest_expect(make_directory("missing/subdir"), -__k_ENOENT);

    struct __k_utsname name = {};
    ktest_syscall1(__k_SYS_UNAME, (long)(&name));

    long fd = ktest_open("filename.txt", __k_O_WRONLY | __k_O_CREAT, 0644);
    ktest_assert(fd >= 0);
    ktest_write(fd, name.sysname, ktest_strlen(name.sysname));
    ktest_write(fd, "\n\n", 2);
    ktest_write(fd, name.release, ktest_strlen(name.release));
    ktest_close(fd);
    ktest_expect(ktest_file_has_prefix("filename.txt", name.sysname), 1);

    // F_SETFL changes O_APPEND and O_NONBLOCK only, the access mode stays
    fd = ktest_open("filename.txt", __k_O_RDONLY, 0);
    ktest_assert(fd >= 0);
    ktest_expect(ktest_syscall3(__k_SYS_FCNTL, fd, __k_F_GETFL, 0) == __k_O_RDONLY, 1);
    ktest_expect(ktest_syscall3(__k_SYS_FCNTL, fd, __k_F_SETFL, __k_O_NONBLOCK | __k_O_WRONLY), 0);
    ktest_expect(ktest_syscall3(__k_SYS_FCNTL, fd, __k_F_GETFL, 0) == (__k_O_RDONLY | __k_O_NONBLOCK), 1);
    ktest_expect(ktest_syscall3(__k_SYS_FCNTL, fd, 99, 0), -__k_EINVAL);
    ktest_close(fd);

    // unlinkat refuses a directory without AT_REMOVEDIR
    make_directory("subdir");
    ktest_expect(unlink_path("subdir", 0), -__k_EISDIR);
    ktest_expect(unlink_path("subdir", __k_AT_REMOVEDIR), 0);

    // renameat moves a file into another directory
    make_directory("moved");
    ktest_expect(rename_path("filename.txt", "moved/renamed.txt"), 0);
    ktest_expect(ktest_open("filename.txt", __k_O_RDONLY, 0), -__k_ENOENT);
    ktest_expect(ktest_file_has_prefix("moved/renamed.txt", name.sysname), 1);

    // renameat replaces an existing file, and refuses to move a directory into itself
    write_file("other.txt", "replaced\n");
    ktest_expect(rename_path("other.txt", "moved/renamed.txt"), 0);
    ktest_expect(ktest_file_has_prefix("moved/renamed.txt", "replaced"), 1);
    ktest_expect(rename_path("moved", "moved/inner"), -__k_EINVAL);

    // A moved directory's ".." follows it to its new parent
    make_directory("parent");
    ktest_expect(rename_path("moved", "parent/moved"), 0);
    ktest_expect(rename_path("parent/moved/../moved", "moved2"), 0);
    ktest_expect(ktest_file_has_prefix("moved2/renamed.txt", "replaced"), 1);

    // Long names spill the directory past its first cluster
    for(int i = 0; i < LONG_NAME_COUNT; ++i) {
        write_file(long_names[i], long_names[i]);
    }

    ktest_expect(ktest_file_has_prefix(long_names[5], "moved2/a-rather"), 1);
    ktest_expect(rename_path(long_names[5], "moved2/short.txt"), 0);
    ktest_expect(ktest_file_has_prefix("moved2/short.txt", "moved2/a-rather"), 1);
    ktest_expect(unlink_path("moved2/short.txt", 0), 0);
    ktest_expect(unlink_path(long_names[4], 0), 0);

    return ktest_result();
}
