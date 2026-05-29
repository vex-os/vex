// SPDX-License-Identifier: BSD-3-Clause

#include <arch/hart.h>
#include <arch/interrupts.h>
#include <core/atomic.h>
#include <core/cdefs.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <init/initfn.h>
#include <limine.h>

static volatile __used __section(".limine_requests") struct limine_mp_request mp_request = {
    .id = LIMINE_MP_REQUEST_ID,
    .revision = 0,
};

static void __noreturn mp_entry(struct limine_mp_info* info)
{
    UNUSED_ARG(info);

    call_initfns(initfns_mp_start, initfns_mp_end);

    enable_interrupts();

    for(;;) {
        stop_hart();
    }

    unreachable();
}

static int init_mp(void)
{
    struct limine_mp_response* response = mp_request.response;
    uint64_t i;

    if(response == nullptr) {
        return ENODEV;
    }

    call_initfns(initfns_mp_start, initfns_mp_end);

    for(i = 0; i < response->cpu_count; ++i) {
        atomic_store_n(&response->cpus[i]->goto_address, &mp_entry, ATOMIC_RELEASE);
    }

    return 0;
}

DEFINE_INITFN_BSP(init_mp, INITFN_STAGE_BSP_MP, INITFN_ORDER_FIRST);
