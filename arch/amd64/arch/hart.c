#include <amd64/lapic.h>
#include <amd64/msr.h>
#include <arch/hart.h>
#include <arch/interrupts.h>
#include <arch/ireq.h>
#include <core/assert.h>
#include <core/atomic.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <init/initcall.h>
#include <ireq/dispatch.h>
#include <lib/string.h>
#include <mm/malloc.h>

#define PANIC_HALT_VECTOR 0xFF
#define PANIC_HALT_IREQ   (PANIC_HALT_VECTOR - ARCH_IREQ_BASE)

#define MSR_GS_BASE 0xC0000101

static unsigned int hart_count = 0;

static void panic_halt_handler(unsigned number, struct interrupt_frame* restrict frame, void* restrict ctxdata)
{
    disable_interrupts();

    for(;;) {
        stop_hart();
    }
}

void halt_other_harts(void)
{
    lapic_send_broadcast_ipi(PANIC_HALT_VECTOR);
}

define_initcall(amd64_hart, INITCALL_STAGE_SMP_HART, INITCALL_ORDER_FIRST)
{
    struct hart_local* hart;

    hart = kmalloc(sizeof(struct hart_local));
    strict_kassert_text(hart, "out of memory");

    memset(hart, 0, sizeof(struct hart_local));

    hart->self = hart;
    hart->index = atomic_fetch_add(&hart_count, 1, ATOMIC_RELAXED);

    msr_write(MSR_GS_BASE, (uint64_t)(hart));
}

define_initcall(amd64_panic_halt, INITCALL_STAGE_SMP_IREQ, INITCALL_ORDER_LATE)
{
    register_ireq_handler(PANIC_HALT_IREQ, &panic_halt_handler, nullptr);
}
