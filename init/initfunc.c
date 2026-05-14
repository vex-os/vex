#include <core/errno.h>
#include <core/kprintf.h>
#include <core/panic.h>
#include <init/initfunc.h>
#include <lib/string.h>

void do_initfuncs(const struct initfunc_metadata* start, const struct initfunc_metadata* end)
{
    size_t i;
    int result;

    for(i = 0; &start[i] < end; ++i) {
        kprintf(KP_DEBUG, "initfunc: calling %s <%p> lvl=%u", start[i].identity, start[i].function_ptr, start[i].level);

        result = start[i].function_ptr();

        if(result) {
            if(result == ENODEV) {
                kprintf(KP_DEBUG, "initfunc: %s returned ENODEV", start[i].identity);
                continue;
            }

            panic("initfunc: %s: %s", start[i].identity, strerror(result));
        }
    }
}
