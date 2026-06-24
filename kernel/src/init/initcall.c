#include <vex/initcall.h>
#include <vex/kprintf.h>

void invoke_initcalls(const struct initcall* restrict start, const struct initcall* restrict end)
{
    const struct initcall* it;

    for(it = start; it < end; ++it) {
#if TRACE_INITCALLS
        pr_verbose("initcall: identifier=%s function=%p", it->identifier, it->function);
#endif

        it->function();
    }
}
