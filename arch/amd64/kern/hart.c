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
        panic("out of memory");
    }

    hart->self = hart;
    hart->index = atomic_fetch_add(&hart_count, 1, ATOMIC_RELAXED);

    wrmsr(MSR_GS_BASE, (uint64_t)(hart));
}

initcall_smp_hart(arch_init_hart, INIT_FIRST);
