#include <vex.arch/hart.h>
#include <vex.arch/intr.h>
#include <vex.arch/ireq.h>
#include <vex.x86/gdt.h>
#include <vex.x86/lapic.h>
#include <vex.x86/msr.h>
#include <vex/atomic.h>
#include <vex/errno.h>
#include <vex/hhdm.h>
#include <vex/initcall.h>
#include <vex/ireq.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/limits.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/pslab.h>
#include <vex/string.h>

static unsigned hart_count = 0;
static unsigned panic_ipi_vector = 0;

static void panic_halt_handler(unsigned number, const struct regfile* restrict regs, void* restrict ctxdata)
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

static void arch_init_panic_ipi(void)
{
    panic_ipi_vector = add_ireq_handler(UINT_MAX, &panic_halt_handler, nullptr);
    panic_ipi_vector += ARCH_IREQ_BASE;
}

initcall_bsp_multicore(arch_init_panic_ipi, INIT_MIDDLE);

static void arch_init_hart(void)
{
    struct hart_local* hart = kmalloc(sizeof(struct hart_local));

    if(hart == nullptr) {
        panic("hart: out of memory");
    }

    hart->self = hart;
    hart->index = atomic_fetch_add(&hart_count, 1, ATOMIC_RELAXED);

    wrmsr(MSR_IA32_GS_BASE, (uintptr_t)(hart));
}

initcall_smp_hart(arch_init_hart, INIT_FIRST);
