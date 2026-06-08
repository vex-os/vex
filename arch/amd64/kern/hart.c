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

#define MSR_EFER    0xC0000080
#define MSR_STAR    0xC0000081
#define MSR_LSTAR   0xC0000082
#define MSR_SFMASK  0xC0000084
#define MSR_GS_BASE 0xC0000101

#define EFER_SCE  (1 << 0)
#define RFLAGS_IF (1 << 9)

#define STAR_KERN_CS   (uint64_t)(GDT_KERN_CODE_64 << 3)
#define STAR_USER_BASE (uint64_t)(GDT_KERN_DATA_64 << 3)

extern void amd64_syscall_entry(void);

static unsigned hart_count = 0;
static unsigned panic_ipi_vector = 0;

static void panic_halt_handler(unsigned number, const struct interrupt_frame* restrict frame, void* restrict ctxdata)
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
        panic("hart[%hd]: out of memory", hart->index);
    }

    hart->self = hart;
    hart->index = atomic_fetch_add(&hart_count, 1, ATOMIC_RELAXED);

    wrmsr(MSR_GS_BASE, (uint64_t)(hart));

    void* exit_ksp = pslab_alloc_virt();

    if(exit_ksp == nullptr) {
        panic("hart[%hd]: out of memory", hart->index);
    }

    hart->amd64_exit_ksp = (uintptr_t)(exit_ksp);
}

initcall_smp_hart(arch_init_hart, INIT_FIRST);

static void arch_init_syscall(void)
{
    wrmsr(MSR_EFER, (EFER_SCE | rdmsr(MSR_EFER)));
    wrmsr(MSR_STAR, (STAR_USER_BASE << 48) | (STAR_KERN_CS << 32));
    wrmsr(MSR_LSTAR, (uint64_t)(&amd64_syscall_entry));
    wrmsr(MSR_SFMASK, RFLAGS_IF);
}

initcall_smp_syscall(arch_init_syscall, INIT_FIRST);
