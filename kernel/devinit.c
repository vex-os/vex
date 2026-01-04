#include <vex/devinit.h>

#if CONF_DEV_FBCONS_ENABLE
#include <vex/fbcons.h>
#endif

void init_device_drivers(void)
{
#if CONF_DEV_FBCONS_ENABLE
    init_fbcons();
#endif

    // TODO: add calls to device driver initialization functions here
}
