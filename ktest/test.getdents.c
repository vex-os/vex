#include <vex/dirent.h>
#include <vex/errno.h>
#include <vex/fcntl.h>
#include <vex/syscall.h>

#include "ktest.h"

#define FILE_COUNT            5
#define RECORD_ALIGNMENT      8
#define LARGE_BUFFER_SIZE     1024
#define SMALL_BUFFER_SIZE     48
#define TOO_SMALL_BUFFER_SIZE 8

static long make_directory(const char* path)
{
    return ktest_syscall3(__k_SYS_MKDIRAT, __k_AT_FDCWD, (long)(path), 0755);
}

static long read_entries(long fd, void* buffer, size_t size)
{
    return ktest_syscall3(__k_SYS_GETDENTS, fd, (long)(buffer), (long)(size));
}

// Reads the whole directory with buffers of the given size and returns the count of the entries other than "." and ".."
static long count_entries(const char* path, size_t buffer_size)
{
    _Alignas(8) char records[LARGE_BUFFER_SIZE];
    long fd = ktest_open(path, __k_O_RDONLY, 0);
    long count = 0;

    ktest_assert(fd >= 0);

    for(;;) {
        long filled = read_entries(fd, records, buffer_size);

        ktest_assert(filled >= 0);

        if(filled == 0) {
            break;
        }

        long position = 0;

        while(position < filled) {
            const struct __k_dirent* entry = (const struct __k_dirent*)(records + position);
            const char* name = (const char*)(entry->d_name);

            ktest_expect(entry->d_reclen % RECORD_ALIGNMENT, 0);
            ktest_assert(entry->d_reclen > 0);

            if(!ktest_string_equal(name, ".") && !ktest_string_equal(name, "..")) {
                count += 1;
            }

            position += entry->d_reclen;
        }

        ktest_expect(position, filled);
    }

    ktest_close(fd);
    return count;
}

static long create_files(void)
{
    char name[16];

    for(int i = 0; i < FILE_COUNT; ++i) {
        ktest_snprintf(name, sizeof(name), "dir/file-%d", i);

        long fd = ktest_open(name, __k_O_WRONLY | __k_O_CREAT, 0644);

        if(fd < 0) {
            return fd;
        }

        ktest_close(fd);
    }

    return 0;
}

int main(void)
{
    ktest_enter_work_directory("getdents");

    ktest_expect(make_directory("dir"), 0);
    ktest_expect(create_files(), 0);

    ktest_expect(count_entries("dir", LARGE_BUFFER_SIZE), FILE_COUNT);

    // A buffer that holds a record or two makes the call return in several parts
    ktest_expect(count_entries("dir", SMALL_BUFFER_SIZE), FILE_COUNT);

    // The first record doesn't fit
    _Alignas(8) char records[LARGE_BUFFER_SIZE];
    long fd = ktest_open("dir", __k_O_RDONLY, 0);

    ktest_assert(fd >= 0);
    ktest_expect(read_entries(fd, records, TOO_SMALL_BUFFER_SIZE), -__k_EINVAL);

    // The failed call doesn't move the position
    ktest_assert(read_entries(fd, records, sizeof(records)) > 0);
    ktest_close(fd);

    return ktest_result();
}
