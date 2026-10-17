#include <arch/amd64.lapic.h>
#include <arch/amd64.msr.h>
#include <arch/ireq.h>
#include <limine.h>
#include <vex/assert.h>
#include <vex/atomic.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/ireq.h>
#include <vex/kprintf.h>
#include <vex/limits.h>
#include <vex/panic.h>
#include <vex/thread.h>

LIMINE_REQUEST static volatile struct limine_mp_request mp_request = {
    .id = LIMINE_MP_REQUEST_ID,
    .revision = 0,
};

static unsigned panic_ipi_vector = 0;
static struct limine_mp_info* mp_infos[MAX_HARTS];
static struct thread boot_thread; // this_thread returns this before true threading initializes

static void panic_halt_handler(unsigned number, const struct hart_context* restrict ctx, void* restrict ctxdata)
{
    disable_interrupts();

    struct hart_local* hart = this_hart();
    pr_emerg("stopping hart %hu", hart->index);

    for(;;) {
        hart_spin();
    }
}

static void __noreturn limine_hart_entry(struct limine_mp_info* restrict info)
{
    hart_park((struct hart_local*)(uintptr_t)(info->extra_argument));
}

void arch_enumerate_harts(void)
{
    const struct limine_mp_response* response = mp_request.response;

    if(response == nullptr) {
        panic("bootloader did not provide multicore information");
    }

    for(uint64_t i = 0; i < response->cpu_count; ++i) {
        struct limine_mp_info* info = response->cpus[i];
        struct hart_local* hart = hart_register(info->lapic_id == response->bsp_lapic_id);

        if(hart) {
            hart->arch.lapic_id = info->lapic_id;
            mp_infos[hart->index] = info;
        }
    }
}

void arch_start_hart(struct hart_local* restrict hart)
{
    struct limine_mp_info* info = mp_infos[hart->index];

    info->extra_argument = (uintptr_t)(hart);
    atomic_store_n(&info->goto_address, &limine_hart_entry, ATOMIC_RELEASE);
}

void hart_setup(struct hart_local* restrict hart)
{
    wrmsr(MSR_GS_BASE, (uintptr_t)(hart->idle_thread));
    wrmsr(MSR_KERNEL_GS_BASE, (uintptr_t)(hart->idle_thread->arch.gs_base));
}

void hart_ipi(struct hart_local* restrict target, unsigned ireq_number)
{
    kassert(target);

    if(target == HART_IPI_EVERYONE) {
        lapic_ipi(0, LAPIC_IPI_EVERYONE, ireq_number + ARCH_IREQ_BASE);
        return;
    }

    if(target == HART_IPI_EXCEPT_ME) {
        lapic_ipi(0, LAPIC_IPI_EXCEPT_ME, ireq_number + ARCH_IREQ_BASE);
        return;
    }

    lapic_ipi(target->arch.lapic_id, LAPIC_IPI_SPECIFIC, ireq_number + ARCH_IREQ_BASE);
}

static void arch_init_boot_thread(void)
{
    boot_thread.self = &boot_thread;
    wrmsr(MSR_GS_BASE, (uintptr_t)(&boot_thread));
}

initcall_bsp_early(arch_init_boot_thread, INIT_FIRST);

static void arch_init_panic_ipi(void)
{
    panic_ipi_vector = add_ireq_handler(UINT_MAX, &panic_halt_handler, nullptr);
    panic_ipi_vector += ARCH_IREQ_BASE;
}

initcall_bsp_multicore(arch_init_panic_ipi, INIT_MIDDLE);
