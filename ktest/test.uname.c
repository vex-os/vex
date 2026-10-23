#include <vex/utsname.h>

#include "ktest.h"

int main(int argc, char** argv)
{
    struct __k_utsname name = {};

    long retval = ktest_syscall1(__k_SYS_UNAME, (long)(&name));

    if(retval < 0) {
        ktest_fail("uname failed: %ld", retval);
    }

    ktest_printf("%s %s %s %s\n", name.sysname, name.release, name.version, name.machine);
    ktest_assert(ktest_string_equal(name.sysname, "Vex"));

    return 0;
}
