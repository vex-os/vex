/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <kan/errno.h>
#include <kan/initcall.h>

static int tier_0_init(void)
{
    return EOK;
}
initcall_bind(tier_0, tier_0_init);
initcall_depend(tier_1, tier_0);

static int tier_1_init(void)
{
    return EOK;
}
initcall_bind(tier_1, tier_1_init);
initcall_depend(tier_2, tier_1);

static int tier_2_init(void)
{
    return EOK;
}
initcall_bind(tier_2, tier_2_init);
