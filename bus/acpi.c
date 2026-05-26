// SPDX-License-Identifier: BSD-3-Clause

#include <core/errno.h>
#include <core/kprintf.h>
#include <init/initfn.h>
#include <uacpi/uacpi.h>

static int acpi_init(void)
{
    uacpi_status status;

    status = uacpi_initialize(0);

    if(uacpi_unlikely_error(status)) {
        kprintf(KP_ERROR, "acpi: %s", uacpi_status_to_string(status));
        return EIO;
    }

    status = uacpi_namespace_load();

    if(uacpi_unlikely_error(status)) {
        kprintf(KP_ERROR, "acpi: uacpi_namespace_load: %s", uacpi_status_to_string(status));
        return EIO;
    }

    status = uacpi_namespace_initialize();

    if(uacpi_unlikely_error(status)) {
        kprintf(KP_ERROR, "acpi: uacpi_namespace_initialize: %s", uacpi_status_to_string(status));
        return EIO;
    }

    return 0;
}

DEFINE_INITFN(acpi_init, INITFN_STAGE_BUS, INITFN_ORDER_FIRST);
