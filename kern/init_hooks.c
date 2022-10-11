/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/init.h>

static void tier_0_init(void)
{

}
initcall_bind(tier_0, tier_0_init);
initcall_depend(tier_1, tier_0);

static void tier_1_init(void)
{

}
initcall_bind(tier_1, tier_1_init);
initcall_depend(tier_2, tier_1);

static void tier_2_init(void)
{

}
initcall_bind(tier_2, tier_2_init);
