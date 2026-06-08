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
#include <sys/thread.h>
#include <sys/version.h>
#include <sys/vmm.h>
#include <sys/vprot.h>

#define UCODE_VIRT  0x400000UL
#define USTACK_VIRT 0x800000UL

static __attribute__((naked)) void user_test_fn(void)
{
    asm volatile("mov $42, %rax\n"         // SYS_TEST
                 "mov $0xDEADBEEF, %rdi\n" // arg_0
                 "mov $0xCAFEBABE, %rsi\n" // arg_1
                 "mov $0x1337C0DE, %rdx\n" // arg_2
                 "mov $0xBAADF00D, %r10\n" // arg_3
                 "mov $0x0DEFACED, %r8\n"  // arg_4
                 "mov $0xFEEDFACE, %r9\n"  // arg_5
                 "syscall\n"
                 "xor %rdi, %rdi\n"
                 "mov $1, %rax\n" // SYS_EXIT
                 "syscall\n");
}

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

    struct process* proc = process_create();
    if(proc == nullptr)
        panic("kmain: process alloc failed");

    uintptr_t fn_virt = (uintptr_t)user_test_fn;
    uintptr_t code_phys = vmm_resolve(&sys_pagemap, fn_virt);
    uintptr_t fn_offset = fn_virt & (PAGE_SIZE - 1);
    vmm_map(proc->vm, UCODE_VIRT, code_phys, VPROT_URX);

    uintptr_t stack_phys = pslab_alloc();
    if(stack_phys == 0)
        panic("kmain: user stack alloc failed");
    vmm_map(proc->vm, USTACK_VIRT, stack_phys, VPROT_URW);

    if(process_spawn_thread(proc, UCODE_VIRT + fn_offset, USTACK_VIRT + PAGE_SIZE) == nullptr)
        panic("kmain: user thread alloc failed");

    enable_interrupts();

    pr_emerg("hanging around...");

    for(;;) {
        sched_idle();
    }

    unreachable();
}
