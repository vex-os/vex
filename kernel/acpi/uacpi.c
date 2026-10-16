#include <arch/ireq.h>
#include <uacpi/kernel_api.h>
#include <vex/atomic.h>
#include <vex/clocksrc.h>
#include <vex/event.h>
#include <vex/hart.h>
#include <vex/hhdm.h>
#include <vex/initcall.h>
#include <vex/intctl.h>
#include <vex/ireq.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/limits.h>
#include <vex/mutex.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/pci.h>
#include <vex/pmap.h>
#include <vex/rsdp.h>
#include <vex/sched.h>
#include <vex/spinlock.h>
#include <vex/thread.h>
#include <vex/vprot.h>

#define UACPI_TIMEOUT_INFINITE 0xFFFF

struct acpi_work {
    struct acpi_work* next;
    uacpi_work_handler handler;
    uacpi_handle ctx;
};

static struct acpi_work* work_head;
static struct acpi_work* work_tail;
static spinlock_type work_lock = SPINLOCK_INIT;
static struct event work_queued;
static unsigned long works_running;
static unsigned long irqs_running;

// Waits by polling, yielding when the
// scheduler can switch away and spinning otherwise
static bool poll_until(bool (*done)(void* arg), void* arg, uacpi_u16 timeout_msec)
{
    uint64_t deadline = clocksrc_nsec() + timeout_msec * UINT64_C(1000000);

    for(;;) {
        if(done(arg)) {
            return true;
        }

        if(timeout_msec != UACPI_TIMEOUT_INFINITE && clocksrc_nsec() >= deadline) {
            return false;
        }

        if(interrupt_state()) {
            sched_yield();
        }
        else {
            hart_spin();
        }
    }
}

static bool try_mutex(void* arg)
{
    return mutex_try_acquire(arg);
}

static bool try_event(void* arg)
{
    struct event* evnt = arg;
    return event_await(&evnt, 1) >= 0;
}

static bool no_irqs_running(void* arg)
{
    return 0 == atomic_load_n(&irqs_running, ATOMIC_ACQUIRE);
}

static bool no_works_running(void* arg)
{
    return 0 == atomic_load_n(&works_running, ATOMIC_ACQUIRE);
}

static struct acpi_work* pop_work(void)
{
    bool intr_state = spinlock_acquire_intr(&work_lock);

    struct acpi_work* work = work_head;

    if(work) {
        work_head = work->next;

        if(work_head == nullptr) {
            work_tail = nullptr;
        }

        atomic_fetch_add(&works_running, 1, ATOMIC_ACQ_REL);
    }

    spinlock_release_intr(&work_lock, intr_state);

    return work;
}

static void run_work(struct acpi_work* restrict work)
{
    work->handler(work->ctx);
    kfree(work);
    atomic_fetch_sub(&works_running, 1, ATOMIC_ACQ_REL);
}

static void __noreturn work_thread(void* restrict arg)
{
    for(;;) {
        struct acpi_work* work = pop_work();

        if(work) {
            run_work(work);
            continue;
        }

        struct event* queued = &work_queued;
        event_await_blocking(&queued, 1);
    }
}

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr* out)
{
    *out = rsdp_address;

    return UACPI_STATUS_OK;
}

void uacpi_kernel_log(uacpi_log_level lvl, const uacpi_char* msg)
{
#ifndef CONFIG_UACPI_QUIET
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
#endif
}

void* uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len)
{
    uintptr_t phys = page_align(addr);
    uintptr_t virt = hhdm_base + phys;
    uintptr_t end = page_align_up(addr + len);

    while(phys < end) {
        pmap_map(&sys_pmap, virt, phys, VPROT_RW);
        phys += PAGE_SIZE;
        virt += PAGE_SIZE;
    }

    return phys_to_virt(addr);
}

void uacpi_kernel_unmap(void* addr, uacpi_size len)
{
    // empty: hhdm pages may back other tables and RAM
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
    struct mutex* mtx = kmalloc(sizeof(struct mutex));

    if(mtx) {
        mutex_init(mtx);
    }

    return mtx;
}

void uacpi_kernel_free_mutex(uacpi_handle handle)
{
    mutex_deinit(handle);
    kfree(handle);
}

uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle handle, uacpi_u16 timeout)
{
    if(timeout == UACPI_TIMEOUT_INFINITE) {
        mutex_acquire(handle);
        return UACPI_STATUS_OK;
    }

    return poll_until(&try_mutex, handle, timeout) ? UACPI_STATUS_OK : UACPI_STATUS_TIMEOUT;
}

void uacpi_kernel_release_mutex(uacpi_handle handle)
{
    mutex_release(handle);
}

uacpi_handle uacpi_kernel_create_event(void)
{
    struct event* evnt = kmalloc(sizeof(struct event));

    if(evnt) {
        event_init(evnt);
    }

    return evnt;
}

void uacpi_kernel_free_event(uacpi_handle handle)
{
    event_deinit(handle);
    kfree(handle);
}

uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle handle, uacpi_u16 timeout)
{
    if(timeout == UACPI_TIMEOUT_INFINITE && interrupt_state()) {
        struct event* evnt = handle;
        return event_await_blocking(&evnt, 1) >= 0;
    }

    return poll_until(&try_event, handle, timeout);
}

void uacpi_kernel_signal_event(uacpi_handle handle)
{
    event_trigger(handle, true);
}

void uacpi_kernel_reset_event(uacpi_handle handle)
{
    event_reset(handle);
}

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void)
{
    return clocksrc_nsec();
}

void uacpi_kernel_stall(uacpi_u8 usec)
{
    uacpi_u64 deadline = clocksrc_nsec() + usec * UINT64_C(1000);

    while(clocksrc_nsec() < deadline) {
        hart_spin();
    }
}

void uacpi_kernel_sleep(uacpi_u64 msec)
{
    uacpi_u64 deadline = clocksrc_nsec() + msec * UINT64_C(1000000);

    while(clocksrc_nsec() < deadline) {
        hart_spin();
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
    uacpi_handle ctxdata;
    uint32_t gsi;
};

static void acpi_irq_dispatch(unsigned number, const struct hart_context* restrict ctx, void* restrict ctxdata)
{
    struct acpi_irq_trampoline* trampoline = ctxdata;

    atomic_fetch_add(&irqs_running, 1, ATOMIC_ACQ_REL);
    trampoline->handler(trampoline->ctxdata);
    atomic_fetch_sub(&irqs_running, 1, ATOMIC_ACQ_REL);
}

uacpi_status uacpi_kernel_install_interrupt_handler(uacpi_u32 gsi, uacpi_interrupt_handler handler, uacpi_handle ctxdata,
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
    trampoline->ctxdata = ctxdata;
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
    struct acpi_work* work = kmalloc(sizeof(struct acpi_work));

    if(work == nullptr) {
        return UACPI_STATUS_OUT_OF_MEMORY;
    }

    work->handler = handler;
    work->ctx = ctx;

    bool intr_state = spinlock_acquire_intr(&work_lock);

    if(work_tail) {
        work_tail->next = work;
    }
    else {
        work_head = work;
    }

    work_tail = work;

    spinlock_release_intr(&work_lock, intr_state);

    event_trigger(&work_queued, true);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_wait_for_work_completion(void)
{
    poll_until(&no_irqs_running, nullptr, UACPI_TIMEOUT_INFINITE);

    // Draining the queue here also works
    // before the scheduler has started the worker
    for(struct acpi_work* work = pop_work(); work; work = pop_work()) {
        run_work(work);
    }

    poll_until(&no_works_running, nullptr, UACPI_TIMEOUT_INFINITE);

    return UACPI_STATUS_OK;
}

static void init_acpi_work(void)
{
    event_init(&work_queued);

    struct thread* worker = create_thread_kern(&work_thread, nullptr);

    if(worker == nullptr) {
        panic("uacpi: out of memory");
    }

    // GPE work has to run on the boot CPU,
    // some firmware has SMI handlers that assume it
    worker->pinned_hart = boot_hart();

    sched_enqueue(worker);
}

initcall_bsp_multicore(init_acpi_work, INIT_LAST);
