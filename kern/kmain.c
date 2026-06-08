#include <limine.h>
#include <sys.arch/hart.h>
#include <sys.arch/intr.h>
#include <sys/delay.h>
#include <sys/hhdm.h>
#include <sys/initcall.h>
#include <sys/kprintf.h>
#include <sys/page.h>
#include <sys/panic.h>
#include <sys/process.h>
#include <sys/pslab.h>
#include <sys/sched.h>
#include <sys/syscall.h>
#include <sys/version.h>
#include <sys/vmm.h>
#include <sys/vprot.h>

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

// test user entry — mapped into user pagemap and jumped to from ring 3
static void __used test_user_entry(void)
{
    register uintptr_t nr asm("rax") = SYS_EXIT;
    register uintptr_t arg asm("rdi") = 1337;
    asm volatile("syscall" ::"r"(nr), "r"(arg) : "rcx", "r11", "memory");

    for(;;)
        pause_hart();
}

static void test_userspace(void)
{
    struct uprocess* proc = uprocess_create(nullptr);
    if(proc == nullptr)
        panic("kmain: test uprocess alloc failed");

    // map test_user_entry (kernel virt) into the user pagemap
    uintptr_t entry_virt = (uintptr_t)test_user_entry;
    if(vmm_patch(proc->vm, entry_virt, VPROT_URX) < 0)
        panic("kmain: failed to map user entry");

    // allocate and map a user stack
    uintptr_t ustack_phys = pslab_alloc();
    if(ustack_phys == 0)
        panic("kmain: failed to alloc user stack");

    uintptr_t ustack_virt = 0x7fff0000;
    if(vmm_map(proc->vm, ustack_virt, ustack_phys, VPROT_URW) < 0)
        panic("kmain: failed to map user stack");

    struct uthread* thread = uthread_create(proc);
    if(thread == nullptr)
        panic("kmain: test uthread alloc failed");

    uthread_set_entry(thread, entry_virt, ustack_virt + PAGE_SIZE);
    uthread_enqueue(thread);

    pr_inform("userspace: enqueued uthread, entry=%p ustack=%p", (void*)entry_virt, (void*)(ustack_virt + PAGE_SIZE));
}

void __noreturn kmain(void)
{
    pr_emerg("%s %s %s", k_sysname, k_release, k_version);

    invoke_initcalls(initcalls_bsp_start, initcalls_bsp_end);

    struct kthread* thread = kthread_create(&test_task, nullptr);
    if(thread == nullptr)
        panic("kmain: test thread alloc failed");
    kthread_enqueue(thread);

    test_userspace();

    enable_interrupts();

    pr_emerg("hanging around...");

    for(;;) {
        sched_hart_idle();
    }

    unreachable();
}
