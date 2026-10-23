#include <vex/errno.h>
#include <vex/fcntl.h>
#include <vex/stat.h>
#include <vex/syscall.h>
#include <vex/time.h>

#include "ktest.h"

#define STAMPED_PATH "stamped.txt"
#define SET_SECONDS  1000000000
// Past 2038 the seconds no longer fit ext2's signed 32-bit field
#define FAR_SECONDS 4000000000

static long stat_stamped(struct __k_stat* st)
{
    return ktest_syscall4(__k_SYS_FSTATAT, __k_AT_FDCWD, (long)(STAMPED_PATH), (long)(st), 0);
}

static void read_stamped(void)
{
    ktest_expect(ktest_file_has_prefix(STAMPED_PATH, "stamped"), 1);
}

int main(void)
{
    struct __k_timespec saved_realtime = {};
    struct __k_timespec saved_monotonic = {};
    struct __k_timespec now = {};
    struct __k_timespec date = {};
    struct __k_stat st = {};

    ktest_enter_work_directory("fs_times");

    ktest_syscall2(__k_SYS_GETTIME, __k_CLOCK_REALTIME, (long)(&saved_realtime));
    ktest_syscall2(__k_SYS_GETTIME, __k_CLOCK_MONOTONIC, (long)(&saved_monotonic));
    ktest_expect(saved_realtime.tv_sec > 0, 1);

    date.tv_sec = SET_SECONDS;
    ktest_expect(ktest_syscall2(__k_SYS_SETTIME, __k_CLOCK_MONOTONIC, (long)(&date)), -__k_EINVAL);
    ktest_expect(ktest_syscall2(__k_SYS_SETTIME, __k_CLOCK_REALTIME, (long)(&date)), 0);
    ktest_syscall2(__k_SYS_GETTIME, __k_CLOCK_REALTIME, (long)(&now));
    ktest_expect(now.tv_sec, SET_SECONDS);

    long fd = ktest_open(STAMPED_PATH, __k_O_WRONLY | __k_O_CREAT, 0644);
    ktest_assert(fd >= 0);
    ktest_write(fd, "stamped\n", 8);
    ktest_close(fd);

    stat_stamped(&st);
    ktest_expect(st.st_mtim.tv_sec, SET_SECONDS); // the clock was just set

    struct __k_timespec times[2] = {};
    times[0].tv_sec = FAR_SECONDS;
    times[0].tv_nsec = 123;
    times[1].tv_sec = FAR_SECONDS;
    times[1].tv_nsec = 456;
    ktest_syscall4(__k_SYS_UTIMENSAT, __k_AT_FDCWD, (long)(STAMPED_PATH), (long)(times), 0);
    stat_stamped(&st);
    ktest_expect(st.st_mtim.tv_sec, FAR_SECONDS);
    ktest_expect(st.st_mtim.tv_nsec, 456);

    // Relatime: the first read moves the atime past the change, later ones leave it
    struct __k_stat before = {};
    struct __k_stat first = {};
    struct __k_stat second = {};

    ktest_syscall4(__k_SYS_UTIMENSAT, __k_AT_FDCWD, (long)(STAMPED_PATH), 0, 0);
    stat_stamped(&before);
    read_stamped();
    stat_stamped(&first);
    read_stamped();
    stat_stamped(&second);
    ktest_expect(first.st_atim.tv_nsec != before.st_atim.tv_nsec, 1);
    ktest_expect(second.st_atim.tv_nsec - first.st_atim.tv_nsec, 0);
    ktest_expect(second.st_ctim.tv_nsec - before.st_ctim.tv_nsec, 0);

    // Puts the wall clock back, with the time that has passed since
    struct __k_timespec later = {};
    ktest_syscall2(__k_SYS_GETTIME, __k_CLOCK_MONOTONIC, (long)(&later));
    saved_realtime.tv_sec += later.tv_sec - saved_monotonic.tv_sec;
    ktest_expect(ktest_syscall2(__k_SYS_SETTIME, __k_CLOCK_REALTIME, (long)(&saved_realtime)), 0);

    return ktest_result();
}
