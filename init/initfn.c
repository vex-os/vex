// SPDX-License-Identifier: BSD-3-Clause

#include <core/errno.h>
#include <core/kprintf.h>
#include <core/panic.h>
#include <init/initfn.h>
#include <lib/string.h>

void call_initfns(const struct initfn_entry* restrict start, const struct initfn_entry* restrict end)
{
    const struct initfn_entry* it;
    int retval;

    for(it = start; it < end; ++it) {
        kprintf(KP_DEBUG, "init: calling initfn: %s <%p>", it->name, it->pfn);

        retval = it->pfn();

        if(retval == 0 || retval == ENODEV) {
            continue;
        }

        panic("init: initfn: %s: %s", it->name, strerror(retval));
        unreachable();
    }
}
