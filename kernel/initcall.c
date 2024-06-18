/* SPDX-License-Identifier: Zlib */
#include <iserix/initcall.h>

static void init_core(void)
{

}
INITCALL(core, init_core);

static void init_subsys(void)
{

}
INITCALL(subsys, init_subsys);
INITCALL_DEPENDENCY(subsys, core);

static void init_filesystem(void)
{

}
INITCALL(filesystem, init_filesystem);
INITCALL_DEPENDENCY(filesystem, subsys);

static void init_device(void)
{

}
INITCALL(device, init_device);
INITCALL_DEPENDENCY(device, filesystem);

static void init_late(void)
{

}
INITCALL(late, init_late);
INITCALL_DEPENDENCY(late, filesystem);
