#include <uacpi/uacpi.h>
#include <vex/errno.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/limits.h>
#include <vex/panic.h>
#include <vex/pslab.h>

static void init_acpi_tables(void)
{
    void* temporary_buffer = pslab_alloc_virt();

    if(temporary_buffer == nullptr) {
        panic("acpi: out of memory");
    }

    uacpi_status status = uacpi_setup_early_table_access(temporary_buffer, PAGE_SIZE);

    pslab_free_virt(temporary_buffer);

    if(uacpi_unlikely_error(status)) {
        panic("acpi: early table access: %s", uacpi_status_to_string(status));
    }
}

initcall_bsp_vmm(init_acpi_tables, INIT_LAST);

static void init_acpi(void)
{
    uacpi_status status = uacpi_initialize(0);

    if(uacpi_unlikely_error(status)) {
        panic("acpi: %s", uacpi_status_to_string(status));
    }

    status = uacpi_namespace_load();

    if(uacpi_unlikely_error(status)) {
        panic("acpi: uacpi_namespace_load: %s", uacpi_status_to_string(status));
    }

    status = uacpi_namespace_initialize();

    if(uacpi_unlikely_error(status)) {
        panic("acpi: uacpi_namespace_initialize: %s", uacpi_status_to_string(status));
    }

    disable_interrupts();
}

initcall_bsp_bus(init_acpi, INIT_MIDDLE);
