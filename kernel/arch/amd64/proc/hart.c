#include <arch/amd64.lapic.h>
#include <arch/amd64.msr.h>
#include <arch/irq.h>
#include <limine.h>
#include <vex/assert.h>
#include <vex/atomic.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/panic.h>
#include <vex/thread.h>

LIMINE_REQUEST static volatile struct limine_mp_request mp_request = {
    .id = LIMINE_MP_REQUEST_ID,
    .revision = 0,
};

static struct limine_mp_info* mp_infos[MAX_HARTS];
static thread_t boot_thread; // this_thread returns this before true threading initializes

static void __noreturn limine_hart_entry(struct limine_mp_info* restrict info)
{
    hart_park((hart_local_t*)(uintptr_t)(info->extra_argument));
}

void arch_enumerate_harts(void)
{
    const struct limine_mp_response* response = mp_request.response;

    if(response == nullptr) {
        panic("bootloader did not provide multicore information");
    }

    for(uint64_t i = 0; i < response->cpu_count; ++i) {
        struct limine_mp_info* info = response->cpus[i];
        hart_local_t* hart = hart_register(info->lapic_id == response->bsp_lapic_id);

        if(hart) {
            hart->arch.lapic_id = info->lapic_id;
            mp_infos[hart->index] = info;
        }
    }
}

void arch_start_hart(hart_local_t* restrict hart)
{
    struct limine_mp_info* info = mp_infos[hart->index];

    info->extra_argument = (uintptr_t)(hart);
    atomic_store_n(&info->goto_address, &limine_hart_entry, ATOMIC_RELEASE);
}

void hart_setup(hart_local_t* restrict hart)
{
    wrmsr(MSR_GS_BASE, (uintptr_t)(hart->idle_thread));
    wrmsr(MSR_KERNEL_GS_BASE, (uintptr_t)(hart->idle_thread->arch.gs_base));
}

void hart_ipi(hart_local_t* restrict target, unsigned irq_number)
{
    kassert(target);

    if(target == HART_IPI_EVERYONE) {
        lapic_ipi(0, LAPIC_IPI_EVERYONE, irq_number + ARCH_IRQ_BASE);
        return;
    }

    if(target == HART_IPI_EXCEPT_ME) {
        lapic_ipi(0, LAPIC_IPI_EXCEPT_ME, irq_number + ARCH_IRQ_BASE);
        return;
    }

    lapic_ipi(target->arch.lapic_id, LAPIC_IPI_SPECIFIC, irq_number + ARCH_IRQ_BASE);
}

static void arch_init_boot_thread(void)
{
    boot_thread.self = &boot_thread;
    wrmsr(MSR_GS_BASE, (uintptr_t)(&boot_thread));
}

initcall_bsp_early(arch_init_boot_thread, INIT_FIRST);
