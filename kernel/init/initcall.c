#include <vex/initcall.h>
#include <vex/kprintf.h>

void invoke_initcalls(const initcall_t* restrict start, const initcall_t* restrict end)
{
    const initcall_t* it;

    for(it = start; it < end; it += 1) {
#ifdef CONFIG_TRACE_INITCALLS
        pr_verbose("initcall: identifier=%s function=%p", it->name, it->function);
#endif

        it->function();
    }
}
