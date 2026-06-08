#include <limine.h>
#include <sys.arch/hart.h>
#include <sys.arch/intr.h>
#include <sys/delay.h>
#include <sys/initcall.h>
#include <sys/kprintf.h>
#include <sys/panic.h>
#include <sys/sched.h>
#include <sys/version.h>

static volatile __used __section(".limine_requests") uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);
static volatile __used __section(".limine_requests_start") uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;
static volatile __used __section(".limine_requests_end") uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static int16_t test_task(void* restrict ctxdata)
{
    int count = 0;

    for(;;) {
        pr_inform("TICK %d", count);
        sched_sleep_msec(500);

        if(++count >= 5) {
            return 42;
        }
    }

    unreachable();
}

void __noreturn kmain(void)
{
    pr_emerg("%s %s %s", k_sysname, k_release, k_version);

    invoke_initcalls(initcalls_bsp_start, initcalls_bsp_end);

    struct kthread* task = kthread_create(&test_task, nullptr);
    if(task == nullptr)
        panic("kmain: test task alloc failed");
    kthread_enqueue(task);

    enable_interrupts();

    pr_emerg("hanging around...");

    for(;;) {
        stop_hart();
    }

    unreachable();
}
