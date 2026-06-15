#include <limine.h>
#include <vex/clockevt.h>
#include <vex/delay.h>
#include <vex/hart.h>
#include <vex/hhdm.h>
#include <vex/initcall.h>
#include <vex/ireq.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/pslab.h>
#include <vex/version.h>
#include <vex/vmm.h>
#include <vex/vprot.h>

static volatile __used __section(".limine_requests") uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);
static volatile __used __section(".limine_requests_start") uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;
static volatile __used __section(".limine_requests_end") uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static void timer_ireq(unsigned number, const struct hart_context* restrict ctx, void* restrict ctxdata)
{
    pr_inform("timer interrupt");
}

void __noreturn kmain(void)
{
    pr_emerg("%s %s %s", k_sysname, k_release, k_version);

    invoke_initcalls(initcalls_bsp_start, initcalls_bsp_end);

    pr_emerg("hanging around...");

    unsigned ireq = add_ireq_handler(UINT_MAX, &timer_ireq, NULL);

    enable_interrupts();

    clockevt_periodic(ireq, 500); // 500ms

    for(;;) {
        hart_spin();
    }

    unreachable();
}
