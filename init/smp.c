#include <limine.h>
#include <vex.arch/hart.h>
#include <vex/atomic.h>
#include <vex/compiler.h>
#include <vex/errno.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/panic.h>
#include <vex/vmm.h>

static volatile __used __section(".limine_requests") struct limine_mp_request mp_request = {
    .id = LIMINE_MP_REQUEST_ID,
    .revision = 0,
};

#if ENABLE_SMP
static uint64_t hart_count = 0;
static volatile uint64_t ready_count = 0;
#endif

#if ENABLE_SMP
static void __noreturn smp_entry(struct limine_mp_info* restrict info)
{
    vmm_switch(&sys_pagemap);

    invoke_initcalls(initcalls_smp_start, initcalls_smp_end);

    // if(this_hart()->index == 1) {
    //     panic("test panic from hart %u", this_hart()->index);
    // }

    atomic_fetch_add(&ready_count, UINT64_C(1), ATOMIC_RELEASE);

    enable_interrupts();

    for(;;) {
        hart_stop();
    }
}
#endif

static void init_smp(void)
{
    if(mp_request.response == nullptr) {
        panic("bootloader did not provide multicore information");
    }

#if ENABLE_SMP
    hart_count = mp_request.response->cpu_count;
#endif

    invoke_initcalls(initcalls_smp_start, initcalls_smp_end);

#if ENABLE_SMP
    atomic_store_n(&ready_count, UINT64_C(1), ATOMIC_RELAXED);

    for(uint64_t i = 0; i < hart_count; ++i) {
        atomic_store_n(&mp_request.response->cpus[i]->goto_address, &smp_entry, ATOMIC_RELEASE);
    }

    while(atomic_load_n(&ready_count, ATOMIC_ACQUIRE) < hart_count) {
        hart_spin();
    }

    pr_inform("smp: %llu harts online", (unsigned long long)(hart_count));
#endif
}

initcall_bsp_multicore(init_smp, INIT_LAST);
