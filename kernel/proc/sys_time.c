#include <vex/capability.h>
#include <vex/clocksrc.h>
#include <vex/cred.h>
#include <vex/errno.h>
#include <vex/process.h>
#include <vex/syscall.h>
#include <vex/thread.h>
#include <vex/time.h>
#include <vex/uaccess.h>
#include <vex/wallclock.h>

static intptr_t sys_gettime(const uintptr_t* restrict args)
{
    struct k_timespec time;

    switch(args[0]) {
        case CLOCK_REALTIME:
            wallclock_get(&time);
            break;

        case CLOCK_MONOTONIC:
            timespec_from_nsec(&time, clocksrc_nsec());
            break;

        default:
            return -EINVAL;
    }

    ssize_t copied = user_copyout(args[1], &time, sizeof(time));

    if(copied < 0) {
        return copied;
    }

    return 0;
}

static intptr_t sys_settime(const uintptr_t* restrict args)
{
    struct thread* thrd = this_thread();

    if(thrd->proc == nullptr) {
        return -ENOSYS;
    }

    if(args[0] != CLOCK_REALTIME) {
        return -EINVAL;
    }

    struct k_timespec time;
    ssize_t copied = user_copyin(&time, args[1], sizeof(time));

    if(copied < 0) {
        return copied;
    }

    if(!cred_capable(thrd->cred, CAP_SYS_TIME)) {
        return -EPERM;
    }

    return wallclock_set(&time);
}

define_syscall(sys_gettime, SYS_GETTIME);
define_syscall(sys_settime, SYS_SETTIME);
