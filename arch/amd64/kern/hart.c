#include <limits.h>
#include <string.h>
#include <sys.amd64/gdt.h>
#include <sys.amd64/lapic.h>
#include <sys.amd64/msr.h>
#include <sys.arch/hart.h>
#include <sys.arch/intr.h>
#include <sys.arch/ireq.h>
#include <sys/atomic.h>
#include <sys/errno.h>
#include <sys/hhdm.h>
#include <sys/initcall.h>
#include <sys/ireq.h>
#include <sys/kprintf.h>
#include <sys/malloc.h>
#include <sys/page.h>
#include <sys/panic.h>
#include <sys/pslab.h>

static unsigned hart_count = 0;
static unsigned panic_ipi_vector = 0;

extern void amd64_syscall_entry(void);

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

    // enable SYSCALL/SYSRET (SCE bit in EFER)
    wrmsr(0xC0000080, rdmsr(0xC0000080) | 1);

    // STAR[47:32] = kern CS (syscall loads this into CS, CS+8 into SS)
    // STAR[63:48] = base for sysret: CPU sets CS = base+16|3, SS = base+8|3
    //   with our GDT: base=0x30 → SS=0x3B (user data), CS=0x43 (user code)
    uint64_t star = UINT64_C(0);
    star |= ((uint64_t)(gdt_selector(GDT_KERN_CODE_64, 0, 0)) << 32);
    star |= ((uint64_t)(gdt_selector(GDT_USER_DATA_64, 0, 0) - 8) << 48);

    wrmsr(0xC0000101, (uintptr_t)(hart));                 // IA32_GS_BASE
    wrmsr(0xC0000081, star);                              // IA32_STAR
    wrmsr(0xC0000082, (uintptr_t)(&amd64_syscall_entry)); // IA32_LSTAR
    wrmsr(0xC0000084, (UINT64_C(1) << 9));                // IA32_SFMASK: гасим IF
}

initcall_smp_hart(arch_init_hart, INIT_FIRST);
