#include <core/cdefs.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <core/panic.h>
#include <core/version.h>
#include <init/initfn.h>
#include <lib/string.h>

static void call_all_initfns(void)
{
    size_t i;
    int error;

    for(i = 0; &initfns_start[i] < initfns_end; ++i) {
        kprintf(KP_DEBUG, "kmain: calling initfn: %s [%p]", initfns_start[i].name, initfns_start[i].pfn);

        error = initfns_start[i].pfn();

        if(error == 0 || error == ENODEV) {
            continue;
        }

        panic("kmain: initfn: %s: %s", initfns_start[i].name, strerror(error));
        unreachable();
    }
}

void __noreturn kmain(void)
{
    kprintf(KP_INFORM, "%s %s %s", k_sysname, k_release, k_version);

    call_all_initfns();

    panic("kmain: nothing else to do");
    unreachable();
}
