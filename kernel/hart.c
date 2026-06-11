#include <limine.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/panic.h>
#include <vex/vmm.h>

LIMINE_REQUEST static volatile struct limine_mp_request mp_request = {
    .id = LIMINE_MP_REQUEST_ID,
    .revision = 0,
};

struct hart_local* harts;
unsigned long num_harts;

#ifdef ENABLE_SMP
static volatile unsigned long ready_count;
#endif

#ifdef ENABLE_SMP
static void __noreturn smp_entry(struct limine_mp_info* restrict info)
{
    vmm_switch(&sys_pagemap);

    struct hart_local* hart = harts[info->processor_id];

    struct hart_local* hart = this_hart();
    hart->flags &= ~HART_FLAG_BSP;

    invoke_initcalls(initcalls_smp_start, initcalls_smp_end);

    // if(this_hart()->index == 1) {
    //     panic("test panic from hart %u", this_hart()->index);
    // }

    atomic_fetch_add(&ready_count, 1, ATOMIC_RELEASE);

    enable_interrupts();

    for(;;) {
        hart_stop();
    }
}
#endif

static void init_hart(void)
{
    if(mp_request.response == nullptr) {
        panic("bootloader did not provide multicore information");
    }

    num_harts = mp_request.response->cpu_count;
    harts = kmalloc(num_harts * sizeof(struct hart_local));

    if(harts == nullptr) {
        panic("hart: out of memory");
    }

    for(unsigned long i = 0; i < num_harts; ++i) {
        harts[i].index = i;
        harts[i].flags = HART_FLAG_BSP; // this is cleared later by non-bsp harts
    }

    invoke_initcalls(initcalls_smp_start, initcalls_smp_end);

#ifdef ENABLE_SMP
    atomic_store_n(&ready_count, 1, ATOMIC_RELAXED);

    for(unsigned long i = 0; i < num_harts; ++i) {
        atomic_store_n(&mp_request.response->cpus[i]->goto_address, &smp_entry, ATOMIC_RELEASE);
    }

    while(atomic_load_n(&ready_count, ATOMIC_ACQUIRE) < num_harts) {
        hart_spin();
    }
#endif

    pr_inform("hart: %lu harts online", num_harts);
}

initcall_bsp_multicore(init_hart, INIT_EARLY);
