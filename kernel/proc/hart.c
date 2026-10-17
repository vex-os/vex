#include <vex/atomic.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/panic.h>
#include <vex/pmap.h>
#include <vex/sched.h>
#include <vex/thread.h>
#include <vex/tlb.h>

struct hart_local harts[MAX_HARTS];
unsigned long num_harts = 1U;

static unsigned long ignored_harts = 0U;
static volatile unsigned long ready_harts = 0U;

static void bring_online(struct hart_local* restrict hart)
{
    hart_setup(hart);

    invoke_initcalls(initcalls_smp_start, initcalls_smp_end);

    tlb_hart_online();

    hart->flags |= HART_FLAG_ONLINE;

    atomic_fetch_add(&ready_harts, 1, ATOMIC_RELEASE);
}

struct hart_local* this_hart(void)
{
    struct thread* thrd = this_thread();

    return thrd->hart;
}

struct hart_local* hart_register(bool is_boot)
{
    struct hart_local* hart;

    if(is_boot) {
        hart = &harts[0];
    }
    else if(num_harts < MAX_HARTS) {
        hart = &harts[num_harts];
        num_harts += 1;
    }
    else {
        ignored_harts += 1;
        return nullptr;
    }

    hart->index = (unsigned short)(hart - harts);

    hart->flags = HART_FLAG_PRESENT | HART_FLAG_ACTIVE;

    if(is_boot) {
        hart->flags |= HART_FLAG_BSP;
    }

    return hart;
}

void hart_entry(struct hart_local* restrict hart)
{
    pmap_switch(&sys_pmap);

    bring_online(hart);

    enable_interrupts();

    for(;;) {
        hart_stop();
    }
}

static void init_hart_structs(void)
{
    arch_enumerate_harts();

    if(0 == (harts[0].flags & HART_FLAG_PRESENT)) {
        panic("hart: firmware did not report the boot hart");
    }

    if(ignored_harts) {
#ifdef CONFIG_SMP
        pr_warning("hart: %lu harts past CONFIG_MAX_HARTS=%u left unused", ignored_harts, MAX_HARTS);
#else
        pr_inform("hart: SMP is disabled, %lu harts left unused", ignored_harts);
#endif
    }
}

static void init_hart_boot(void)
{
    bring_online(boot_hart());

    for(unsigned long i = 1U; i < num_harts; ++i) {
        arch_start_hart(&harts[i]);
    }

    while(atomic_load_n(&ready_harts, ATOMIC_ACQUIRE) < num_harts) {
        hart_spin();
    }

    pr_verbose("%lu harts online", num_harts);
}

initcall_bsp_multicore(init_hart_structs, INIT_EARLY);
initcall_bsp_multicore(init_hart_boot, INIT_LAST);
