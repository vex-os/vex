/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/initcall.h>
#include <kaneru/resource.h>

static struct resource pc_serial0 = {
    .name = "pc.serial0",
    .base = 0x3F8,
    .flags = RESOURCE_PMIO,
    .next = NULL,  
};

static int init_pc_dev(void)
{
    resource_register(&pc_serial0);

    return 0;
}
initcall_tier_0(pc_dev, init_pc_dev);
