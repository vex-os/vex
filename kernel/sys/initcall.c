/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/initcall.h>

static int tier_0_init(void)
{

    return 0;
}
initcall_bind(tier_0, tier_0_init);
initcall_depend(tier_1, tier_0);

static int tier_1_init(void)
{

    return 0;
}
initcall_bind(tier_1, tier_1_init);
initcall_depend(tier_2, tier_1);

static int tier_2_init(void)
{

    return 0;
}
initcall_bind(tier_2, tier_2_init);
