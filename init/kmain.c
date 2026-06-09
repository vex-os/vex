#include <limine.h>
#include <vex.arch/hart.h>
#include <vex.arch/intr.h>
#include <vex/delay.h>
#include <vex/hhdm.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/process.h>
#include <vex/pslab.h>
#include <vex/sched.h>
#include <vex/signal.h>
#include <vex/syscall.h>
#include <vex/thread.h>
#include <vex/version.h>
#include <vex/vmm.h>
#include <vex/vprot.h>

#define UCODE_VIRT    0x400000UL
#define USTACK_VIRT   0x800000UL
#define UCODE2_VIRT   0x500000UL
#define UHANDLER_VIRT 0x600000UL
#define USTACK2_VIRT  0x900000UL

static __attribute__((naked, aligned(PAGE_SIZE))) void user_test_fn(void)
{
    asm volatile("mov $42, %rax\n"
                 "mov $1000, %rdi\n"
                 "mov $0xCAFEBABE, %rsi\n"
                 "mov $0x1337C0DE, %rdx\n"
                 "mov $0xBAADF00D, %r10\n"
                 "mov $0x0DEFACED, %r8\n"
                 "mov $0xFEEDFACE, %r9\n"
                 "syscall\n"
                 "xor %rdi, %rdi\n"
                 "mov $1, %rax\n"
                 "syscall\n");
}

static __attribute__((naked, aligned(PAGE_SIZE))) void user_test_fn2(void)
{
    asm volatile("mov $42, %rax\n"
                 "xor %rdi, %rdi\n"
                 "syscall\n"
                 "ret\n");
}

static __attribute__((naked, aligned(PAGE_SIZE))) void user_sigsegv_handler(void)
{
    asm volatile( // "mov $1337, %rdi\n"
                  // "mov $1, %rax\n"
                  // "syscall\n"
        "ret\n");
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
    *(uintptr_t*)phys_to_virt(stack_phys + PAGE_SIZE - 8) = 0;

    if(process_spawn_thread(proc, UCODE_VIRT + fn_offset, USTACK_VIRT + PAGE_SIZE - 8) == nullptr)
        panic("kmain: user thread alloc failed");

    struct process* proc2 = process_create();
    if(proc2 == nullptr)
        panic("kmain: process2 alloc failed");

    uintptr_t fn2_virt = (uintptr_t)user_test_fn2;
    uintptr_t code2_phys = vmm_resolve(&sys_pagemap, fn2_virt);
    uintptr_t fn2_offset = fn2_virt & (PAGE_SIZE - 1);
    vmm_map(proc2->vm, UCODE2_VIRT, code2_phys, VPROT_URX);

    uintptr_t handler_virt = (uintptr_t)user_sigsegv_handler;
    uintptr_t handler_phys = vmm_resolve(&sys_pagemap, handler_virt);
    uintptr_t handler_offset = handler_virt & (PAGE_SIZE - 1);
    vmm_map(proc2->vm, UHANDLER_VIRT, handler_phys, VPROT_URX);

    proc2->sig_actions[SIGSEGV].sa_handler = (sa_handler_t)(UHANDLER_VIRT + handler_offset);

    uintptr_t stack2_phys = pslab_alloc();
    if(stack2_phys == 0)
        panic("kmain: user stack2 alloc failed");
    vmm_map(proc2->vm, USTACK2_VIRT, stack2_phys, VPROT_URW);
    *(uintptr_t*)phys_to_virt(stack2_phys + PAGE_SIZE - 8) = 0;

    if(process_spawn_thread(proc2, UCODE2_VIRT + fn2_offset, USTACK2_VIRT + PAGE_SIZE - 8) == nullptr)
        panic("kmain: user thread2 alloc failed");

    enable_interrupts();

    pr_inform("user_test_fn  virt=%p mapped=%p", (void*)(UCODE_VIRT + fn_offset), (void*)fn_virt);
    pr_inform("user_test_fn2 virt=%p mapped=%p", (void*)(UCODE2_VIRT + fn2_offset), (void*)fn2_virt);
    pr_emerg("hanging around...");

    for(;;) {
        sched_idle();
    }

    unreachable();
}
