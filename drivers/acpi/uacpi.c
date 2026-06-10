#include <uacpi/kernel_api.h>
#include <vex.arch/hart.h>
#include <vex.arch/ireq.h>
#include <vex/clocksrc.h>
#include <vex/hhdm.h>
#include <vex/intctl.h>
#include <vex/ireq.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/limits.h>
#include <vex/page.h>
#include <vex/pci.h>
#include <vex/rsdp.h>
#include <vex/vmm.h>
#include <vex/vprot.h>


uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr* out)
{
    *out = rsdp_address;

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
            sv = KP_VERBOSE;
            break;

        case UACPI_LOG_DEBUG:
            sv = KP_VERBOSE;
            break;

        default:
            sv = KP_VERBOSE;
            break;
    }

    kprintf(sv, "uacpi: %s", msg);
}

void* uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len)
{
    uintptr_t phys = page_align(addr);
    uintptr_t virt = hhdm_base + phys;
    uintptr_t end = page_align_up(addr + len);

    while(phys < end) {
        vmm_map(&sys_pagemap, virt, phys, VPROT_RW);
        phys += PAGE_SIZE;
        virt += PAGE_SIZE;
    }

    return phys_to_virt(addr);
}

void uacpi_kernel_unmap(void* addr, uacpi_size len)
{
    uintptr_t virt = page_align((uintptr_t)addr);
    uintptr_t end = page_align_up((uintptr_t)addr + len);

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
    return (uacpi_handle)42;
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
    return (uacpi_handle)69;
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
    return clocksrc_nsec();
}

void uacpi_kernel_stall(uacpi_u8 usec)
{
    uacpi_u64 deadline = clocksrc_nsec() + usec * UINT64_C(1000);

    while(clocksrc_nsec() < deadline) {
        pause_hart();
    }
}

void uacpi_kernel_sleep(uacpi_u64 msec)
{
    uacpi_u64 deadline = clocksrc_nsec() + msec * UINT64_C(1000000);

    while(clocksrc_nsec() < deadline) {
        pause_hart();
    }
}

uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address address, uacpi_handle* out_handle)
{
    struct pci_device* dev = pci_find(address.segment, address.bus, address.device, address.function);

    if(dev == nullptr) {
        return UACPI_STATUS_NOT_FOUND;
    }

    *out_handle = dev;

    return UACPI_STATUS_OK;
}

void uacpi_kernel_pci_device_close(uacpi_handle handle)
{
    // empty
}

uacpi_status uacpi_kernel_pci_read8(uacpi_handle handle, uacpi_size off, uacpi_u8* value)
{
    *value = pci_cfg_read8(handle, off);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read16(uacpi_handle handle, uacpi_size off, uacpi_u16* value)
{
    *value = pci_cfg_read16(handle, off);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read32(uacpi_handle handle, uacpi_size off, uacpi_u32* value)
{
    *value = pci_cfg_read32(handle, off);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8(uacpi_handle handle, uacpi_size off, uacpi_u8 value)
{
    pci_cfg_write8(handle, off, value);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write16(uacpi_handle handle, uacpi_size off, uacpi_u16 value)
{
    pci_cfg_write16(handle, off, value);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write32(uacpi_handle handle, uacpi_size off, uacpi_u32 value)
{
    pci_cfg_write32(handle, off, value);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request* request)
{
    if(request->type == UACPI_FIRMWARE_REQUEST_TYPE_BREAKPOINT) {
        pr_verbose("uacpi: AML breakpoint hit");
        return UACPI_STATUS_OK;
    }

    if(request->type == UACPI_FIRMWARE_REQUEST_TYPE_FATAL) {
        unsigned long long arg = request->fatal.arg;
        pr_verbose("uacpi: AML fatal: type=0x%02X code=0x%04X arg=0x%llx", request->fatal.type, (unsigned)(request->fatal.code), arg);
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

static void acpi_irq_dispatch(unsigned number, const struct regfile* restrict regs, void* restrict ctxdata)
{
    struct acpi_irq_trampoline* trampoline = ctxdata;

    trampoline->handler(trampoline->ctx);
}

uacpi_status uacpi_kernel_install_interrupt_handler(uacpi_u32 gsi, uacpi_interrupt_handler handler, uacpi_handle ctx,
    uacpi_handle* out_handle)
{
    struct acpi_irq_trampoline* trampoline = kmalloc(sizeof(struct acpi_irq_trampoline));

    if(trampoline == nullptr) {
        return UACPI_STATUS_OUT_OF_MEMORY;
    }

    struct intctl* controller = find_intctl(gsi);

    if(controller == nullptr) {
        kfree(trampoline);

        return UACPI_STATUS_NOT_FOUND;
    }

    trampoline->handler = handler;
    trampoline->ctx = ctx;
    trampoline->gsi = gsi;

    unsigned ireq_number = add_ireq_handler(UINT_MAX, &acpi_irq_dispatch, trampoline);

    if(ireq_number == UINT_MAX) {
        kfree(trampoline);

        return UACPI_STATUS_OUT_OF_MEMORY;
    }

    trampoline->ireq_number = ireq_number;

    intctl_route(controller, gsi, ireq_number + ARCH_IREQ_BASE, 0, TRIGGER_LEVEL, ACTIVE_LOW);
    intctl_unmask(controller, gsi);

    *out_handle = trampoline;

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_uninstall_interrupt_handler(uacpi_interrupt_handler handler, uacpi_handle irq_handle)
{
    struct acpi_irq_trampoline* trampoline = irq_handle;

    struct intctl* controller = find_intctl(trampoline->gsi);

    if(controller) {
        intctl_mask(controller, trampoline->gsi);
    }

    remove_ireq_handler(trampoline->ireq_number);
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
