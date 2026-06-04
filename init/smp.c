#include <arch/hart.h>
#include <arch/interrupts.h>
#include <core/atomic.h>
#include <core/cdefs.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <core/panic.h>
#include <init/initcall.h>
#include <limine.h>
#include <mm/vmm.h>

static volatile __used __section(".limine_requests") struct limine_mp_request mp_request = {
    .id = LIMINE_MP_REQUEST_ID,
    .revision = 0,
};

static uint64_t smp_cpu_count = 0;
static volatile uint64_t smp_ready_count = 0;

static void __noreturn smp_entry(struct limine_mp_info* restrict info)
{
    vmm_switch(&sys_pagemap);

    invoke_initcalls(initcalls_smp_start, initcalls_smp_end);

    pr_debug("smp: hart %u ready", this_hart()->index);

    // if(this_hart()->index == 1) {
    //     panic("test panic from hart %u", this_hart()->index);
    //     unreachable();
    // }

    atomic_fetch_add(&smp_ready_count, UINT64_C(1), ATOMIC_RELEASE);

    enable_interrupts();

    for(;;) {
        stop_hart();
    }

    unreachable();
}

define_initcall(smp, INITCALL_STAGE_BSP_MULTICORE, INITCALL_ORDER_LAST)
{
    if(mp_request.response == nullptr) {
        panic("bootloader did not provide multicore information");
        unreachable();
    }

    smp_cpu_count = mp_request.response->cpu_count;

    invoke_initcalls(initcalls_smp_start, initcalls_smp_end);

    pr_debug("smp: hart %u ready", this_hart()->index);

    atomic_store_n(&smp_ready_count, UINT64_C(1), ATOMIC_RELAXED);

    for(uint64_t i = 0; i < smp_cpu_count; ++i) {
        atomic_store_n(&mp_request.response->cpus[i]->goto_address, &smp_entry, ATOMIC_RELEASE);
    }

    enable_interrupts();

    while(atomic_load_n(&smp_ready_count, ATOMIC_ACQUIRE) < smp_cpu_count) {
        yield_hart();
    }
}
