/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <kernel/init.h>

static void init_core(void)
{

}
initcall(core, init_core);

static void init_subsys(void)
{

}
initcall(subsys, init_subsys);
initcall_depend(subsys, core);

static void init_filesystem(void)
{

}
initcall(filesystem, init_filesystem);
initcall_depend(filesystem, subsys);

static void init_device(void)
{

}
initcall(device, init_device);
initcall_depend(device, filesystem);

static void init_late(void)
{

}
initcall(late, init_late);
initcall_depend(late, device);
