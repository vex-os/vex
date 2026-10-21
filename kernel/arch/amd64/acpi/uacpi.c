#include <arch/io.h>
#include <limine.h>
#include <uacpi/kernel_api.h>
#include <vex/hart.h>
#include <vex/hhdm.h>
#include <vex/kmalloc.h>

typedef struct pmio_range_s pmio_range_t;

struct pmio_range_s {
    uacpi_io_addr base;
    uacpi_size length;
};

uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size len, uacpi_handle* out_handle)
{
    pmio_range_t* range = kmalloc(sizeof(pmio_range_t));

    if(range == nullptr) {
        return UACPI_STATUS_OUT_OF_MEMORY;
    }

    range->base = base;
    range->length = len;

    *out_handle = range;

    return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap(uacpi_handle handle)
{
    kfree(handle);
}

uacpi_status uacpi_kernel_io_read8(uacpi_handle handle, uacpi_size off, uacpi_u8* value)
{
    pmio_range_t* range = handle;

    *value = port_read8(range->base + off);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read16(uacpi_handle h, uacpi_size off, uacpi_u16* value)
{
    pmio_range_t* range = h;

    *value = port_read16(range->base + off);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read32(uacpi_handle h, uacpi_size off, uacpi_u32* value)
{
    pmio_range_t* range = h;

    *value = port_read32(range->base + off);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(uacpi_handle h, uacpi_size off, uacpi_u8 value)
{
    pmio_range_t* range = h;

    port_write8(range->base + off, value);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write16(uacpi_handle h, uacpi_size off, uacpi_u16 value)
{
    pmio_range_t* range = h;

    port_write16(range->base + off, value);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write32(uacpi_handle h, uacpi_size off, uacpi_u32 value)
{
    pmio_range_t* range = h;

    port_write32(range->base + off, value);

    return UACPI_STATUS_OK;
}

uacpi_interrupt_state uacpi_kernel_disable_interrupts(void)
{
    return disable_interrupts();
}

void uacpi_kernel_restore_interrupts(uacpi_interrupt_state state)
{
    restore_interrupts(state);
}

uacpi_handle uacpi_kernel_create_spinlock(void)
{
    return (uacpi_handle)420;
}

void uacpi_kernel_free_spinlock(uacpi_handle handle)
{
    // empty
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle handle)
{
    return disable_interrupts();
}

void uacpi_kernel_unlock_spinlock(uacpi_handle handle, uacpi_cpu_flags flags)
{
    restore_interrupts(flags);
}

uacpi_thread_id uacpi_kernel_get_thread_id(void)
{
    return (uacpi_thread_id)1;
}
