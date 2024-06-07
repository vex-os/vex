/* SPDX-License-Identifier: GPL-2.0-only */
#include <sys/initcall.h>

static void init_core(void)
{

}
initcall(core, init_core);

static void init_subsys(void)
{

}
initcall(subsys, init_subsys);
initcall_dependency(subsys, core);

static void init_device(void)
{

}
initcall(device, init_device);
initcall_dependency(device, subsys);

static void init_filesystem(void)
{

}
initcall(filesystem, init_filesystem);
initcall_dependency(filesystem, device);

static void init_late(void)
{

}
initcall(late, init_late);
initcall_dependency(late, filesystem);
