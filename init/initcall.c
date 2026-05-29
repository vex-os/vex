#include <core/kprintf.h>
#include <init/initcall.h>

void invoke_initcalls(const struct initcall_entry* restrict start, const struct initcall_entry* restrict end)
{
    const struct initcall_entry* it;

    for(it = start; it < end; it += 1) {
#ifdef TRACE_INITCALLS
        pr_debug("init: invoking %s", it->name);
#endif

        it->func();
    }
}
