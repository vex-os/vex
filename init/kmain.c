#include <limine.h>
#include <vex/clockevt.h>
#include <vex/delay.h>
#include <vex/event.h>
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

// Unlike sched_test_thread above (which always calls sched_yield() well
// within its timeslice, so sched_ireq_handler is only ever reached via a
// self-IPI), tick_producer never yields voluntarily: delay_msec() just
// busy-spins. The only way it ever gives up the hart is the LAPIC oneshot
// timer armed for its timeslice actually expiring, which is the one
// preemption path nothing else here exercises. tick_consumer proves the
// other half - blocking on and being woken by an event.
static struct event tick_event;

static void tick_producer(void* restrict arg)
{
    (void)(arg);

    for(unsigned long i = 0; i < 6; ++i) {
        delay_msec(1000);
        event_trigger(&tick_event, true);
    }

    sched_terminate();
    unreachable();
}

static void tick_consumer(void* restrict arg)
{
    (void)(arg);

    for(unsigned long i = 0; i < 5; ++i) {
        struct event* events[] = { &tick_event };
        event_await_blocking(events, 1);

        pr_inform("[%hu] tick_consumer: tick %lu", this_hart()->index, i);
    }

    sched_terminate();
    unreachable();
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

    create_thread_kern_enq(tick_producer, nullptr);
    create_thread_kern_enq(tick_consumer, nullptr);

    pr_emerg("sched test threads spawned, enabling interrupts");

    enable_interrupts();

    for(;;) {
        hart_spin();
    }

    unreachable();
}
