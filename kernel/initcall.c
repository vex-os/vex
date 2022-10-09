/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/initcall.h>

static int init_tier_0(void)
{

    return 0;
}
initcall_tier_0(tier_0, init_tier_0);
initcall_dependency(tier_1, tier_0);

static int init_tier_1(void)
{

    return 0;
}
initcall_tier_0(tier_1, init_tier_1);
initcall_dependency(tier_2, tier_1);

static int init_tier_2(void)
{

    return 0;
}
initcall_tier_0(tier_2, init_tier_2);
