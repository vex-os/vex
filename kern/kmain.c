#include <limine.h>
#include <sys.arch/hart.h>
#include <sys.arch/intr.h>
#include <sys/delay.h>
#include <sys/hhdm.h>
#include <sys/initcall.h>
#include <sys/kprintf.h>
#include <sys/page.h>
#include <sys/panic.h>
#include <sys/pslab.h>
#include <sys/sched.h>
#include <sys/syscall.h>
#include <sys/thread.h>
#include <sys/version.h>
#include <sys/vmm.h>
#include <sys/vprot.h>

static volatile __used __section(".limine_requests") uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);
static volatile __used __section(".limine_requests_start") uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;
static volatile __used __section(".limine_requests_end") uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static intptr_t test_task(void* restrict ctxdata)
{
    int count = 0;

    for(;;) {
        pr_inform("TICK %d", count);
        sched_msleep(500);

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

    struct thread* thrd = thread_create_kern(nullptr, &test_task, nullptr);
    if(thrd == nullptr)
        panic("kmain: test thread alloc failed");
    sched_enqueue(thrd);

    enable_interrupts();

    pr_emerg("hanging around...");

    for(;;) {
        sched_idle();
    }

    unreachable();
}
