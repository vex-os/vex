/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <kan/initcall.h>

static void init_early(void)
{

}
initcall(early, init_early);

static void init_core(void)
{

}
initcall(core, init_core);
initcall_depend(core, early);

static void init_postcore(void)
{

}
initcall(postcore, init_postcore);
initcall_depend(postcore, core);

static void init_arch(void)
{

}
initcall(arch, init_arch);
initcall_depend(arch, postcore);

static void init_subsys(void)
{

}
initcall(subsys, init_subsys);
initcall_depend(subsys, postcore);

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
