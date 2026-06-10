#include <limine.h>
#include <uacpi/kernel_api.h>
#include <vex.arch/intr.h>
#include <vex.x86/pmio.h>
#include <vex/hhdm.h>
#include <vex/malloc.h>

struct pmio_range {
    uacpi_io_addr base;
    uacpi_size length;
};

uacpi_status uacpi_kernel_io_map(uacpi_io_addr base, uacpi_size len, uacpi_handle* out_handle)
{
    struct pmio_range* range = kmalloc(sizeof(struct pmio_range));

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
    struct pmio_range* range = handle;

    *value = pmread8(range->base + off);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read16(uacpi_handle h, uacpi_size off, uacpi_u16* value)
{
    struct pmio_range* range = h;

    *value = pmread16(range->base + off);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read32(uacpi_handle h, uacpi_size off, uacpi_u32* value)
{
    struct pmio_range* range = h;

    *value = pmread32(range->base + off);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(uacpi_handle h, uacpi_size off, uacpi_u8 value)
{
    struct pmio_range* range = h;

    pmwrite8(range->base + off, value);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write16(uacpi_handle h, uacpi_size off, uacpi_u16 value)
{
    struct pmio_range* range = h;

    pmwrite16(range->base + off, value);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write32(uacpi_handle h, uacpi_size off, uacpi_u32 value)
{
    struct pmio_range* range = h;

    pmwrite32(range->base + off, value);

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
