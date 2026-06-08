#include <sys/initcall.h>
#include <sys/kprintf.h>

void invoke_initcalls(const struct initcall* restrict start, const struct initcall* restrict end)
{
    const struct initcall* it;

    for(it = start; it < end; ++it) {
#if INITCALL_LOGGING
        pr_verbose("initcall: identifier=%s function=%p", it->identifier, it->function);
#endif

        it->function();
    }
}
