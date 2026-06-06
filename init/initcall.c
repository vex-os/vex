#include <core/kprintf.h>
#include <init/initcall.h>

void invoke_initcalls(const struct initcall_entry* restrict start, const struct initcall_entry* restrict end)
{
    const struct initcall_entry* it;

    for(it = start; it < end; it += 1) {
#if INITCALL_LOGGING
        pr_verbose("initcall: calling %s: func=%p", it->name, it->func);
#endif

        it->func();
    }
}
