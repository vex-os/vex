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

static void __noreturn mp_entry(struct limine_mp_info* info)
{
    vmm_switch(&sys_pagemap);

    invoke_initcalls(initcalls_smp_start, initcalls_smp_end);

    pr_inform("smp: hart %u ready", this_hart()->index);

    enable_interrupts();

    for(;;) {
        stop_hart();
    }

    unreachable();
}

define_initcall(smp, INITCALL_STAGE_BSP_MULTICORE, INITCALL_ORDER_FIRST)
{
    uint64_t i;

    if(mp_request.response == nullptr) {
        panic("bootloader did not provide multicore information");
        unreachable();
    }

    invoke_initcalls(initcalls_smp_start, initcalls_smp_end);

    pr_inform("smp: hart %u ready", this_hart()->index);

    for(i = 0; i < mp_request.response->cpu_count; ++i) {
        atomic_store_n(&mp_request.response->cpus[i]->goto_address, &mp_entry, ATOMIC_RELEASE);
    }
}
