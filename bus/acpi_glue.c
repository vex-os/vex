#include <arch/hart.h>
#include <arch/ireq.h>
#include <bus/acpi_rsdp.h>
#include <bus/pci.h>
#include <core/intctl.h>
#include <core/ireq.h>
#include <core/kprintf.h>
#include <limits.h>
#include <mm/hhdm.h>
#include <mm/malloc.h>
#include <mm/page.h>
#include <mm/vmm.h>
#include <mm/vprot.h>
#include <stdint.h>
#include <time/clocksrc.h>
#include <uacpi/kernel_api.h>

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr* out)
{
    out[0] = rsdp_address;
    return UACPI_STATUS_OK;
}

void uacpi_kernel_log(uacpi_log_level lvl, const uacpi_char* msg)
{
    unsigned sv;

    switch(lvl) {
        case UACPI_LOG_ERROR:
            sv = KP_ERROR;
            break;

        case UACPI_LOG_WARN:
            sv = KP_WARNING;
            break;

        case UACPI_LOG_INFO:
            sv = KP_INFORM;
            break;

        case UACPI_LOG_TRACE:
            sv = KP_DEBUG;
            break;

        case UACPI_LOG_DEBUG:
            sv = KP_DEBUG;
            break;

        default:
            sv = KP_DEBUG;
            break;
    }

    kprintf(sv, "uacpi: %s", msg);
}

void* uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len)
{
    uintptr_t phys = page_align((uintptr_t)(addr));
    uintptr_t virt = hhdm_base + phys;
    uintptr_t end = page_align_up((uintptr_t)(addr) + len);

    while(phys < end) {
        vmm_map(&sys_pagemap, virt, phys, VPROT_RW);
        phys += PAGE_SIZE;
        virt += PAGE_SIZE;
    }

    return phys_to_virt(addr);
}

void uacpi_kernel_unmap(void* addr, uacpi_size len)
{
    uintptr_t virt = page_align((uintptr_t)(addr));
    uintptr_t end = page_align_up((uintptr_t)(addr) + len);

    while(virt < end) {
        vmm_unmap(&sys_pagemap, virt);

        virt += PAGE_SIZE;
    }
}

void* uacpi_kernel_alloc(uacpi_size size)
{
    return kmalloc(size);
}

void uacpi_kernel_free(void* mem)
{
    kfree(mem);
}

uacpi_handle uacpi_kernel_create_mutex(void)
{
    return ((uacpi_handle)(42));
}

void uacpi_kernel_free_mutex(uacpi_handle handle)
{
    // empty
}

uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle handle, uacpi_u16 timeout)
{
    return UACPI_STATUS_OK;
}

void uacpi_kernel_release_mutex(uacpi_handle handle)
{
    // empty
}

uacpi_handle uacpi_kernel_create_event(void)
{
    return ((uacpi_handle)(69));
}

void uacpi_kernel_free_event(uacpi_handle handle)
{
    // empty
}

uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle handle, uacpi_u16 timeout)
{
    return UACPI_TRUE;
}

void uacpi_kernel_signal_event(uacpi_handle handle)
{
    // empty
}

void uacpi_kernel_reset_event(uacpi_handle handle)
{
    // empty
}

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void)
{
    return clocksrc_read_nsec();
}

void uacpi_kernel_stall(uacpi_u8 usec)
{
    uacpi_u64 deadline = clocksrc_read_nsec() + ((uacpi_u64)(usec)) * UINT64_C(1000);

    while(clocksrc_read_nsec() < deadline) {
        yield_hart();
    }
}

void uacpi_kernel_sleep(uacpi_u64 msec)
{
    uacpi_u64 deadline = clocksrc_read_nsec() + msec * UINT64_C(1000000);

    while(clocksrc_read_nsec() < deadline) {
        yield_hart();
    }
}

uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address address, uacpi_handle* out_handle)
{
    struct pci_device* dev = pci_find_raw(address.segment, address.bus, address.device, address.function);

    if(dev == nullptr) {
        return UACPI_STATUS_NOT_FOUND;
    }

    out_handle[0] = dev;

    return UACPI_STATUS_OK;
}

void uacpi_kernel_pci_device_close(uacpi_handle handle)
{
    // empty
}

uacpi_status uacpi_kernel_pci_read8(uacpi_handle handle, uacpi_size off, uacpi_u8* val)
{
    val[0] = pci_read8(handle, off);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read16(uacpi_handle handle, uacpi_size off, uacpi_u16* val)
{
    val[0] = pci_read16(handle, off);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read32(uacpi_handle handle, uacpi_size off, uacpi_u32* val)
{
    val[0] = pci_read32(handle, off);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8(uacpi_handle handle, uacpi_size off, uacpi_u8 val)
{
    pci_write8(handle, off, val);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write16(uacpi_handle handle, uacpi_size off, uacpi_u16 val)
{
    pci_write16(handle, off, val);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write32(uacpi_handle handle, uacpi_size off, uacpi_u32 val)
{
    pci_write32(handle, off, val);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request* request)
{
    unsigned long long arg;

    if(request->type == UACPI_FIRMWARE_REQUEST_TYPE_BREAKPOINT) {
        pr_warning("uacpi: AML breakpoint hit");
        return UACPI_STATUS_OK;
    }

    if(request->type == UACPI_FIRMWARE_REQUEST_TYPE_FATAL) {
        arg = request->fatal.arg;
        pr_error("uacpi: AML fatal: type=0x%x code=0x%x arg=0x%llx", request->fatal.type, request->fatal.code, arg);
        return UACPI_STATUS_OK;
    }

    return UACPI_STATUS_UNIMPLEMENTED;
}

struct acpi_irq_trampoline {
    unsigned ireq_number;
    uacpi_interrupt_handler handler;
    uacpi_handle ctx;
    uint32_t gsi;
};

static void acpi_irq_dispatch(unsigned number, struct interrupt_frame* restrict frame, void* restrict ctxdata)
{
    struct acpi_irq_trampoline* trampoline = ctxdata;

    trampoline->handler(trampoline->ctx);
}

uacpi_status uacpi_kernel_install_interrupt_handler(uacpi_u32 gsi, uacpi_interrupt_handler handler, uacpi_handle ctx,
    uacpi_handle* out_handle)
{
    struct acpi_irq_trampoline* trampoline = kmalloc(sizeof(struct acpi_irq_trampoline));
    struct intctl* controller;
    unsigned ireq_number;

    if(trampoline == nullptr) {
        return UACPI_STATUS_OUT_OF_MEMORY;
    }

    controller = find_intctl(gsi);

    if(controller == nullptr) {
        kfree(trampoline);

        return UACPI_STATUS_NOT_FOUND;
    }

    trampoline->handler = handler;
    trampoline->ctx = ctx;
    trampoline->gsi = gsi;

    ireq_number = register_ireq_handler(UINT_MAX, &acpi_irq_dispatch, trampoline);

    if(ireq_number == UINT_MAX) {
        kfree(trampoline);

        return UACPI_STATUS_OUT_OF_MEMORY;
    }

    trampoline->ireq_number = ireq_number;

    intctl_route(controller, gsi, (uint8_t)(ireq_number + ARCH_IREQ_BASE), 0, TRIGGER_LEVEL, ACTIVE_LOW);
    intctl_unmask(controller, gsi);

    out_handle[0] = trampoline;

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(uacpi_interrupt_handler handler, uacpi_handle irq_handle)
{
    struct acpi_irq_trampoline* trampoline = irq_handle;
    struct intctl* controller;

    controller = find_intctl(trampoline->gsi);

    if(controller) {
        intctl_mask(controller, trampoline->gsi);
    }

    unregister_ireq_handler(trampoline->ireq_number);
    kfree(trampoline);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_schedule_work(uacpi_work_type type, uacpi_work_handler handler, uacpi_handle ctx)
{
    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_wait_for_work_completion(void)
{
    return UACPI_STATUS_OK;
}
