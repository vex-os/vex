#include <limits.h>
#include <string.h>
#include <sys.amd64/lapic.h>
#include <sys.amd64/msr.h>
#include <sys.arch/hart.h>
#include <sys.arch/intr.h>
#include <sys.arch/ireq.h>
#include <sys/atomic.h>
#include <sys/errno.h>
#include <sys/initcall.h>
#include <sys/ireq.h>
#include <sys/kprintf.h>
#include <sys/malloc.h>
#include <sys/panic.h>

#define MSR_GS_BASE 0xC0000101

static unsigned hart_count = 0;
static unsigned panic_ipi_vector = 0;

static void panic_halt_handler(unsigned number, struct hart_regs* restrict regs, void* restrict ctxdata)
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
