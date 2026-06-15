#include <limine.h>
#include <vex/hart.h>
#include <vex/initcall.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/panic.h>
#include <vex/sched.h>
#include <vex/thread.h>
#include <vex/vmm.h>

LIMINE_REQUEST static volatile struct limine_mp_request mp_request = {
    .id = LIMINE_MP_REQUEST_ID,
    .revision = 0,
};

struct hart_local* harts;
unsigned long num_harts;

static volatile unsigned long ready_harts;

#ifdef ENABLE_SMP
static void __noreturn smp_entry(struct limine_mp_info* restrict info)
{
    vmm_switch(&sys_pagemap);

    hart_setup_2(info);

    invoke_initcalls(initcalls_smp_start, initcalls_smp_end);

    // if(this_hart()->index == 1) {
    //     panic("test panic from hart %u", this_hart()->index);
    // }

    atomic_fetch_add(&ready_harts, 1, ATOMIC_RELEASE);

    enable_interrupts();

    for(;;) {
        hart_stop();
    }
}
#endif

struct hart_local* this_hart(void)
{
    struct thread* thrd = this_thread();

    return thrd->hart;
}

static void init_hart_structs(void)
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
        hart_setup_1(mp_request.response->cpus[i], mp_request.response);
    }
}

static void init_hart_boot(void)
{
    for(unsigned long i = 0; i < num_harts; ++i) {
        if(harts[i].flags & HART_FLAG_BSP) {
            hart_setup_2(mp_request.response->cpus[i]);
            break;
        }
    }

    invoke_initcalls(initcalls_smp_start, initcalls_smp_end);

    atomic_store_n(&ready_harts, 1, ATOMIC_RELAXED);

#ifdef ENABLE_SMP
    for(unsigned long i = 0; i < num_harts; ++i) {
        atomic_store_n(&mp_request.response->cpus[i]->goto_address, &smp_entry, ATOMIC_RELEASE);
    }

    while(atomic_load_n(&ready_harts, ATOMIC_ACQUIRE) < num_harts) {
        hart_spin();
    }
#endif

    pr_inform("hart: %lu harts online", num_harts);
}

initcall_bsp_multicore(init_hart_structs, INIT_EARLY);
initcall_bsp_multicore(init_hart_boot, INIT_LAST);
