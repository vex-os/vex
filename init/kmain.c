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
#include <vex/sched.h>
#include <vex/version.h>
#include <vex/vmm.h>
#include <vex/vprot.h>

static volatile __used __section(".limine_requests") uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);
static volatile __used __section(".limine_requests_start") uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;
static volatile __used __section(".limine_requests_end") uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static void sched_test_thread(void* restrict arg)
{
    const char* name = arg;

    for(unsigned long i = 0;; ++i) {
        if(i > 10) {
            sched_terminate();
            unreachable();
        }

        pr_inform("[%hu] sched_test: %s tick %lu", this_hart()->index, name, i);
        sched_yield();
    }
}

void __noreturn kmain(void)
{
    pr_emerg("%s %s %s", k_sysname, k_release, k_version);

    invoke_initcalls(initcalls_bsp_start, initcalls_bsp_end);

    create_thread_kern_enq(sched_test_thread, (void*)"42");
    create_thread_kern_enq(sched_test_thread, (void*)"67");
    create_thread_kern_enq(sched_test_thread, (void*)"69");
    create_thread_kern_enq(sched_test_thread, (void*)"1337");
    create_thread_kern_enq(sched_test_thread, (void*)"420");

    pr_emerg("sched test threads spawned, enabling interrupts");

    enable_interrupts();

    for(;;) {
        hart_spin();
    }

    unreachable();
}
