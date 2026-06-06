#include <limine.h>
#include <sys.arch/hart.h>
#include <sys.arch/intr.h>
#include <sys/atomic.h>
#include <sys/cdefs.h>
#include <sys/errno.h>
#include <sys/initcall.h>
#include <sys/kprintf.h>
#include <sys/panic.h>
#include <sys/vmm.h>

static volatile __used __section(".limine_requests") struct limine_mp_request mp_request = {
    .id = LIMINE_MP_REQUEST_ID,
    .revision = 0,
};

static uint64_t hart_count = 0;
static volatile uint64_t ready_count = 0;

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
        stop_hart();
    }
}

define_initcall(smp, INITCALL_STAGE_BSP_MULTICORE, INITCALL_ORDER_LAST)
{
    if(mp_request.response == nullptr) {
        panic("bootloader did not provide multicore information");
    }

    hart_count = mp_request.response->cpu_count;

    invoke_initcalls(initcalls_smp_start, initcalls_smp_end);

    atomic_store_n(&ready_count, UINT64_C(1), ATOMIC_RELAXED);

    for(uint64_t i = 0; i < hart_count; ++i) {
        atomic_store_n(&mp_request.response->cpus[i]->goto_address, &smp_entry, ATOMIC_RELEASE);
    }

    enable_interrupts();

    while(atomic_load_n(&ready_count, ATOMIC_ACQUIRE) < hart_count) {
        yield_hart();
    }

    pr_inform("smp: %llu harts online", (unsigned long long)(hart_count));
}
