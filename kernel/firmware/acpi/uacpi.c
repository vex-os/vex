#include "uacpi/status.h"
#include <uacpi/acpi.h>
#include <uacpi/kernel_api.h>
#include <uacpi/tables.h>
#include <vex/assert.h>
#include <vex/atomic.h>
#include <vex/clocksrc.h>
#include <vex/device.h>
#include <vex/driver.h>
#include <vex/errno.h>
#include <vex/event.h>
#include <vex/fwnode.h>
#include <vex/hart.h>
#include <vex/hhdm.h>
#include <vex/initcall.h>
#include <vex/intctl.h>
#include <vex/irq.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/memmap.h>
#include <vex/mmio.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/pci.h>
#include <vex/pci_config.h>
#include <vex/rsdp.h>
#include <vex/sched.h>
#include <vex/slab.h>
#include <vex/spinlock.h>
#include <vex/thread.h>
#include <vex/vprot.h>

#define UACPI_TIMEOUT_INFINITE 0xFFFF

#define SCI_FIRMWARE_ID "ACPI_SCI" // The namespace gives the SCI no node, and no _HID has an underscore

typedef struct acpi_work_s acpi_work_t;
typedef struct acpi_semaphore_s acpi_semaphore_t;
typedef struct acpi_irq_trampoline_s acpi_irq_trampoline_t;
typedef struct acpi_pci_function_s acpi_pci_function_t;

struct acpi_work_s {
    acpi_work_t* next;
    uacpi_work_handler handler;
    uacpi_handle ctx;
};

struct acpi_semaphore_s {
    uint32_t units;
    event_t given;
};

struct acpi_irq_trampoline_s {
    interrupt_action_t* action;
    uacpi_interrupt_handler handler;
    uacpi_handle ctxdata;
};

struct acpi_pci_function_s {
    pci_config_space_t* config_space;
    pci_address_t address;
};

static_assert(sizeof(acpi_work_t) <= SLAB_MAX_SIZE);
static_assert(sizeof(acpi_semaphore_t) <= SLAB_MAX_SIZE);
static_assert(sizeof(acpi_pci_function_t) <= SLAB_MAX_SIZE);

static const driver_match_t acpi_sci_matches[] = {
    DRIVER_MATCH_FIRMWARE(SCI_FIRMWARE_ID),
    DRIVER_MATCH_END(),
};

// ACPI 6.5, 5.2.9: SCI_INT is a level, active-low interrupt that devices may share
static fwnode_resource_t sci_resources[] = {
    {
        .type = FWNODE_RESOURCE_IRQ,
        .irq = {
            .trigger = TRIGGER_LEVEL,
            .polarity = ACTIVE_LOW,
            .flags = FWNODE_IRQ_SHARED,
        },
    },
};

static const fwnode_fixed_t sci_fixed = {
    .name = "sci",
    .id = SCI_FIRMWARE_ID,
    .resources = sci_resources,
    .num_resources = sizeof(sci_resources) / sizeof(sci_resources[0]),
};

static acpi_work_t* work_head;
static acpi_work_t* work_tail;
static spinlock_t work_lock = SPINLOCK_INIT;
static event_t work_queued;
static unsigned long irqs_running;
static unsigned long works_pending;
static acpi_irq_trampoline_t* sci_trampoline; // Set once uACPI installs the SCI, before the tree is first bound

static bool poll_until(bool (*done)(void* arg), void* arg, uacpi_u16 timeout_msec)
{
    uint64_t deadline = clock_source_now_ns() + timeout_msec * UINT64_C(1000000);

    for(;;) {
        if(done(arg)) {
            return true;
        }

        if(timeout_msec != UACPI_TIMEOUT_INFINITE && clock_source_now_ns() >= deadline) {
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

static bool take_unit(void* arg)
{
    acpi_semaphore_t* sem = arg;
    uint32_t units = atomic_load_n(&sem->units, ATOMIC_ACQUIRE);

    while(units > 0) {
        if(atomic_cmpxchg_weak(&sem->units, &units, units - 1, ATOMIC_ACQ_REL, ATOMIC_ACQUIRE)) {
            return true;
        }
    }

    return false;
}

static bool no_irqs_running(void* arg)
{
    return 0 == atomic_load_n(&irqs_running, ATOMIC_ACQUIRE);
}

static bool no_works_pending(void* arg)
{
    return 0 == atomic_load_n(&works_pending, ATOMIC_ACQUIRE);
}

static acpi_work_t* pop_work(void)
{
    bool intr_state = spinlock_acquire_intr(&work_lock);

    acpi_work_t* work = work_head;

    if(work) {
        work_head = work->next;

        if(work_head == nullptr) {
            work_tail = nullptr;
        }
    }

    spinlock_release_intr(&work_lock, intr_state);

    return work;
}

static void run_work(acpi_work_t* restrict work)
{
    uacpi_work_handler handler = work->handler;
    uacpi_handle ctx = work->ctx;

    handler(ctx);

    atomic_fetch_sub(&works_pending, 1, ATOMIC_ACQ_REL);
}

static void __noreturn work_thread(void* restrict arg)
{
    for(;;) {
        acpi_work_t* work = pop_work();

        if(work) {
            run_work(work);
            continue;
        }

        event_t* queued = &work_queued;
        event_await_blocking(&queued, 1);
    }
}

static void acpi_irq_dispatch(unsigned number, const hart_context_t* restrict ctx, void* restrict ctxdata)
{
    acpi_irq_trampoline_t* trampoline = ctxdata;

    atomic_fetch_add(&irqs_running, 1, ATOMIC_ACQ_REL);
    trampoline->handler(trampoline->ctxdata);
    atomic_fetch_sub(&irqs_running, 1, ATOMIC_ACQ_REL);
}

static int acpi_sci_probe(device_t* restrict device, const driver_match_t* restrict match)
{
    return DRIVER_SCORE_SPECIFIC;
}

static int acpi_sci_attach(device_t* restrict device, const driver_match_t* restrict match)
{
    device->ctxdata = sci_trampoline;

    return device_request_irq(device, 0, &acpi_irq_dispatch, sci_trampoline);
}

// Events, the embedded controller and sleep all come through the SCI, which has nowhere else to go
static int acpi_sci_offline(device_t* restrict device)
{
    pr_warning("acpi: %s carries every ACPI event", device->name);
    return -EBUSY;
}

static void acpi_sci_online(device_t* restrict device)
{
    // empty
}

static void acpi_sci_detach(device_t* restrict device)
{
    pr_warning("acpi: %s is gone, and ACPI events with it", device->name);
}

// The trampoline is uACPI's, and lives as long as it does
static void acpi_sci_release(device_t* restrict device)
{
    // empty
}

static const driver_ops_t acpi_sci_driver_ops = {
    .probe = &acpi_sci_probe,
    .attach = &acpi_sci_attach,
    .offline = &acpi_sci_offline,
    .online = &acpi_sci_online,
    .detach = &acpi_sci_detach,
    .release = &acpi_sci_release,
};

define_driver(acpi_sci, &acpi_sci_driver_ops, acpi_sci_matches);

// The SCI's handler is requested by the driver of its node, so that the
// node relies on the interrupt controller of its GSI like any other
static uacpi_status add_sci_node(uacpi_u32 gsi, acpi_irq_trampoline_t* restrict trampoline)
{
    device_t* root = device_get_root();

    if(root == nullptr) {
        return UACPI_STATUS_OUT_OF_MEMORY;
    }

    sci_resources[0].irq.number = gsi;
    sci_trampoline = trampoline;

    fwnode_t fwnode;
    fwnode_init_fixed(&fwnode, &sci_fixed);

    char name[DEVICE_NAME_MAX];
    device_t* device;
    int retval = fwnode_get_name(&fwnode, name, sizeof(name));

    if(retval == 0) {
        retval = device_add(root, &fwnode, name, &device);
    }

    device_decref(root);

    if(retval < 0) {
        sci_trampoline = nullptr;
        return retval == -ENOMEM ? UACPI_STATUS_OUT_OF_MEMORY : UACPI_STATUS_INTERNAL_ERROR;
    }

    device_decref(device);

    return UACPI_STATUS_OK;
}

static bool is_sci(uacpi_u32 gsi)
{
    struct acpi_fadt* fadt;

    return uacpi_likely_success(uacpi_table_fadt(&fadt)) && gsi == fadt->sci_int;
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
    if(memmap_is_ram(addr, len)) {
        return phys_to_virt(addr);
    }

    return mmio_map(addr, len, VPROT_MMIO);
}

void uacpi_kernel_unmap(void* addr, uacpi_size len)
{
    if(mmio_owns(addr)) {
        mmio_unmap(addr);
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

uacpi_handle uacpi_kernel_create_semaphore(uacpi_u32 initial_units)
{
    acpi_semaphore_t* sem = slab_alloc(sizeof(acpi_semaphore_t));

    if(sem) {
        sem->units = initial_units;
        event_init(&sem->given);
    }

    return sem;
}

void uacpi_kernel_free_semaphore(uacpi_handle handle)
{
    acpi_semaphore_t* sem = handle;

    event_deinit(&sem->given);
    slab_free(sem);
}

uacpi_status uacpi_kernel_wait_for_semaphore(uacpi_handle handle, uacpi_u16 timeout)
{
    acpi_semaphore_t* sem = handle;

    if(take_unit(sem)) {
        return UACPI_STATUS_OK;
    }

    if(timeout == 0) {
        return UACPI_STATUS_TIMEOUT;
    }

    if(!interrupt_state()) {
        return poll_until(&take_unit, sem, timeout) ? UACPI_STATUS_OK : UACPI_STATUS_TIMEOUT;
    }

    uint64_t deadline = clock_source_now_ns() + timeout * UINT64_C(1000000);

    for(;;) {
        event_t* given = &sem->given;
        ssize_t result;

        if(timeout == UACPI_TIMEOUT_INFINITE) {
            result = event_await_blocking(&given, 1);
        }
        else {
            uint64_t now = clock_source_now_ns();

            if(now >= deadline) {
                if(take_unit(sem))
                    return UACPI_STATUS_OK;
                return UACPI_STATUS_TIMEOUT;
            }

            result = event_await_timeout(&given, 1, deadline - now);
        }

        if(take_unit(sem)) {
            return UACPI_STATUS_OK;
        }

        if(result < 0 && result != -ETIMEDOUT) {
            return UACPI_STATUS_OUT_OF_MEMORY;
        }
    }
}

void uacpi_kernel_signal_semaphore(uacpi_handle handle)
{
    acpi_semaphore_t* sem = handle;

    atomic_fetch_add(&sem->units, 1, ATOMIC_ACQ_REL);
    event_trigger(&sem->given, true);
}

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void)
{
    return clock_source_now_ns();
}

void uacpi_kernel_stall(uacpi_u8 usec)
{
    uacpi_u64 deadline = clock_source_now_ns() + usec * UINT64_C(1000);

    while(clock_source_now_ns() < deadline) {
        hart_spin();
    }
}

void uacpi_kernel_sleep(uacpi_u64 msec)
{
    uacpi_u64 deadline = clock_source_now_ns() + msec * UINT64_C(1000000);

    while(clock_source_now_ns() < deadline) {
        hart_spin();
    }
}

// AML may reach a function before the bus it is on is scanned, or one that isn't there
uacpi_status uacpi_kernel_pci_device_open(uacpi_pci_address address, uacpi_handle* out_handle)
{
#ifdef CONFIG_PCI
    pci_config_space_t* config_space = pci_config_space_find(address.segment, address.bus);
#else
    pci_config_space_t* config_space = nullptr;
#endif

    if(config_space == nullptr) {
        return UACPI_STATUS_NOT_FOUND;
    }

    acpi_pci_function_t* function = slab_alloc(sizeof(acpi_pci_function_t));

    if(function == nullptr) {
        return UACPI_STATUS_OUT_OF_MEMORY;
    }

    function->config_space = config_space;
    function->address.segment = address.segment;
    function->address.bus = address.bus;
    function->address.device = address.device;
    function->address.function = address.function;

    *out_handle = function;

    return UACPI_STATUS_OK;
}

void uacpi_kernel_pci_device_close(uacpi_handle handle)
{
    slab_free(handle);
}

uacpi_status uacpi_kernel_pci_read8(uacpi_handle handle, uacpi_size off, uacpi_u8* value)
{
    acpi_pci_function_t* function = handle;

    *value = pci_config_space_read8(function->config_space, &function->address, off);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read16(uacpi_handle handle, uacpi_size off, uacpi_u16* value)
{
    acpi_pci_function_t* function = handle;

    *value = pci_config_space_read16(function->config_space, &function->address, off);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_read32(uacpi_handle handle, uacpi_size off, uacpi_u32* value)
{
    acpi_pci_function_t* function = handle;

    *value = pci_config_space_read32(function->config_space, &function->address, off);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write8(uacpi_handle handle, uacpi_size off, uacpi_u8 value)
{
    acpi_pci_function_t* function = handle;

    pci_config_space_write8(function->config_space, &function->address, off, value);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write16(uacpi_handle handle, uacpi_size off, uacpi_u16 value)
{
    acpi_pci_function_t* function = handle;

    pci_config_space_write16(function->config_space, &function->address, off, value);

    return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_pci_write32(uacpi_handle handle, uacpi_size off, uacpi_u32 value)
{
    acpi_pci_function_t* function = handle;

    pci_config_space_write32(function->config_space, &function->address, off, value);

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

uacpi_status uacpi_kernel_install_interrupt_handler(uacpi_u32 gsi, uacpi_interrupt_handler handler, uacpi_handle ctxdata,
    uacpi_handle* out_handle)
{
    acpi_irq_trampoline_t* trampoline = kmalloc(sizeof(acpi_irq_trampoline_t));

    if(trampoline == nullptr) {
        return UACPI_STATUS_OUT_OF_MEMORY;
    }

    trampoline->action = nullptr;
    trampoline->handler = handler;
    trampoline->ctxdata = ctxdata;

    if(is_sci(gsi)) {
        uacpi_status status = add_sci_node(gsi, trampoline);

        if(uacpi_unlikely_error(status)) {
            kfree(trampoline);
            return status;
        }

        *out_handle = trampoline;

        return UACPI_STATUS_OK;
    }

    // NOTE: GPE block devices on interrupts of their own stay out of the tree, which QEMU and most boards never have
    int retval = interrupt_attach(gsi, TRIGGER_LEVEL, ACTIVE_LOW, INTERRUPT_SHARED, &acpi_irq_dispatch, trampoline, &trampoline->action);

    if(retval < 0) {
        kfree(trampoline);

        return retval == -ENOMEM ? UACPI_STATUS_OUT_OF_MEMORY : UACPI_STATUS_NOT_FOUND;
    }

    *out_handle = trampoline;

    return UACPI_STATUS_OK;
}

// Vex never tears uACPI down, and the node of the SCI never goes offline
uacpi_status uacpi_kernel_uninstall_interrupt_handler(uacpi_interrupt_handler handler, uacpi_handle irq_handle)
{
    acpi_irq_trampoline_t* trampoline = irq_handle;

    kassert(trampoline != sci_trampoline);

    interrupt_detach(trampoline->action);
    kfree(trampoline);

    return UACPI_STATUS_OK;
}

uacpi_handle uacpi_kernel_create_work_item(void)
{
    return slab_alloc(sizeof(acpi_work_t));
}

void uacpi_kernel_free_work_item(uacpi_handle handle)
{
    slab_free(handle);
}

void uacpi_kernel_schedule_work(uacpi_work_type type, uacpi_handle work_item, uacpi_work_handler handler, uacpi_handle ctx)
{
    acpi_work_t* work = work_item;

    work->next = nullptr;
    work->handler = handler;
    work->ctx = ctx;

    atomic_fetch_add(&works_pending, 1, ATOMIC_ACQ_REL);

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
}

uacpi_status uacpi_kernel_wait_for_work_completion(void)
{
    poll_until(&no_irqs_running, nullptr, UACPI_TIMEOUT_INFINITE);

    for(acpi_work_t* work = pop_work(); work; work = pop_work()) {
        run_work(work);
    }

    poll_until(&no_works_pending, nullptr, UACPI_TIMEOUT_INFINITE);

    return UACPI_STATUS_OK;
}

static void init_acpi_work(void)
{
    event_init(&work_queued);

    thread_t* worker = thread_create_kernel(&work_thread, nullptr);

    if(worker == nullptr) {
        panic("uacpi: out of memory");
    }

    // GPE work has to run on the boot CPU,
    // some firmware has SMI handlers that assume it
    worker->bound_hart = boot_hart();

    sched_enqueue(worker);
}

initcall_bsp_multicore(init_acpi_work, INIT_LAST);
