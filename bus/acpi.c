#include <arch/limits.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <core/panic.h>
#include <init/initcall.h>
#include <mm/pslab.h>
#include <uacpi/uacpi.h>

define_initcall(acpi_tables, INITCALL_STAGE_BSP_VMM, INITCALL_ORDER_LAST)
{
    void* temporary_buffer = pslab_alloc_virt();

    if(temporary_buffer == nullptr) {
        panic("acpi: out of memory");
        unreachable();
    }

    uacpi_status status = uacpi_setup_early_table_access(temporary_buffer, PAGE_SIZE);

    pslab_free_virt(temporary_buffer);

    if(uacpi_unlikely_error(status)) {
        panic("acpi: early table access: %s", uacpi_status_to_string(status));
        unreachable();
    }
}

define_initcall(init_acpi, INITCALL_STAGE_BSP_BUS, INITCALL_ORDER_MIDDLE)
{
    uacpi_status status = uacpi_initialize(0);

    if(uacpi_unlikely_error(status)) {
        panic("acpi: %s", uacpi_status_to_string(status));
        unreachable();
    }

    status = uacpi_namespace_load();

    if(uacpi_unlikely_error(status)) {
        panic("acpi: uacpi_namespace_load: %s", uacpi_status_to_string(status));
        unreachable();
    }

    status = uacpi_namespace_initialize();

    if(uacpi_unlikely_error(status)) {
        panic("acpi: uacpi_namespace_initialize: %s", uacpi_status_to_string(status));
        unreachable();
    }
}
