#include <limine.h>
#include <vex.arch/ireq.h>
#include <vex.x86/lapic.h>
#include <vex.x86/msr.h>
#include <vex/assert.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/ireq.h>
#include <vex/kprintf.h>
#include <vex/limits.h>

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
    hart->x86_lapic_id = info->lapic_id;

    if(info->lapic_id == response->bsp_lapic_id) {
        hart->flags |= HART_FLAG_BSP;
    }
}

void hart_setup_2(const struct limine_mp_info* restrict info)
{
    struct hart_local* hart = &harts[info->processor_id];
    wrmsr(0xC0000101, (uintptr_t)(hart->idle_thread));
}

void hart_ipi(struct hart_local* restrict target, unsigned ireq_number)
{
    if(target == nullptr) {
        lapic_broadcast_ipi((uint8_t)(ireq_number + ARCH_IREQ_BASE));
    }
    else {
        lapic_send_ipi(target->x86_lapic_id, (uint8_t)(ireq_number + ARCH_IREQ_BASE));
    }
}

static void arch_init_panic_ipi(void)
{
    panic_ipi_vector = add_ireq_handler(UINT_MAX, &panic_halt_handler, nullptr);
    panic_ipi_vector += ARCH_IREQ_BASE;
}

initcall_bsp_multicore(arch_init_panic_ipi, INIT_MIDDLE);
