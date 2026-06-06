#include <amd64/lapic.h>
#include <amd64/msr.h>
#include <arch/hart.h>
#include <arch/interrupts.h>
#include <arch/ireq.h>
#include <core/atomic.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <core/panic.h>
#include <init/initcall.h>
#include <ireq/dispatch.h>
#include <lib/string.h>
#include <limits.h>
#include <mm/malloc.h>

#define MSR_GS_BASE 0xC0000101

static unsigned hart_count = 0;
static unsigned panic_ipi_vector = 0;

static void panic_halt_handler(unsigned number, struct interrupt_frame* restrict frame, void* restrict ctxdata)
{
    disable_interrupts();

    pr_emerg("stopping hart %u", this_hart()->index);

    for(;;) {
        stop_hart();
    }
}

void broadcast_panic_ipi(void)
{
    lapic_broadcast_ipi(panic_ipi_vector);
}

define_initcall(amd64_hart, INITCALL_STAGE_SMP_HART, INITCALL_ORDER_FIRST)
{
    struct hart_local* hart = kmalloc(sizeof(struct hart_local));

    if(hart == nullptr) {
        panic("out of memory");
    }

    hart->self = hart;
    hart->index = atomic_fetch_add(&hart_count, 1, ATOMIC_RELAXED);

    wrmsr(MSR_GS_BASE, (uint64_t)hart);
}

define_initcall(amd64_panic_halt, INITCALL_STAGE_BSP_MULTICORE, INITCALL_ORDER_MIDDLE)
{
    panic_ipi_vector = add_ireq_handler(UINT_MAX, &panic_halt_handler, nullptr);
    panic_ipi_vector += ARCH_IREQ_BASE;
}
