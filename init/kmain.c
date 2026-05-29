// SPDX-License-Identifier: BSD-3-Clause

#include <arch/hart.h>
#include <arch/interrupts.h>
#include <core/cdefs.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <core/mp.h>
#include <core/panic.h>
#include <core/version.h>
#include <init/initfn.h>
#include <lib/string.h>
#include <limine.h>

static volatile __used __section(".limine_requests") uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);
static volatile __used __section(".limine_requests_start") uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;
static volatile __used __section(".limine_requests_end") uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

void call_mp_initfns(void)
{
    const struct initfn_entry* it;
    int error;

    for(it = initfns_mp_start; it < initfns_mp_end; ++it) {
        kprintf(KP_DEBUG, "kmain: calling mp initfn: %s <%p>", it->name, it->pfn);

        error = it->pfn();

        if(error == 0 || error == ENODEV) {
            continue;
        }

        panic("kmain: mp initfn: %s: %s", it->name, strerror(error));
        unreachable();
    }
}

static void call_initfns(const struct initfn_entry* start, const struct initfn_entry* end)
{
    const struct initfn_entry* it;
    int error;

    for(it = start; it < end; ++it) {
        kprintf(KP_DEBUG, "kmain: calling initfn: %s <%p>", it->name, it->pfn);

        error = it->pfn();

        if(error == 0 || error == ENODEV) {
            continue;
        }

        panic("kmain: initfn: %s: %s", it->name, strerror(error));
        unreachable();
    }
}

void __noreturn kmain(void)
{
    kprintf(KP_INFORM, "%s %s %s", k_sysname, k_release, k_version);

    call_initfns(initfns_bsp_start, initfns_bsp_end);

    enable_interrupts();

    kprintf(KP_INFORM, "hanging around...");

    for(;;) {
        stop_hart();
    }

    unreachable();
}
