#include <arch/amd64.lapic.h>
#include <arch/amd64.msr.h>
#include <arch/ireq.h>
#include <limine.h>
#include <vex/assert.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/ireq.h>
#include <vex/kprintf.h>
#include <vex/limits.h>
#include <vex/thread.h>

static unsigned panic_ipi_vector = 0;

static void panic_halt_handler(unsigned number, const struct hart_context* restrict ctx, void* restrict ctxdata)
{
    disable_interrupts();

    struct hart_local* hart = this_hart();
    pr_emerg("stopping hart %hu", hart->index);

    for(;;) {
        hart_spin();
    }
}

void hart_setup_1(const struct limine_mp_info* restrict info, const struct limine_mp_response* restrict response)
{
    struct hart_local* hart = &harts[info->processor_id];

    hart->index = info->processor_id;
    hart->flags = HART_FLAG_ACTIVE;
    hart->arch.lapic_id = info->lapic_id;

    if(info->lapic_id == response->bsp_lapic_id) {
        hart->flags |= HART_FLAG_BSP;
    }
}

void hart_setup_2(const struct limine_mp_info* restrict info)
{
    struct hart_local* hart = &harts[info->processor_id];

    // Can't go through arch_thread_ctx_load() here: it calls this_hart(),
    // which itself depends on GS_BASE already being valid on this hart -
    // exactly what we're priming for the first time. Set both halves of
    // the GS pair by hand instead. KERNEL_GS_BASE is what swapgs_if_user
    // will swap into GS_BASE on return to ring 3; the idle thread never
    // gets there, but keeping the pair consistent from boot avoids a
    // stale/garbage KERNEL_GS_BASE the first time any hart does take an
    // interrupt from ring 3.
    wrmsr(0xC0000101, (uintptr_t)(hart->idle_thread));
    wrmsr(0xC0000102, (uintptr_t)(hart->idle_thread->arch.gs_base));
}

void hart_ipi(struct hart_local* restrict target, unsigned ireq_number)
{
    kassert(target); // old api bro

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

static void arch_init_panic_ipi(void)
{
    panic_ipi_vector = add_ireq_handler(UINT_MAX, &panic_halt_handler, nullptr);
    panic_ipi_vector += ARCH_IREQ_BASE;
}

initcall_bsp_multicore(arch_init_panic_ipi, INIT_MIDDLE);
