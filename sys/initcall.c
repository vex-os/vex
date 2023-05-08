/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <sys/initcall.h>

static void init_early(void)
{

}
initcall_binding(early, init_early);

static void init_common(void)
{

}
initcall_binding(common, init_common);
initcall_dependency(common, early);

static void init_device(void)
{

}
initcall_binding(device, init_device);
initcall_dependency(device, common);
