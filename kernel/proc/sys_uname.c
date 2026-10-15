#include <vex/strings.h>
#include <vex/syscall.h>
#include <vex/uaccess.h>
#include <vex/utsname.h>
#include <vex/version.h>

static intptr_t sys_uname(const uintptr_t* restrict args)
{
    struct k_utsname uname;
    kstrncpy(uname.sysname, k_sysname, sizeof(uname.sysname));
    kstrncpy(uname.release, k_release, sizeof(uname.release));
    kstrncpy(uname.version, k_version, sizeof(uname.version));
    kstrncpy(uname.machine, k_machine, sizeof(uname.machine));
    kstrncpy(uname.nodename, "IMPLEMENT_ME", sizeof(uname.nodename));

    ssize_t copied = user_copyout(args[0], &uname, sizeof(uname));

    if(copied < 0) {
        return copied;
    }

    return 0;
}

define_syscall(sys_uname, SYS_UNAME);
