#include <vex/atomic.h>
#include <vex/wallclock.h>

// TODO: a stub until there's a real wall clock (audit item A11). Every call moves it
// a second forward, so times keep their order and are never zero, which e2fsck rejects
static time_t now = 0;

time_t wallclock_now(void)
{
    return atomic_fetch_add(&now, 1, ATOMIC_RELAXED) + 1;
}
