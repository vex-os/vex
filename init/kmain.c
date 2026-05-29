#include <arch/hart.h>
#include <arch/interrupts.h>
#include <core/cdefs.h>
#include <core/kprintf.h>
#include <core/version.h>
#include <init/initcall.h>
#include <limine.h>

static volatile __used __section(".limine_requests") uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);
static volatile __used __section(".limine_requests_start") uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;
static volatile __used __section(".limine_requests_end") uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

void __noreturn kmain(void)
{
    pr_inform("%s %s %s", k_sysname, k_release, k_version);

    invoke_initcalls(initcalls_bsp_start, initcalls_bsp_end);

    enable_interrupts();

    pr_inform("hanging around...");

    for(;;) {
        stop_hart();
    }

    unreachable();
}
