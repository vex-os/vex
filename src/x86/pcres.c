/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, VxEngine Contributors */
#include <x86/pcres.h>

resource_t bochs_port_E9 = {
    .name = "bochs_port_E9",
    .base = 0x00E9,
    .size = 1,
    .flags = RESOURCE_PMIO | RESOURCE_PMIO_SLOW,
    .next = NULL,
};

resource_t serial_0 = {
    .name = "serial_0",
    .base = 0x03F8,
    .size = 8,
    .flags = RESOURCE_PMIO,
    .next = NULL,
};

resource_t serial_1 = {
    .name = "serial_1",
    .base = 0x02F8,
    .size = 8,
    .flags = RESOURCE_PMIO,
    .next = NULL,
};

static int init_pcres(void)
{
    register_resource(&bochs_port_E9);
    register_resource(&serial_0);
    register_resource(&serial_1);
    return 0;
}
initcall_tier_0(pcres, init_pcres);
