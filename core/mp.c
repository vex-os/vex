// SPDX-License-Identifier: BSD-3-Clause

#include <arch/hart.h>
#include <arch/interrupts.h>
#include <core/cdefs.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <core/mp.h>
#include <init/initfn.h>
#include <limine.h>

static volatile __used __section(".limine_requests") struct limine_mp_request mp_request = {
    .id = LIMINE_MP_REQUEST_ID,
    .revision = 0,
};

static void __noreturn mp_entry(struct limine_mp_info* info)
{
    UNUSED_ARG(info);

    call_mp_initfns();
    enable_interrupts();

    for(;;) {
        stop_hart();
    }

    unreachable();
}

static int init_mp(void)
{
    struct limine_mp_response* response = mp_request.response;
    struct limine_mp_info* info;
    uint64_t i;

    if(response == nullptr) {
        return ENODEV;
    }

    call_mp_initfns();

    for(i = 0; i < response->cpu_count; ++i) {
        info = response->cpus[i];

        if(info->lapic_id == response->bsp_lapic_id) {
            continue;
        }

        __atomic_store_n(&info->goto_address, mp_entry, __ATOMIC_RELEASE);
    }

    return 0;
}

DEFINE_INITFN_BSP(init_mp, INITFN_STAGE_BSP_MP, INITFN_ORDER_FIRST);
