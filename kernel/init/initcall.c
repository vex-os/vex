#include <vex/initcall.h>
#include <vex/kprintf.h>

void invoke_initcalls(const struct initcall* restrict start, const struct initcall* restrict end)
{
    const struct initcall* it;

    for(it = start; it < end; it += 1) {
#ifdef CONFIG_TRACE_INITCALLS
        pr_verbose("initcall: identifier=%s function=%p", it->identifier, it->function);
#endif

        it->function();
    }
}
